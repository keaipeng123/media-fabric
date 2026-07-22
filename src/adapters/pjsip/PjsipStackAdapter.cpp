#include "PjsipStackAdapter.h"

#include "ManscdpMessage.h"
#include "SipDef.h"
#include "Logger.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

namespace gb28181 {
namespace {

PjsipStackAdapter* g_adapter = NULL;

std::string pjStrToString(const pj_str_t& value)
{
    if (value.ptr == NULL || value.slen <= 0)
    {
        return "";
    }
    return std::string(value.ptr, value.ptr + value.slen);
}

pj_str_t toPjString(const std::string& value)
{
    return pj_str(const_cast<char*>(value.c_str()));
}

std::string sipUri(const std::string& sipId, const std::string& ip, int port)
{
    std::ostringstream output;
    output << "sip:" << sipId << "@" << ip;
    if (port > 0)
    {
        output << ":" << port;
    }
    return output.str();
}

std::string sipContact(const std::string& sipId, const std::string& ip, int port)
{
    std::ostringstream output;
    output << "<" << sipUri(sipId, ip, port) << ">";
    return output.str();
}

std::string sipUriFromContact(const std::string& contact)
{
    const std::string marker = "sip:";
    std::string::size_type begin = contact.find(marker);
    if (begin == std::string::npos)
    {
        return "";
    }

    std::string::size_type end = contact.find('>', begin);
    if (end == std::string::npos)
    {
        end = contact.find(';', begin);
    }
    if (end == std::string::npos)
    {
        end = contact.find_first_of(" \t\r\n", begin);
    }
    return contact.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

std::string pjStatusText(pj_status_t status)
{
    char buffer[PJ_ERR_MSG_SIZE] = {0};
    pj_strerror(status, buffer, sizeof(buffer));
    return buffer;
}

void logPjFailure(const std::string& action, pj_status_t status)
{
    std::ostringstream detail;
    detail << action << " failed: status=" << status << " message=" << pjStatusText(status);
    media_fabric::Logger::instance().log(media_fabric::LOG_ERROR, "pjsip", detail.str());
}

void ensurePjThreadRegistered()
{
    if (pj_thread_is_registered())
    {
        return;
    }

    static thread_local pj_thread_desc desc;
    static thread_local pj_thread_t* thread = NULL;
    std::memset(desc, 0, sizeof(desc));
    pj_thread_register("pjsip_external", desc, &thread);
}

void setPoolString(pj_pool_t* pool, pj_str_t* target, const std::string& value)
{
    pj_strdup2(pool, target, value.c_str());
}

void initMethod(const std::string& methodName, pjsip_method* method)
{
    if (methodName == "REGISTER")
    {
        pjsip_method_set(method, PJSIP_REGISTER_METHOD);
        return;
    }
    if (methodName == "INVITE")
    {
        pjsip_method_set(method, PJSIP_INVITE_METHOD);
        return;
    }
    if (methodName == "BYE")
    {
        pjsip_method_set(method, PJSIP_BYE_METHOD);
        return;
    }
    if (methodName == "ACK")
    {
        pjsip_method_set(method, PJSIP_ACK_METHOD);
        return;
    }

    pj_str_t name = toPjString(methodName);
    pjsip_method_init_np(method, &name);
}

std::string parseHeaderId(pjsip_msg* msg, pjsip_hdr_e headerType)
{
    pjsip_hdr* header = static_cast<pjsip_hdr*>(pjsip_msg_find_hdr(msg, headerType, NULL));
    if (header == NULL)
    {
        return "";
    }

    char buf[1024] = {0};
    const int size = header->vptr->print_on(header, buf, sizeof(buf));
    if (size <= 0)
    {
        return "";
    }

    std::string value(buf, buf + size);
    const std::string marker = "sip:";
    std::string::size_type begin = value.find(marker);
    if (begin == std::string::npos)
    {
        return "";
    }
    begin += marker.size();
    std::string::size_type end = value.find('@', begin);
    if (end == std::string::npos || end <= begin)
    {
        return "";
    }
    return value.substr(begin, end - begin);
}

std::string headerValue(pjsip_msg* msg, pjsip_hdr_e headerType)
{
    pjsip_hdr* header = static_cast<pjsip_hdr*>(pjsip_msg_find_hdr(msg, headerType, NULL));
    if (header == NULL)
    {
        return "";
    }

    char buf[1024] = {0};
    const int size = header->vptr->print_on(header, buf, sizeof(buf));
    if (size <= 0)
    {
        return "";
    }
    return std::string(buf, buf + size);
}

std::string dialogTag(pjsip_msg* msg, pjsip_hdr_e headerType)
{
    pjsip_fromto_hdr* header = static_cast<pjsip_fromto_hdr*>(pjsip_msg_find_hdr(msg, headerType, NULL));
    if (header == NULL)
    {
        return "";
    }
    return pjStrToString(header->tag);
}

void fillDialogHeaders(pjsip_msg* msg, SipRequestContext* request)
{
    pjsip_cid_hdr* callId = static_cast<pjsip_cid_hdr*>(pjsip_msg_find_hdr(msg, PJSIP_H_CALL_ID, NULL));
    if (callId != NULL)
    {
        request->callId = pjStrToString(callId->id);
    }

    pjsip_cseq_hdr* cseq = static_cast<pjsip_cseq_hdr*>(pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL));
    if (cseq != NULL)
    {
        request->cseq = std::to_string(cseq->cseq);
    }

    request->contact = headerValue(msg, PJSIP_H_CONTACT);
    request->fromTag = dialogTag(msg, PJSIP_H_FROM);
    request->toTag = dialogTag(msg, PJSIP_H_TO);
}

std::string cseqMethod(pjsip_msg* msg)
{
    pjsip_cseq_hdr* cseq = static_cast<pjsip_cseq_hdr*>(pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL));
    if (cseq == NULL)
    {
        return "";
    }
    return pjStrToString(cseq->method.name);
}

void fillDigestAuth(pjsip_msg* msg, SipRequestContext* request)
{
    pjsip_authorization_hdr* auth = static_cast<pjsip_authorization_hdr*>(
        pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL));
    if (auth == NULL)
    {
        return;
    }

    request->digestAuth.username = pjStrToString(auth->credential.digest.username);
    request->digestAuth.realm = pjStrToString(auth->credential.digest.realm);
    request->digestAuth.nonce = pjStrToString(auth->credential.digest.nonce);
    request->digestAuth.uri = pjStrToString(auth->credential.digest.uri);
    request->digestAuth.response = pjStrToString(auth->credential.digest.response);
    request->digestAuth.algorithm = pjStrToString(auth->credential.digest.algorithm);
    request->digestAuth.opaque = pjStrToString(auth->credential.digest.opaque);
    request->digestAuth.qop = pjStrToString(auth->credential.digest.qop);
    request->digestAuth.nc = pjStrToString(auth->credential.digest.nc);
    request->digestAuth.cnonce = pjStrToString(auth->credential.digest.cnonce);
    request->authenticated = !request->digestAuth.response.empty();
}

void fillDigestChallenge(pjsip_msg* msg, SipRequestContext* response)
{
    pjsip_www_authenticate_hdr* auth = static_cast<pjsip_www_authenticate_hdr*>(
        pjsip_msg_find_hdr(msg, PJSIP_H_WWW_AUTHENTICATE, NULL));
    if (auth == NULL)
    {
        return;
    }

    response->digestAuth.realm = pjStrToString(auth->challenge.digest.realm);
    response->digestAuth.nonce = pjStrToString(auth->challenge.digest.nonce);
    response->digestAuth.opaque = pjStrToString(auth->challenge.digest.opaque);
    response->digestAuth.algorithm = pjStrToString(auth->challenge.digest.algorithm);
    response->digestAuth.qop = pjStrToString(auth->challenge.digest.qop);
}

SipRequestContext toRequestContext(pjsip_rx_data* rdata)
{
    SipRequestContext request;
    pjsip_msg* msg = rdata->msg_info.msg;
    if (msg == NULL)
    {
        return request;
    }

    request.method = pjStrToString(msg->line.req.method.name);
    request.fromId = parseHeaderId(msg, PJSIP_H_FROM);
    request.toId = parseHeaderId(msg, PJSIP_H_TO);
    fillDialogHeaders(msg, &request);
    fillDigestAuth(msg, &request);

    pjsip_expires_hdr* expires = (pjsip_expires_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);
    if (expires != NULL)
    {
        request.expires = expires->ivalue;
    }

    if (msg->body != NULL && msg->body->data != NULL && msg->body->len > 0)
    {
        request.body.assign((const char*)msg->body->data, (const char*)msg->body->data + msg->body->len);
    }

    if (request.method == "REGISTER")
    {
        request.event = "request";
    }
    else if (request.method == "INVITE" || request.method == "BYE" || request.method == "ACK")
    {
        request.event = "request";
    }
    else if (request.method == "MESSAGE")
    {
        if (parseManscdpMessage(request.body, &request.manscdp))
        {
            request.event = request.manscdp.event();
        }
    }

    return request;
}

SipRequestContext toResponseContext(pjsip_rx_data* rdata)
{
    SipRequestContext response;
    pjsip_msg* msg = rdata->msg_info.msg;
    if (msg == NULL)
    {
        return response;
    }

    response.method = cseqMethod(msg);
    response.event = "response";
    response.fromId = parseHeaderId(msg, PJSIP_H_FROM);
    response.toId = parseHeaderId(msg, PJSIP_H_TO);
    response.statusCode = msg->line.status.code;
    response.reason = pjStrToString(msg->line.status.reason);
    fillDialogHeaders(msg, &response);
    fillDigestChallenge(msg, &response);

    if (msg->body != NULL && msg->body->data != NULL && msg->body->len > 0)
    {
        response.body.assign((const char*)msg->body->data, (const char*)msg->body->data + msg->body->len);
    }
    if (response.method == "MESSAGE" && parseManscdpMessage(response.body, &response.manscdp))
    {
        response.event = response.manscdp.event();
    }

    return response;
}

pj_bool_t onRxRequest(pjsip_rx_data* rdata)
{
    if (g_adapter == NULL)
    {
        return PJ_FALSE;
    }
    return g_adapter->handleRxRequest(rdata) ? PJ_TRUE : PJ_FALSE;
}

pj_bool_t onRxResponse(pjsip_rx_data* rdata)
{
    if (g_adapter == NULL)
    {
        return PJ_FALSE;
    }
    return g_adapter->handleRxResponse(rdata) ? PJ_TRUE : PJ_FALSE;
}

std::string rxDataMethod(pjsip_rx_data* rdata)
{
    if (rdata == NULL || rdata->msg_info.msg == NULL)
    {
        return "";
    }
    pjsip_msg* msg = rdata->msg_info.msg;
    if (msg->type == PJSIP_REQUEST_MSG)
    {
        return pjStrToString(msg->line.req.method.name);
    }
    if (msg->type == PJSIP_RESPONSE_MSG)
    {
        pjsip_cseq_hdr* cseq = static_cast<pjsip_cseq_hdr*>(pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL));
        if (cseq != NULL)
        {
            return pjStrToString(cseq->method.name) + "/" + std::to_string(msg->line.status.code);
        }
        return std::to_string(msg->line.status.code);
    }
    return "";
}

char g_recvModuleName[] = "mod-gb28181-node";

pjsip_module g_recvModule =
{
    NULL, NULL,
    {g_recvModuleName, 16},
    -1,
    // Handle dialog ACKs before PJSIP's UA layer consumes unknown dialogs.
    PJSIP_MOD_PRIORITY_UA_PROXY_LAYER - 1,
    NULL,
    NULL,
    NULL,
    NULL,
    onRxRequest,
    onRxResponse,
    NULL,
    NULL,
    NULL,
};

} // namespace

PjsipStackAdapter::PjsipStackAdapter()
    : m_endpoint(NULL),
      m_mediaEndpoint(NULL),
      m_pool(NULL),
      m_running(false),
      m_sentMessageCount(0),
      m_hasPendingResponse(false),
      m_pendingResponse(),
      m_pjlibInitialized(false),
      m_cachingPoolInitialized(false),
      m_eventThreadPj(NULL)
{
    std::memset(&m_cachingPool, 0, sizeof(m_cachingPool));
    std::memset(m_eventThreadDesc, 0, sizeof(m_eventThreadDesc));
}

PjsipStackAdapter::~PjsipStackAdapter()
{
    stop();
}

bool PjsipStackAdapter::start(const std::vector<SipListenEndpoint>& endpoints, const RequestHandler& handler)
{
    ensurePjThreadRegistered();

    if (endpoints.empty() || !handler)
    {
        return false;
    }

    m_handler = handler;
    g_adapter = this;

    if (!initPjlib() || !initEndpoint())
    {
        stop();
        return false;
    }

    for (std::vector<SipListenEndpoint>::const_iterator it = endpoints.begin(); it != endpoints.end(); ++it)
    {
        if (!startTransport(it->port))
        {
            stop();
            return false;
        }
    }

    if (!registerRecvModule())
    {
        stop();
        return false;
    }

    m_running.store(true);
    m_eventThread = std::thread(&PjsipStackAdapter::eventLoop, this);
    return true;
}

void PjsipStackAdapter::stop()
{
    ensurePjThreadRegistered();

    m_running.store(false);
    m_sentMessageCount = 0;
    if (m_eventThread.joinable())
    {
        m_eventThread.join();
    }

    if (m_mediaEndpoint != NULL)
    {
        pjmedia_endpt_destroy(m_mediaEndpoint);
        m_mediaEndpoint = NULL;
    }
    if (m_endpoint != NULL)
    {
        pjsip_endpt_destroy(m_endpoint);
        m_endpoint = NULL;
        m_pool = NULL;
    }

    if (m_cachingPoolInitialized)
    {
        pj_caching_pool_destroy(&m_cachingPool);
        m_cachingPoolInitialized = false;
    }

    if (m_pjlibInitialized)
    {
        pj_shutdown();
        m_pjlibInitialized = false;
    }

    if (g_adapter == this)
    {
        g_adapter = NULL;
    }
}

bool PjsipStackAdapter::running() const
{
    return m_running.load();
}

size_t PjsipStackAdapter::sentMessageCount() const
{
    return m_sentMessageCount;
}

bool PjsipStackAdapter::send(const SipMessageContext& message)
{
    ensurePjThreadRegistered();

    if (!m_running.load() || m_endpoint == NULL || message.method.empty())
    {
        return false;
    }

    if (message.response)
    {
        m_pendingResponse = message;
        m_hasPendingResponse = true;
        ++m_sentMessageCount;
        return true;
    }

    pjsip_method method;
    initMethod(message.method, &method);

    const std::string contactTarget = sipUriFromContact(message.contact);
    if (contactTarget.empty() && (message.remoteIp.empty() || message.remotePort <= 0))
    {
        return false;
    }

    const std::string targetValue = contactTarget.empty() ? sipUri(message.toId, message.remoteIp, message.remotePort) : contactTarget;
    const std::string fromValue = sipUri(message.fromId, message.localIp, message.localPort);
    const std::string toValue = sipUri(message.toId, message.remoteIp, message.remotePort);
    const std::string contactValue = sipContact(message.fromId, message.localIp, message.localPort);

    pj_str_t target = toPjString(targetValue);
    pj_str_t from = toPjString(fromValue);
    pj_str_t to = toPjString(toValue);
    pj_str_t contact = toPjString(contactValue);
    pj_str_t body = toPjString(message.body);

    pjsip_tx_data* tdata = NULL;
    const pj_status_t createStatus = pjsip_endpt_create_request(m_endpoint,
                                                                 &method,
                                                                 &target,
                                                                 &from,
                                                                 &to,
                                                                 &contact,
                                                                 NULL,
                                                                 -1,
                                                                 message.body.empty() ? NULL : &body,
                                                                 &tdata);
    if (createStatus != PJ_SUCCESS || tdata == NULL)
    {
        return false;
    }

    if (message.method == "REGISTER" && message.expires >= 0 && tdata->msg != NULL)
    {
        pjsip_expires_hdr* expires = pjsip_expires_hdr_create(tdata->pool, message.expires);
        pjsip_msg_add_hdr(tdata->msg, reinterpret_cast<pjsip_hdr*>(expires));
    }

    if (!message.callId.empty() && tdata->msg != NULL)
    {
        pjsip_cid_hdr* callId = static_cast<pjsip_cid_hdr*>(pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CALL_ID, NULL));
        if (callId != NULL)
        {
            setPoolString(tdata->pool, &callId->id, message.callId);
        }
    }

    if ((!message.fromTag.empty() || !message.toTag.empty()) && tdata->msg != NULL)
    {
        pjsip_fromto_hdr* fromHeader = static_cast<pjsip_fromto_hdr*>(pjsip_msg_find_hdr(tdata->msg, PJSIP_H_FROM, NULL));
        if (fromHeader != NULL && !message.fromTag.empty())
        {
            setPoolString(tdata->pool, &fromHeader->tag, message.fromTag);
        }

        pjsip_fromto_hdr* toHeader = static_cast<pjsip_fromto_hdr*>(pjsip_msg_find_hdr(tdata->msg, PJSIP_H_TO, NULL));
        if (toHeader != NULL && !message.toTag.empty())
        {
            setPoolString(tdata->pool, &toHeader->tag, message.toTag);
        }
    }

    if (!message.cseq.empty() && tdata->msg != NULL)
    {
        pjsip_cseq_hdr* cseq = static_cast<pjsip_cseq_hdr*>(pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL));
        if (cseq != NULL)
        {
            cseq->cseq = static_cast<pj_int32_t>(std::atoi(message.cseq.c_str()));
        }
    }

    if (!message.digestAuth.response.empty() && tdata->msg != NULL)
    {
        pjsip_authorization_hdr* auth = pjsip_authorization_hdr_create(tdata->pool);
        // Authorization may be serialized after this function returns.  Store every
        // Digest value in tdata's pool instead of retaining std::string storage.
        setPoolString(tdata->pool, &auth->scheme, "Digest");
        setPoolString(tdata->pool, &auth->credential.digest.username, message.digestAuth.username);
        setPoolString(tdata->pool, &auth->credential.digest.realm, message.digestAuth.realm);
        setPoolString(tdata->pool, &auth->credential.digest.nonce, message.digestAuth.nonce);
        setPoolString(tdata->pool, &auth->credential.digest.uri, message.digestAuth.uri);
        setPoolString(tdata->pool, &auth->credential.digest.response, message.digestAuth.response);
        const std::string algorithm = message.digestAuth.algorithm.empty() ? "MD5" : message.digestAuth.algorithm;
        setPoolString(tdata->pool, &auth->credential.digest.algorithm, algorithm);
        setPoolString(tdata->pool, &auth->credential.digest.opaque, message.digestAuth.opaque);
        setPoolString(tdata->pool, &auth->credential.digest.qop, message.digestAuth.qop);
        setPoolString(tdata->pool, &auth->credential.digest.nc, message.digestAuth.nc);
        setPoolString(tdata->pool, &auth->credential.digest.cnonce, message.digestAuth.cnonce);
        pjsip_msg_add_hdr(tdata->msg, reinterpret_cast<pjsip_hdr*>(auth));
    }

    if (!message.contentType.empty() && tdata->msg != NULL && tdata->msg->body != NULL)
    {
        std::string type = "Application";
        std::string subtype = "MANSCDP+xml";
        const std::string::size_type slash = message.contentType.find('/');
        if (slash != std::string::npos)
        {
            type = message.contentType.substr(0, slash);
            subtype = message.contentType.substr(slash + 1);
        }

        pj_str_t pjType = toPjString(type);
        pj_str_t pjSubtype = toPjString(subtype);
        pj_str_t pjBody = toPjString(message.body);
        tdata->msg->body = pjsip_msg_body_create(tdata->pool, &pjType, &pjSubtype, &pjBody);
    }

    std::string generatedCallId;
    std::string generatedCseq;
    if (tdata->msg != NULL)
    {
        pjsip_cid_hdr* callId = static_cast<pjsip_cid_hdr*>(pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CALL_ID, NULL));
        pjsip_cseq_hdr* cseq = static_cast<pjsip_cseq_hdr*>(pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL));
        if (callId != NULL) generatedCallId = pjStrToString(callId->id);
        if (cseq != NULL) generatedCseq = std::to_string(cseq->cseq);
    }
    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "pjsip",
                                         "send method=" + message.method + " target=" + targetValue +
                                         " from=" + fromValue + " to=" + toValue +
                                         " call_id=" + generatedCallId + " cseq=" + generatedCseq);

    if (message.method == "INVITE")
    {
        const pj_status_t sendStatus = pjsip_endpt_send_request(m_endpoint, tdata, -1, this, &PjsipStackAdapter::sendCallback);
        if (sendStatus != PJ_SUCCESS)
        {
            logPjFailure("pjsip INVITE send", sendStatus);
            return false;
        }

        ++m_sentMessageCount;
        return true;
    }

    const pj_status_t sendStatus = pjsip_endpt_send_request_stateless(m_endpoint, tdata, NULL, NULL);
    if (sendStatus != PJ_SUCCESS)
    {
        logPjFailure("pjsip stateless send", sendStatus);
        return false;
    }

    ++m_sentMessageCount;
    return true;
}

void PjsipStackAdapter::sendCallback(void* token, pjsip_event* event)
{
    PjsipStackAdapter* adapter = static_cast<PjsipStackAdapter*>(token);
    if (adapter == NULL || event == NULL)
    {
        return;
    }

    if (event->type != PJSIP_EVENT_TSX_STATE)
    {
        return;
    }

    pjsip_rx_data* rdata = event->body.tsx_state.src.rdata;
    if (rdata == NULL || rdata->msg_info.msg == NULL)
    {
        return;
    }

    if (rdata->msg_info.msg->type != PJSIP_RESPONSE_MSG)
    {
        return;
    }

    media_fabric::Logger::instance().log(media_fabric::LOG_DEBUG, "pjsip", "transaction response=" + rxDataMethod(rdata));
    adapter->handleRxResponse(rdata);
}

bool PjsipStackAdapter::initPjlib()
{
    pj_status_t status = pj_init();
    if (status != PJ_SUCCESS)
    {
        logPjFailure("pj_init", status);
        return false;
    }
    m_pjlibInitialized = true;

    status = pjlib_util_init();
    if (status != PJ_SUCCESS)
    {
        logPjFailure("pjlib_util_init", status);
        return false;
    }
    pj_log_set_level(0);
    return true;
}

bool PjsipStackAdapter::initEndpoint()
{
    pj_caching_pool_init(&m_cachingPool, NULL, SIP_STACK_SIZE);
    m_cachingPoolInitialized = true;

    pj_status_t status = pjsip_endpt_create(&m_cachingPool.factory, NULL, &m_endpoint);
    if (status != PJ_SUCCESS)
    {
        logPjFailure("pjsip_endpt_create", status);
        return false;
    }

    pj_ioqueue_t* ioqueue = pjsip_endpt_get_ioqueue(m_endpoint);
    status = pjmedia_endpt_create(&m_cachingPool.factory, ioqueue, 0, &m_mediaEndpoint);
    if (status != PJ_SUCCESS)
    {
        logPjFailure("pjmedia_endpt_create", status);
        return false;
    }

    status = pjsip_tsx_layer_init_module(m_endpoint);
    if (status != PJ_SUCCESS)
    {
        logPjFailure("pjsip_tsx_layer_init_module", status);
        return false;
    }

    status = pjsip_ua_init_module(m_endpoint, NULL);
    if (status != PJ_SUCCESS)
    {
        logPjFailure("pjsip_ua_init_module", status);
        return false;
    }

    m_pool = pjsip_endpt_create_pool(m_endpoint, NULL, SIP_ALLOC_POOL_1M, SIP_ALLOC_POOL_1M);
    return m_pool != NULL;
}

bool PjsipStackAdapter::startTransport(int sipPort)
{
    pj_sockaddr_in addr;
    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = pj_AF_INET();
    addr.sin_addr.s_addr = 0;
    addr.sin_port = pj_htons((pj_uint16_t)sipPort);

    pj_status_t status = pjsip_udp_transport_start(m_endpoint, &addr, NULL, 1, NULL);
    if (status != PJ_SUCCESS)
    {
        std::ostringstream action;
        action << "pjsip_udp_transport_start port=" << sipPort;
        logPjFailure(action.str(), status);
        return false;
    }

    status = pjsip_tcp_transport_start(m_endpoint, &addr, 1, NULL);
    if (status != PJ_SUCCESS)
    {
        if (status == PJSIP_ETYPEEXISTS)
        {
            media_fabric::Logger::instance().log(media_fabric::LOG_WARN, "pjsip",
                                                 "TCP transport already exists on port=" + std::to_string(sipPort));
            return true;
        }

        std::ostringstream action;
        action << "pjsip_tcp_transport_start port=" << sipPort;
        logPjFailure(action.str(), status);
        return false;
    }

    return true;
}

bool PjsipStackAdapter::registerRecvModule()
{
    return pjsip_endpt_register_module(m_endpoint, &g_recvModule) == PJ_SUCCESS;
}

bool PjsipStackAdapter::sendStatelessResponse(pjsip_rx_data* rdata, const SipMessageContext& response)
{
    if (rdata == NULL || m_endpoint == NULL || response.statusCode <= 0)
    {
        return false;
    }

    pj_str_t reason = toPjString(response.reason);
    pjsip_msg_body* body = NULL;
    if (!response.body.empty())
    {
        std::string type = "text";
        std::string subtype = "plain";
        const std::string::size_type slash = response.contentType.find('/');
        if (slash != std::string::npos)
        {
            type = response.contentType.substr(0, slash);
            subtype = response.contentType.substr(slash + 1);
        }

        pj_str_t pjType = toPjString(type);
        pj_str_t pjSubtype = toPjString(subtype);
        pj_str_t pjBody = toPjString(response.body);
        body = pjsip_msg_body_create(rdata->tp_info.pool, &pjType, &pjSubtype, &pjBody);
    }

    pjsip_hdr hdrList;
    pj_list_init(&hdrList);
    const pjsip_hdr* responseHeaders = NULL;
    if (!response.authRealm.empty())
    {
        pjsip_www_authenticate_hdr* auth = pjsip_www_authenticate_hdr_create(rdata->tp_info.pool);
        auth->scheme = pj_str(const_cast<char*>("Digest"));
        auth->challenge.digest.realm = toPjString(response.authRealm);
        auth->challenge.digest.nonce = toPjString(response.authNonce);
        auth->challenge.digest.opaque = toPjString(response.authOpaque);
        const std::string algorithm = response.authAlgorithm.empty() ? "MD5" : response.authAlgorithm;
        auth->challenge.digest.algorithm = toPjString(algorithm);
        pj_list_push_back(&hdrList, auth);
        responseHeaders = &hdrList;
    }

    const pj_status_t status = pjsip_endpt_respond_stateless(m_endpoint,
                                                              rdata,
                                                              response.statusCode,
                                                              response.reason.empty() ? NULL : &reason,
                                                              responseHeaders,
                                                              body);
    return status == PJ_SUCCESS;
}

void PjsipStackAdapter::eventLoop()
{
    std::memset(m_eventThreadDesc, 0, sizeof(m_eventThreadDesc));
    pj_thread_register("pjsip_event_loop", m_eventThreadDesc, &m_eventThreadPj);

    while (m_running.load())
    {
        pj_time_val timeout = {0, 500};
        pjsip_endpt_handle_events(m_endpoint, &timeout);
    }
}

bool PjsipStackAdapter::handleRxRequest(pjsip_rx_data* rdata)
{
    if (rdata == NULL || rdata->msg_info.msg == NULL || !m_handler)
    {
        return false;
    }

    media_fabric::Logger::instance().log(media_fabric::LOG_DEBUG, "pjsip", "rx request=" + rxDataMethod(rdata));

    m_hasPendingResponse = false;
    const bool handled = m_handler(toRequestContext(rdata));

    if (m_hasPendingResponse)
    {
        sendStatelessResponse(rdata, m_pendingResponse);
        m_hasPendingResponse = false;
    }
    else if (rxDataMethod(rdata) != "ACK")
    {
        SipMessageContext response;
        response.response = true;
        response.statusCode = handled ? 200 : 403;
        response.reason = handled ? "OK" : "Forbidden";
        sendStatelessResponse(rdata, response);
    }

    return handled;
}

bool PjsipStackAdapter::handleRxResponse(pjsip_rx_data* rdata)
{
    if (rdata == NULL || rdata->msg_info.msg == NULL || !m_handler)
    {
        return false;
    }

    media_fabric::Logger::instance().log(media_fabric::LOG_DEBUG, "pjsip", "rx response=" + rxDataMethod(rdata));

    return m_handler(toResponseContext(rdata));
}

} // namespace gb28181

#ifndef _SIPDEF_H
#define _SIPDEF_H

#define SIP_STACK_SIZE (1024 * 256)
#define SIP_ALLOC_POOL_1M (1024 * 1024 * 1)

#define SIP_NOTIFY "Notify"
#define SIP_HEARTBEAT "keepalive"
#define SIP_RESPONSE "Response"
#define SIP_QUERY "Query"
#define SIP_CATALOG "Catalog"
#define SIP_RECORDINFO "RecordInfo"

enum statusCode
{
    SIP_SUCCESS = 200,
    SIP_TRYING = 100,
    SIP_UNAUTHORIZED = 401,
};

enum DevTypeCode
{
    IPC_Code = 132,
    NVR_Code = 118,
    CenterServer_Code = 200,
};

#endif

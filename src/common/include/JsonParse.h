#ifndef _JSONPARSE_H
#define _JSONPARSE_H

#include "json/json.h"

#include <memory>
#include <string>

class JsonParse
{
public:
    explicit JsonParse(const std::string& s) : m_str(s) {}
    explicit JsonParse(const Json::Value& j) : m_json(j) {}

    bool toJson(Json::Value& j)
    {
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);
        builder["collectComments"] = true;

        const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        JSONCPP_STRING errs;
        return reader->parse(m_str.data(), m_str.data() + m_str.size(), &j, &errs) && errs.empty();
    }

    std::string toString()
    {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return Json::writeString(builder, m_json);
    }

private:
    std::string m_str;
    Json::Value m_json;
};

#endif

#ifndef _COMMON_H
#define _COMMON_H
#include <glog/logging.h>
#include"tinyxml2.h"
#include"json/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <string>

using namespace std;

#define LOG_DIR "/home/GB28181-Server/log"
#define LOG_FILE_NAME "SipSubService.log" 
#define BODY_SIZE 1024*10

class AutoMutexLock
{
    public:
    AutoMutexLock(pthread_mutex_t* l):lock(l)
    {
        LOG(INFO)<<"getLock";
        getLock();
    };
    ~AutoMutexLock()
    {
        LOG(INFO)<<"freeLock";
        freeLock();
    };
    private:
    //将默认构造，拷贝构造，运算符重载私有化，禁止外部调用
    AutoMutexLock();
    AutoMutexLock(const AutoMutexLock&);
    AutoMutexLock& operator=(const AutoMutexLock&);
    void getLock(){pthread_mutex_lock(lock);}
    void freeLock(){pthread_mutex_unlock(lock);}
    pthread_mutex_t* lock;
};

class JsonParse
{
    public:
    JsonParse(string s):m_str(s){}
    JsonParse(Json::Value j):m_json(j){}
    bool toJson(Json::Value& j)
    {
        bool bret =false;
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);
        //LOG(INFO) << "CharReaderBuilder defaults:\n" << builder.settings_.toStyledString();
        builder["collectComments"]=true;
        //LOG(INFO) << "CharReaderBuilder strictMode:\n" << builder.settings_.toStyledString();

        const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());//智能指针
        JSONCPP_STRING errs;
        bret=reader->parse(m_str.data(),m_str.data()+m_str.size(),&j,&errs);
        if(!bret||!errs.empty())
        {
            LOG(ERROR)<<"json parse error:"<<errs.c_str();
        }
        return bret;
    }

    string toString()
    {
        Json::StreamWriterBuilder builder;
        const char* indent="";
        builder["indentation"]=indent;
        return Json::writeString(builder,m_json);
    }
    private:
    string m_str;
    Json::Value m_json;
};

#endif
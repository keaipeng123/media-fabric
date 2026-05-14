#ifndef _SIPDEF_H
#define _SIPDEF_H
#include<string>
#include<string.h>
using namespace std;
#define SIP_STACK_SIZE 1024*256
#define SIP_ALLOC_POOL_1M 1024*1024*1

#define SIP_NOTIFY "Notify"
#define SIP_HEARTBEAT "keepalive"
#define SIP_RESPONSE "Response"
#define SIP_CATALOG "Catalog"
#define SIP_RECORDINFO "RecordInfo"

enum statusCode
{
    SIP_SUCCESS=200,
    SIP_BADREQUEST=400,//请求的参数或者格式不对，请求非法
    SIP_FORBIDDEN=404,//请求的用户在本域中不存在
    
};

//类型编码枚举 11 12 13位
enum DevTypeCode
{
    Error_Code=-1,
    Dvr_Code= 111,
    ViderServer_Code=112,//视频服务器
    Encoder_Code=113,//编码
    Decoder_Code=114,//解码
    AlarmDev_Code=117,//报警控制器编码
    NVR_Code=118,//网络视频录像机

    Camera_Code=131,//USB摄像头
    Ipc_Code=132,//网络摄像头
    VGA_Code=133,//显示器
    AlarmInput_Code=134,//报警设备
    AlarmOutput_Code=135,//报警输出设备

    CenterServer_Code=200, //中心信令控制服务器编码
};

struct DeviceInfo
{
    string devid;
    string playformId;//中心平台id
    string streamName;//实时流还是回放流
    string setupType; //指定rtp流为tcp时，需要指定setup为active主动或者passive被动
    int protocal;//tcp udp
    int startTime;
    int endTime;
    //int fd;
};

//与前端协商的内部自定义头部
struct StreamHeader
{
    int type;//媒体类型
    int length;//负载数据的长度
    int videoH;//视频分辨率-高
    int videoW;//视频分辨率-宽
    char format[4];//音视频其他参数

};

enum CommandCode
{
    Command_Session_Register=1,
    Command_Session_Catalog=2,
};

#endif
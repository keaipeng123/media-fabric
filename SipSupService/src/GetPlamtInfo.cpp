#include"GetPlamtInfo.h"

GetPlamtInfo::GetPlamtInfo(struct bufferevent* bev)
:ThreadTask(bev)
{
    
}

void GetPlamtInfo::run()
{
    LOG(INFO)<<"GetPlamtInfo run";
}
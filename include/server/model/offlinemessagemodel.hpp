#ifndef OFFLINEMESSAGEMODEL_H
#define OFFLINEMESSAGEMODEL_H

#include<vector>
#include<string>
using namespace std;
class OfflineMsgModel
{
public:

//存储用户离线消息
void insert(int userid, std::string msg);
//删除用户离线消息
void remove(int userid);
//查询用户离线消息
vector<string> query(int userid);
private:

};

#endif
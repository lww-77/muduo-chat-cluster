#include"offlinemessagemodel.hpp"
#include"db.h"

//存储用户离线消息
void OfflineMsgModel::insert(int userid, std::string msg)
{
    //组装SQL语句
    char sql[1024]={0};
    sprintf(sql, "insert into offlinemessage valuses(%d,'%s')",
            userid,msg.c_str());

    MySQL mysql;
    if(mysql.connect())
    {
      mysql.update(sql);
    }

}

//删除用户离线消息
void OfflineMsgModel::remove(int userid)
{
    //组装SQL语句
    char sql[1024]={0};
    sprintf(sql, "delete from offlinemessage where userid = %d",userid);

    MySQL mysql;
    if(mysql.connect())
    {
      mysql.update(sql);
    }

}
//查询用户离线消息
std::vector<std::string> OfflineMsgModel::query(int userid)
{
    char sql[1024]={0};
    sprintf(sql,"select message from offlinemessage where userid = %d",userid);

    MySQL mysql;
    std::vector<std::string> vec;
    if(mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if(res!=nullptr)
        {           
            //把userid的所有离线消息放入vector容器中返回
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                vec.push_back(row[0]);
            }     
            mysql_free_result(res);
        }
    }
    return vec;

}
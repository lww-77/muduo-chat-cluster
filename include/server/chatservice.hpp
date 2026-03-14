#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include<functional>
#include<muduo/net/TcpServer.h>
#include<unordered_map>
#include<usermodel.hpp>
#include<mutex>
#include"offlinemessagemodel.hpp"
#include"friendmodel.hpp"
#include"groupmodel.hpp"
using namespace muduo;
using namespace muduo::net;
using namespace std;
#include"json.hpp"
#include"redis.hpp"
using json = nlohmann::json;
//定义事件处理器
using msgHandler = std::function<void(const TcpConnectionPtr &conn,json &js,Timestamp)>;

//聊天服务器类
class ChatService
{
public:
    //获取单例模式对象的接口函数
    static ChatService* instance();

    //处理注册业务
    void Login(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //登陆方法的回调函数
    void Reg(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //注销方法的回调函数
    void loginOut(const TcpConnectionPtr &conn,json &js,Timestamp time);
    
    //一对一聊天服务
    void oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //获取消息对应的处理器
    msgHandler getHandler(int msgid);

    //客户端断开连接异常处理
    void cilentCloseException(const TcpConnectionPtr &conn);

    //服务器异常，业务重置方法
    void reset();

    //添加好友
    void addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //创建群组任务
    void creatGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //加入群组任务
    void joinGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //群组聊天业务
    void groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //处理上报通道消息的回调
    void handlerRedisSubscribeMessage(int userid,string message);
private:
    ChatService();

    //存储消息id和其他对应的业务处理方法
    std::unordered_map<int,msgHandler> _msgHandlerMap;

    //记录用户的连接信息
    std::unordered_map<int,TcpConnectionPtr> _userConnMap;

    //构建互斥锁，保证线程通信的互斥操作
    std::mutex _connMutex;
    
    //数据操作类对象
    UserModel _userModel;

    OfflineMsgModel _offlineMsgModel;

    FriendModel _friendModel;

    GroupModel _groupModel;

    Redis _redis;
};





#endif
#include"chatservice.hpp"
#include"public.hpp"
#include<vector>
#include<muduo/base/Logging.h>
using namespace std::placeholders;
using namespace std;

    //获取单例模式对象的接口函数
    ChatService* ChatService::instance()
    {
        static ChatService service;
        return &service;
    }

    //注册消息以及对应的Handler回调操作
    ChatService::ChatService()
    {
        //用户基本业务管理相关事件处理回调注册
        _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::Login,this,_1,_2,_3)});
        _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginOut,this,_1,_2,_3)});
        _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::Reg,this,_1,_2,_3)});
        _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat,this,_1,_2,_3)});
        _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend,this,_1,_2,_3)});

        //群组业务管理相关事件处理回调注册
        _msgHandlerMap.insert({CREAT_GROUP_MSG, std::bind(&ChatService::creatGroup,this,_1,_2,_3)});
        _msgHandlerMap.insert({JOIN_GROUP_MSG, std::bind(&ChatService::joinGroup,this,_1,_2,_3)});
        _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat,this,_1,_2,_3)});



        //连接redis服务器
        if(_redis.connect())
        {
            //设置上报消息的回调
            _redis.init_notify_handler(std::bind(&ChatService::handlerRedisSubscribeMessage,this,_1,_2));
        }
    }

    //获取消息对应的处理器
    msgHandler ChatService::getHandler(int msgid)
    {
        
        if(_msgHandlerMap.find(msgid)==_msgHandlerMap.end())
        {
            //记录错误日志，msgid没有对应的事件处理回调
            return [=](const TcpConnectionPtr &conn,json &js,Timestamp time)
                    {
                        LOG_ERROR<<"msgid"<<" can not find handler!";
                    };
        }
        else
        {
             return _msgHandlerMap[msgid];
        }
       
    }

    //处理登录业务
    void ChatService::Login(const TcpConnectionPtr &conn,json &js,Timestamp time)
    {
        int id=js["id"].get<int>();
        string pwd=js["password"];
        User user = _userModel.query(id);
        if(user.getId()!=-1 && user.getPwd()==pwd)
        {

            //判断用户是否已经在线
            if(user.getState()=="online")
            {
                json response;
                response["msgid"]=LOGIN_MSG_ACK;
                response["error_num"]=2;
                response["errormsg"]="该用户已经登陆！";
                conn->send(response.dump());
            }

            //登陆成功 记录用户的连接数据
            {
                std::lock_guard<mutex> _lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            //id登陆成功，向redis订阅通道channel(id)
            _redis.subscribe(id);

            //登陆成功  更改用户状态信息
            user.setState("online");
            _userModel.updateState(user);


            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["error_num"]=0;
            response["id"]=user.getId();
            response["name"]=user.getName();

            //查询该用户是否有离线信息
            vector<string> vec =_offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"]=vec;
                //读取完用户的离线信息之后，将所有的离线信息删除
                _offlineMsgModel.remove(id);
            }

            //查询用户的好友列表
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(User& user : userVec)
                {
                    json js;
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"]=vec2;
            }
            LOG_INFO << "After setting friends: " << response.dump();

            //查询用户的群组列表
            vector<Group>groupvec = _groupModel.queryGroup(id);
            if(!groupvec.empty())
            {
                vector<string> groupV;     
                for(Group& group : groupvec)
                {
                    json groupjs;
                    groupjs["groupid"]=group.getGroupId();
                    groupjs["groupname"]=group.getGroupName();
                    groupjs["groupdesc"]=group.getGroupDesc();
                    vector<string>userV;
                    for(GroupUser& users:group.getGroupUsers())
                    {
                        json groupuserjs;
                        groupuserjs["userid"] = users.getId();
                        groupuserjs["name"] = users.getName();
                        groupuserjs["state"] = users.getState();
                        groupuserjs["role"] = users.getRole();
                        userV.push_back(groupuserjs.dump());
                    }

                    groupjs["users"]=userV;
                    groupV.push_back(groupjs.dump());
                }
                response["groups"]=groupV;
            }
            LOG_INFO << "Final login response: " << response.dump();
            conn->send(response.dump());


        }
        else
        {
            //密码错误，或者用户不存在
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["error_num"]=1;
            response["errormsg"]="用户不存在，或密码错误！";
            conn->send(response.dump());
        }
    }

    //注册方法的回调函数
    void ChatService::Reg(const TcpConnectionPtr &conn,json &js,Timestamp time)
    {
        
        string name = js["name"];
        string password = js["password"];

        User user;
        user.setName(name);
        user.setPwd(password);
        
        bool state = _userModel.insert(user);
        if(state)
        {
            //注册成功
            json resonse;
            resonse["msgid"]=REG_MSG_ACK;
            resonse["error_num"]=0;
            resonse["id"]=user.getId();
            conn->send(resonse.dump());
        }
        else
        {
            //注册失败
            json resonse;
            resonse["msgid"]=REG_MSG_ACK;
            resonse["error_num"]=1;
            
            conn->send(resonse.dump());
        }
    }

//注销方法的回调函数
void ChatService::loginOut(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        unique_lock<mutex>lock(_connMutex);
        auto it = _userConnMap.find(userid);

            if(it!=_userConnMap.end())
            {
                _userConnMap.erase(it);
            }

    }

    //用户注销，相当于下线，在redis中取消通道订阅
    _redis.unSubscribe(userid);

    //更新状态信息
    User user;
    user.setId(userid);
    user.setState("offline");
    _userModel.updateState(user);
}

//客户端断开连接异常处理
void ChatService::cilentCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        std::lock_guard<mutex> _lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                user.setId(it->first);
                _userConnMap.erase(it->first);
                break;
            }
        }
    }

    //用户注销，相当于下线，在redis中取消通道订阅
    _redis.unSubscribe(user.getId());

    if(user.getId()!=-1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }

}

//一对一聊天服务
void ChatService::oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int toid = js["toid"].get<int>();

    {
        std::lock_guard<mutex> _lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线 转发消息，服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    //在user表中查询用户是否在其他服务器上登录
    User user = _userModel.query(toid);
    if(user.getState()=="online")
    {
        _redis.publish(toid,js.dump());
        return;
    }



    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

//服务器异常，业务重置方法
void ChatService::reset()
{
    _userModel.resetState();
}

//添加好友
void ChatService::addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int friendid=js["friendid"].get<int>();
    int userid=js["userid"].get<int>();

    _friendModel.insert(userid,friendid);
}



//创建群组任务
void ChatService::creatGroup(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    string groupname = js["groupname"];
    string groupdesc = js["groupdesc"];

    Group group;
    group.setGroupName(groupname);
    group.setGroupDesc(groupdesc);

    if(_groupModel.creatGroup(group))
    {
        _groupModel.joinGroup(userid, group.getGroupId(),"creator");
    }
}

//加入群组任务
void ChatService::joinGroup(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();

    _groupModel.joinGroup(userid,groupid,"normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    vector<int>vec;
    vec = _groupModel.queryGroupUser(userid,groupid);

    lock_guard<mutex> lock(_connMutex);
    for(int userid:vec)
    {
        auto it = _userConnMap.find(userid);
        if(it!=_userConnMap.end())
        {
            //发送在线消息
            it->second->send(js.dump());
        }
        else
        {
                //在user表中查询用户是否在其他服务器上登录
                User user = _userModel.query(userid);
                if(user.getState()=="online")
                {
                    _redis.publish(userid,js.dump());
                    return;
                }
                else
                {
                    // 发送离线消息
                    _offlineMsgModel.insert(userid, js.dump());
                }

        }
    }
}

//从redis消息队列中获取订阅的信息
void ChatService::handlerRedisSubscribeMessage(int userid,string message)
{
    lock_guard<mutex>lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it!=_userConnMap.end())
    {
        it->second->send(message);
    }
    else
    {
        _offlineMsgModel.insert(userid,message);
    }
}
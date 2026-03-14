#include<iostream>
#include"json.hpp"
#include<vector>
#include<thread>
#include<string>
#include<chrono>
#include<ctime>
#include<functional>
#include<unordered_map>
using namespace std;
using json = nlohmann::json;


#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<semaphore.h>
#include<atomic>


#include"group.hpp"
#include"user.hpp"
#include"public.hpp"


//记录当前系统登录的用户信息
User g_currentUser;
//记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
//控制聊天界面程序
bool isMainMenuRunning = false;
//定义用于读写线程之间的通信
sem_t _rwsem;
//记录登陆状态
atomic_bool g_isLoginSuccess{false};

//显示当前登录成功用户的基本信息
void showCurrentUserData();

//接收线程
void readTaskHandler(int clientfd);

//获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();

//主聊天界面
void mainMenu(int clientfd);

//聊天客户端实现 main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if(argc<3)
    {
        cerr<<"command invalid! example: ./ChatClient 192.168.88.129 6000"<<endl;
        exit(-1);
    }

    //解析通过命令行参数传递的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    //创建client端的socket
    int clientfd = socket(AF_INET,SOCK_STREAM,0);
    if(-1==clientfd)
    {
        cerr<<"socket create error"<<endl;
        exit(-1);
    }

    //填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server,0,sizeof(sockaddr_in));
    
    server.sin_family=AF_INET;
    server.sin_port=htons(port);
    server.sin_addr.s_addr=inet_addr(ip);

    //client 和server 对接
    if(-1==connect(clientfd,(sockaddr*)&server,sizeof(sockaddr_in)))
    {
        cerr<<"connect server error"<<endl;
        close(clientfd);
        exit(-1);
    }

    //初始化信号量资源计数
    sem_init(&_rwsem,0,0);

    //连接服务器成功，启动子线程
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    //main线程用于接受用户输入 负责发送数据
    for (;;)
    {
        // 显示受界面菜单 登录 注册 退出
        cout << "==========================" << endl;
        cout << "1、LOGIN" << endl;
        cout << "2、REGISTER" << endl;
        cout << "3、QUIT" << endl;
        cout << "==========================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残存的回车

        switch (choice)
        {

            case 1: // login业务
            {
                int id = 0;
                cout << "userid:";
                cin >> id;
                cin.get();
                cout << "userpassword:";
                char pwd[50] = {0};
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();


                g_isLoginSuccess=false;
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if (-1 == len)
                {
                    cerr << "send login response error" << request << endl;
                }

                sem_wait(&_rwsem);//等待信号量，子线程处理完登录响应，通知处理

                if(g_isLoginSuccess==true)
                {
                    // 进入聊天主菜单页面
                    isMainMenuRunning = true;
                    mainMenu(clientfd);
                }


                break;
            }

            case 2: // register业务
            {
                char name[50] = {0};
                char pwd[50] = {0};
                cout << "username:";
                cin.getline(name, 50);
                cout << "userpassword:";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if (-1 == len)
                {
                    cerr << "send reg response error" << endl;
                }
                sem_wait(&_rwsem);
                
                break;
            }

            
            case 3: // 退出业务
                close(clientfd);
                sem_destroy(&_rwsem);
                exit(0);
            default:
                cerr << "invalid input" << endl;
                break;
        
        }     
    }
    return 0;
}
//显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout<<"========================LOGIN USER========================"<<endl;
    cout<<"current user => id:"<<g_currentUser.getId()<<" name:"<<g_currentUser.getName()<<endl;
    cout<<"------------------------FRIEND LIST------------------------"<<endl;
    if(!g_currentUserFriendList.empty())
    {
        for(User& user:g_currentUserFriendList)
        {
            cout<<user.getId()<<" "<<user.getName()<<" "<<user.getState()<<" "<<endl;
        }
    }
    cout<<"------------------------GROUP LIST------------------------"<<endl;
    if(!g_currentUserGroupList.empty())
    {
        for(Group& group:g_currentUserGroupList)
        {
            cout<<group.getGroupId()<<" "<<group.getGroupName()<<" "<<group.getGroupDesc()<<" "<<endl;
                    for(User& user:group.getGroupUsers())
                        {
                            cout<<user.getId()<<" "<<user.getName()<<" "<<user.getState()<<" "<<endl;
                        }
        }
    }
    cout<<"========================================================="<<endl;
}

//处理登录响应
void doLoginResponse(json& responsejs)
{
    if (0 != responsejs["error_num"].get<int>())
    {
        // 登录失败
        cerr << responsejs["errormsg"] << endl;
        g_isLoginSuccess=false;
    }
    else
    {
        // 登录成功
        g_isLoginSuccess=true;
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        // 记录当前好友信息列表
        if (responsejs.contains("friends"))
        {
            // 初始化
            g_currentUserFriendList.clear();

            vector<string> vec = responsejs["friends"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }
        // 记录当前群组列表信息
        if (responsejs.contains("groups"))
        {
            // 初始化
            g_currentUserGroupList.clear();

            vector<string> groupVec = responsejs["groups"];
            for (string &groupstr : groupVec)
            {
                json groupjs = json::parse(groupstr);
                Group group;
                group.setGroupId(groupjs["groupid"]);
                group.setGroupName(groupjs["groupname"]);
                group.setGroupDesc(groupjs["groupdesc"]);

                vector<string> vec2 = groupjs["users"];
                for (string &userVec : vec2)
                {
                    json userjs = json::parse(userVec);
                    GroupUser groupuser;
                    groupuser.setId(userjs["userid"]);
                    groupuser.setName(userjs["name"]);
                    groupuser.setState(userjs["state"]);
                    groupuser.setRole(userjs["role"]);
                    group.getGroupUsers().push_back(groupuser);
                }

                g_currentUserGroupList.push_back(group);
            }
        }
        // 显示当前用户信息
        showCurrentUserData();

        // 显示当前用户离线信息 个人聊天信息或者群组消息
        if (responsejs.contains("offlinemsg"))
        {
            vector<string> offlineVec = responsejs["offlinemsg"];
            for (string &offstr : offlineVec)
            {
                json offjs = json::parse(offstr);
                if (ONE_CHAT_MSG == offjs["msgid"].get<int>())
                {
                    cout << offjs["time"].get<string>() << " [" << offjs["id"] << " ]" << offjs["name"].get<string>() << " said:" << offjs["message"].get<string>() << endl;
                }
                else
                {
                    cout << "群消息[" << offjs["groupid"] << "]:" << offjs["time"].get<string>() << " [" << offjs["id"] << " ]" << offjs["name"].get<string>() << " said:" << offjs["message"].get<string>() << endl;
                }
            }
        }
    }
}
//处理注册响应
void doRegResponse(json& responsejs)
{
    if (0 != responsejs["error_num"].get<int>())
    {
        // 注册失败
        cerr << "is already exist, register error!" << endl;
    }
    else
    {
        // 注册成功
        cout << " register success " << "userid id:" << responsejs["id"] << "do not forget it!" << endl;
    }
}

//接收线程
void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[1024]={0};
        int len = recv(clientfd,buffer,1024,0);
        if(-1==len || len==0)
        {
            close(clientfd);
            exit(-1);
        }

        //接收ChatServer转发的数据，反序列化生成json对象
        json js = json::parse(buffer);

        int messagetype=js["msgid"].get<int>();
        if(ONE_CHAT_MSG==messagetype)
        {
            cout<<js["time"].get<string>()<<" ["<<js["id"]<<" ]"<<js["name"].get<string>()<<" said:"<<js["message"].get<string>()<<endl;
            continue;
        }
        if(GROUP_CHAT_MSG==messagetype)
        {
            cout<<"群消息["<<js["groupid"]<<"]:"<<js["time"].get<string>()<<" ["<<js["id"]<<" ]"<<js["name"].get<string>()<<" said:"<<js["message"].get<string>()<<endl;
            continue;
        }

        if(LOGIN_MSG_ACK==messagetype)
        {
            //处理登陆响应的业务逻辑
            doLoginResponse(js);
            sem_post(&_rwsem);//通知主线程
            continue;
        }
        if(messagetype==REG_MSG_ACK)
        {
            doRegResponse(js);
            sem_post(&_rwsem);
            continue;
        }
    }
}


void help(int fd=0,string str="");
void chat(int,string);
void addfriend(int,string);
void creategroup(int,string);
void joingroup(int,string);
void groupchat(int,string);
void loginout(int,string);



// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
{"help", "显示所有支持的命令,格式help"}, 
{"chat", "一对一聊天,格式chat:friendid:message"},
{"addfriend", "添加好友,格式addfriend:friendid"},
{"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
{"joingroup", "加入群组,格式joingroup:groupid"},
{"groupchat", "群聊,格式groupchat:groupid:message"},
{"loginout", "注销,格式loginout"}
};

//注册系统支持的客户端命令处理
unordered_map<string,function<void(int, string)>>commandHandlerMap={

    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"joingroup",joingroup},
    {"groupchat",groupchat},
    {"loginout",loginout}

};

void help(int,string)
{
    cout<<"show command list "<<endl;
    for(auto map:commandMap)
    {
        cout<<map.first<<" : "<<map.second<<endl;
    }
}
void chat(int clientfd,string str)
{
    //{"chat", "一对一聊天,格式chat:friendid:message"},
    int idx=str.find(":");
    if(-1==idx)
    {
        cerr<<"chat command invalid!"<<endl;
        return;
    }
    int friendid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"]=ONE_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["toid"]=friendid;
    js["message"]=message;
    js["time"]=getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg fail->" << buffer << endl;
    }
}
void addfriend(int clientfd,string str)
{

        //{"addfriend", "添加好友,格式addfriend:friendid"},
        int friendid = atoi(str.c_str());
        json js;
        js["msgid"]=ADD_FRIEND_MSG;
        js["userid"]=g_currentUser.getId();
        js["friendid"]=friendid;
        string buffer=js.dump();

        int len = send(clientfd, buffer.c_str(),strlen(buffer.c_str())+1,0);
        if(-1==len)
        {
            cerr<<"send addfriend msg fail->"<<buffer<<endl;
        }
}
void creategroup(int clientfd,string str)
{
    //{"creategroup", "创建群组,格式creategroup:groupname:groupdesc"}
    int idx=str.find(":");
    if(-1==idx)
    {
        cerr<<"creategroup command invalid!"<<endl;
        return;
    }
    string groupname=str.substr(0,idx).c_str();
    string groupdesc=str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"]=CREAT_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupname"]=groupname;
    js["groupdesc"]=groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg fail->" << buffer << endl;
    }
}
void joingroup(int clientfd,string str)
{
    //{"joingroup", "加入群组,格式joingroup:groupid"}
    int groupid = atoi(str.c_str());
    
    json js;
    js["msgid"]=JOIN_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupid"]=groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send joingroup msg fail->" << buffer << endl;
    }
}
void groupchat(int clientfd,string str)
{
    //{"groupchat", "群聊,格式groupchat:groupid:message"}
    int idx=str.find(":");
    if(-1==idx)
    {
        cerr<<"groupchat command invalid!"<<endl;
        return;
    }
    int groupid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"]=GROUP_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["groupid"]=groupid;
    js["message"]=message;
    js["time"]=getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg fail->" << buffer << endl;
    }
}
void loginout(int clientfd,string str)
{
    //{"loginout", "注销,格式loginout"}
    json js;
    js["msgid"]=LOGINOUT_MSG;
    js["id"]=g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send loginout msg fail->" << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

//主聊天界面
void mainMenu(int clientfd)
{
    help();

    char buffer[1024]={0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer,1024);
        string commandbuf(buffer);
        string command;//存储命令
        int idx = commandbuf.find(":");
        if(-1==idx)
        {
            command=commandbuf;
        }
        else
        {
            command=commandbuf.substr(0,idx);
        }

        auto it = commandHandlerMap.find(command);
        if(it==commandHandlerMap.end())
        {
            cerr<<"invalid input command!"<<endl;
            continue;
        }
        else
        {
            //调用相应命令的处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
           it->second(clientfd,commandbuf.substr(idx+1,commandbuf.size()-idx));
        }

    }
}

//获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm=localtime(&tt);
    char date[60]={0};
    sprintf(date,"%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year+1900,(int)ptm->tm_mon+1,(int)ptm->tm_mday,
            (int)ptm->tm_hour,(int)ptm->tm_min,(int)ptm->tm_sec);
    return std::string(date);
}
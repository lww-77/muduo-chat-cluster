#include"redis.hpp"
#include<iostream>
Redis::Redis()
    :_publish_context(nullptr),_subscribe_context(nullptr)
    {}

Redis::~Redis()
{
    if(_publish_context!=nullptr)
    {
        redisFree(_publish_context);
    }
    if(_subscribe_context!=nullptr)
    {
        redisFree(_subscribe_context);
    }
}

// 连接redis服务器
bool Redis::connect()
{
    //负责publish发布消息的上下文连接
    _publish_context=redisConnect("127.0.0.1",6379);
    if(_publish_context==nullptr)
    {
        cerr <<"connect redis fail!"<<endl;
        return false;
    }
    //负责subscribe发布消息的上下文连接
    _subscribe_context=redisConnect("127.0.0.1",6379);
    if(_subscribe_context==nullptr)
    {
        cerr <<"connect redis fail!"<<endl;
        return false;
    }

    //单独的线程 监听通道上的事件，有消息给业务层上报
    thread t([&](){observe_channel_message();});
    t.detach();

    cout<<"connect redis success!"<<endl;
    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{
    redisReply* reply=(redisReply*)redisCommand(_publish_context, "PUBLISH %d %s", channel,message.c_str());
    if(reply==nullptr)
    {
        cerr<<"publish command fail!"<<endl;
        return false;
    }

    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道channel订阅消息
bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源

    if(REDIS_ERR==redisAppendCommand(this->_subscribe_context,"SUBSCRIBE %d", channel))
    {
        cerr<<"unsubscribe command fail!"<<endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置一)
    int done=0;
    while(!done)
    {
        if(REDIS_ERR==redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr<<"unsubscribe command fail!"<<endl;
            return false;
        }
    }
    return true;

}

// 向redis指定的通道channel取消订阅消息
bool Redis::unSubscribe(int channel)
{
    if(REDIS_ERR==redisAppendCommand(this->_subscribe_context,"UNSUBSCRIBE %d", channel))
    {
        cerr<<"unsubscribe command fail!"<<endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置一)
    int done=0;
    while(!done)
    {
        if(REDIS_ERR==redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr<<"unsubscribe command fail!"<<endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接受订阅通道中的消息
void Redis::observe_channel_message()
{
    redisReply* reply = nullptr;
    while(REDIS_OK==redisGetReply(this->_subscribe_context,(void**)&reply))
    {
        //订阅接收的消息是一个三元素的数组 
        if(reply!=nullptr &&reply->element[2]!=nullptr && reply->element[2]->str!=nullptr)
        {
            //给业务层上报通道中的消息
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }
        freeReplyObject(reply);
    }

    cerr<<">>>>>>>>>>>>>>>>>>>>>>>>observer_channel_message quit!<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"<<endl;
}

// 初始化向业务层上报通道消息的回调函数
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;
}
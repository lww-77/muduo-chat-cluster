# muduo-chat-cluster
基于 muduo 网络库的高并发集群聊天服务器，使用 Nginx TCP 负载均衡、Redis 发布订阅实现跨服务器通信，MySQL + 连接池持久化数据，JSON 私有协议。支持水平扩展和高性能。

## 特性

- **高性能网络层**：基于 `muduo` 网络库的 Reactor 模型，主从事件驱动，单机支持数万并发连接。
- **集群部署**：通过 Nginx TCP 负载均衡（轮询策略）分发客户端请求，支持水平扩展。
- **跨服务器通信**：利用 Redis 发布订阅功能，实现集群内消息广播与转发。
- **数据持久化**：MySQL 存储用户信息、好友关系、离线消息，并使用连接池提升数据库访问性能。
- **自定义通信协议**：采用 JSON 格式序列化消息，简洁易扩展，通过长度字段解决 TCP 粘包问题。
- **双模式线程池**：集成通用线程池（Fixed/Cached 模式），处理耗时业务，避免阻塞 I/O 线程。

- ## 技术栈

- **编程语言**：C++17
- **网络库**：[muduo](https://github.com/chenshuo/muduo)
- **负载均衡**：Nginx（TCP 反向代理，轮询策略）
- **中间件**：Redis（发布订阅）、MySQL
- **序列化**：JSON（nlohmann/json 或自定义）
- **构建工具**：CMake

**通信流程**：
1. 客户端通过 TCP 连接到 Nginx 集群入口。
2. Nginx 根据轮询策略将连接转发至某一台 muduo 服务器。
3. 服务器处理登录、注册、加好友、私聊/群聊等业务。
4. 若消息目标用户不在本服务器，通过 Redis 发布订阅转发到目标服务器。
5. 数据持久化使用 MySQL，连接池管理数据库连接。

## 快速开始

### 环境依赖

- Linux（推荐 Ubuntu 18.04+ 或 CentOS 7+）
- CMake 3.10+
- g++ 7.5+（支持 C++17）
- muduo 网络库
- Nginx（需编译 stream 模块）
- Redis 5.0+
- MySQL 5.7+
- JSON 库（如 nlohmann/json，已包含在 `thirdparty` 目录）

- ### 编译安装

```bash
git clone https://github.com/lww-77/muduo-chat-cluster.git
cd muduo-chat-cluster
./autobuild.sh   # 或者手动执行 cd ./build  && cmake .. && make

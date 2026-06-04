# ReactorHttpKit

## 当前可运行版本

项目现在已经补齐为一个可直接运行的 C++17 Reactor HTTP 库存订单系统演示版，包含：

- Reactor 网络核心：EventLoop、Channel、Epoll、Socket。
- TCP 服务层：Acceptor、TcpConnection、TcpServer、Buffer。
- HTTP 服务层：请求解析、响应封装、路由分发、静态资源访问。
- MySQL 数据库层：连接池、预处理语句、自动建表、事务提交/回滚。
- 库存订单 API：注册、登录、商品列表、商品详情、创建订单、查询订单、取消订单、管理概览，数据持久化到 MySQL。
- 前端展示界面：登录/注册、商品库存、下单、订单列表、取消订单、运行概览。

当前业务数据已经接入 MySQL。服务启动时会自动创建数据库、数据表和演示数据，也可以手动执行 `sql/schema.sql` 初始化。

### 快速运行

```bash
export RHK_DB_HOST=127.0.0.1
export RHK_DB_PORT=3306
export RHK_DB_USER=root
export RHK_DB_PASSWORD='你的 MySQL 密码'
export RHK_DB_NAME=reactor_http_kit
cmake -S . -B build
cmake --build build
./build/examples/reactor_shop_server 8080 public
```

浏览器打开：

```text
http://127.0.0.1:8080
```

内置演示账号：

```text
demo / demo
```

### 已实现接口

```text
GET  /api/health
POST /api/register
POST /api/login
GET  /api/products
GET  /api/products/{id}
POST /api/orders
GET  /api/orders?userId=1
GET  /api/orders/{id}
POST /api/orders/{id}/cancel
GET  /api/admin/summary
```

### 数据库关系模式

完整关系模式说明见：

```text
docs/RELATION-MODEL-CN.md
```

核心关系：

```text
users 1 --- N orders
orders 1 --- N order_items
products 1 --- N order_items
products 1 --- N inventory_logs
orders 0/1 --- N inventory_logs
```

核心表：

```text
users(id PK, username UNIQUE, password_hash, created_at)
products(id PK, name, description, price_cents, stock, created_at)
orders(id PK, user_id FK, status, total_cents, created_at, updated_at)
order_items(id PK, order_id FK, product_id FK, product_name, price_cents, quantity, total_cents)
inventory_logs(id PK, product_id FK, order_id, change_amount, reason, created_at)
```

## 项目定位

ReactorHttpKit 是一个基于 C++17 实现的高并发 HTTP 服务项目。项目底层采用 epoll-based Reactor 网络模型，封装事件循环、连接管理、定时器、HTTP 请求处理等能力，并在此基础上构建一个面向数据库课程设计的库存订单管理系统。

本项目的目标不是只做一个简单 CRUD，而是完成一个能够体现网络编程、MySQL 数据库设计、事务一致性、前后端交互和工程化能力的综合项目，可作为后端方向简历项目使用。

## 核心目标

1. 实现一个可运行的 C++ Reactor HTTP 服务框架。
2. 基于 MySQL 设计订单业务数据库，并通过后端接口完成数据读写。
3. 提供简单前端页面，支持用户下单、查询订单等交互。
4. 使用事务、索引、连接池等机制体现数据库课程设计要求。
5. 支持 Nginx 反向代理，并预留多实例部署、读写分离等扩展能力。

## 技术栈

- 编程语言：C++17
- 网络模型：主从 Reactor、epoll、非阻塞 I/O
- 构建工具：CMake
- 数据库：MySQL
- 前端：HTML、CSS、JavaScript
- 网关：Nginx
- 数据格式：HTTP、JSON
- 可选压测工具：wrk、ab

## 业务需求

项目业务采用库存订单管理系统，围绕用户、商品、库存和订单展开。

### 用户功能

- 用户注册
- 用户登录
- 查询商品列表
- 查看商品详情
- 创建订单
- 查询个人订单列表
- 查询订单详情
- 取消未完成订单

### 商品与库存功能

- 商品信息展示
- 商品库存查询
- 下单时扣减库存
- 取消订单时恢复库存
- 记录库存变更流水

### 订单功能

- 创建订单
- 查询订单
- 取消订单
- 订单状态流转
- 超时未支付订单自动取消

订单状态初步设计：

```text
CREATED    已创建
PAID       已支付
CANCELED   已取消
TIMEOUT    超时关闭
```

## 数据库设计需求

MySQL 部分是本项目的课程设计重点，需要体现表结构设计、事务控制、索引优化和并发一致性。

### 核心数据表

- users：用户表
- products：商品表
- orders：订单主表
- order_items：订单明细表
- inventory_logs：库存流水表

### 数据库能力要求

- 使用 MySQL 持久化用户、商品、订单和库存数据。
- 下单流程必须使用事务，保证订单创建和库存扣减的一致性。
- 使用行级锁或条件更新防止并发下单导致库存超卖。
- 为常用查询建立索引，例如用户订单查询、订单状态查询、商品查询。
- 后端 SQL 操作应使用预处理语句，避免 SQL 注入。
- 实现 MySQL 连接池，减少频繁创建和销毁连接的开销。

### 下单事务流程

```text
begin
  查询商品库存
  判断库存是否充足
  扣减商品库存
  创建订单记录
  创建订单明细
  写入库存流水
commit
```

如果任一步失败，需要执行 rollback。

## 后端架构需求

### Reactor 网络层

底层网络库需要逐步实现：

- EventLoop：事件循环
- Channel：文件描述符事件封装
- Epoll：epoll 封装
- Acceptor：新连接接收
- TcpConnection：连接生命周期管理
- Buffer：输入输出缓冲区
- Timer：定时器
- EventLoopThreadPool：主从 Reactor 线程模型

### HTTP 服务层

HTTP 模块需要实现：

- HTTP 请求解析
- HTTP 响应封装
- 路由分发
- JSON 请求和响应
- 静态资源访问
- 基础错误码处理

### 业务接口

初步接口设计：

```text
POST /api/register          用户注册
POST /api/login             用户登录
GET  /api/products          查询商品列表
GET  /api/products/{id}     查询商品详情
POST /api/orders            创建订单
GET  /api/orders            查询当前用户订单
GET  /api/orders/{id}       查询订单详情
POST /api/orders/{id}/cancel 取消订单
```

### 日志与异常处理

- 记录服务启动、连接建立、连接关闭等运行日志。
- 记录 HTTP 请求日志。
- 记录数据库操作失败、事务回滚等错误日志。
- 对非法请求、数据库异常、业务异常返回统一错误响应。

## 前端交互需求

前端页面不追求复杂视觉效果，重点是能完整演示数据库交互流程。

需要包含：

- 登录/注册页面
- 商品列表页面
- 商品详情或下单区域
- 我的订单页面
- 订单详情页面
- 简单管理页面，可查看商品库存和订单状态

前端通过 HTTP API 与 C++ 后端交互，后端再访问 MySQL 完成数据读写。

## Nginx 与部署需求

Nginx 作为网关层，用于反向代理后端服务。

第一阶段：

- Nginx 代理单个 C++ 后端实例。
- 前端静态文件可以由 Nginx 提供。

第二阶段：

- Nginx 代理多个后端实例。
- 通过 upstream 实现简单负载均衡。

第三阶段：

- 支持 MySQL 主从部署。
- 写请求访问主库。
- 读请求访问从库。
- 后端配置读写分离策略。

## 阶段规划

### 第一阶段：跑通 Reactor 核心

- 完成 EventLoop、Channel、Epoll。
- 使用 timerfd 验证事件循环和回调机制。
- 保证基础事件分发链路可运行。

当前已验证链路：

```text
timerfd -> Channel -> Epoll -> EventLoop -> callback
```

### 第二阶段：实现 TCP 与 HTTP 服务

- 实现 Acceptor 和 TcpConnection。
- 支持客户端连接、读写数据和连接关闭。
- 实现 HTTP 请求解析和响应返回。
- 提供简单路由功能。

### 第三阶段：接入 MySQL

- 设计数据库表结构。
- 编写建表 SQL 和测试数据。
- 实现 MySQL 连接封装。
- 实现 MySQL 连接池。
- 完成用户、商品、订单相关 DAO。

### 第四阶段：完成订单业务

- 实现注册、登录、商品查询、创建订单、订单查询和取消订单。
- 使用事务保证下单过程中的库存一致性。
- 使用定时器处理超时订单。
- 处理库存不足、订单不存在、重复取消等业务异常。

### 第五阶段：完成前端页面

- 编写基础 HTML、CSS、JavaScript 页面。
- 通过 fetch 调用后端接口。
- 展示商品、订单和库存数据。
- 完成从前端下单到 MySQL 写入的完整闭环。

### 第六阶段：工程化与增强

- 接入 Nginx 反向代理。
- 补充日志系统。
- 补充异常处理和统一响应格式。
- 进行接口测试和基础压测。
- 可选支持多后端实例和 MySQL 读写分离。

## 简历亮点

项目完成后可以总结为：

```text
基于 C++17 实现 epoll Reactor 高并发 HTTP 服务框架，并在其上构建库存订单管理系统。
项目支持用户下单、库存扣减、订单查询、超时取消、MySQL 事务一致性控制、连接池、Nginx 反向代理和前端交互。
```

可重点描述：

- 自研 epoll-based Reactor 网络模型，封装 EventLoop、Channel、Epoll、Timer 等核心组件。
- 基于 MySQL 事务实现下单扣库存流程，保证订单创建和库存扣减的一致性。
- 使用行级锁或条件更新解决并发下单场景下的库存超卖问题。
- 实现 MySQL 连接池，降低数据库连接创建开销。
- 使用 Nginx 作为反向代理，支持后端多实例部署扩展。
- 提供前端页面完成用户下单、订单查询和库存展示。

## 后续可选优化

- 支持 JWT 或 Session 登录态。
- 增加 Redis 缓存商品信息。
- 增加订单支付模拟流程。
- 增加后台管理端。
- 增加接口压测报告。
- 抽象事件分发层，探索 io_uring 后端实现。

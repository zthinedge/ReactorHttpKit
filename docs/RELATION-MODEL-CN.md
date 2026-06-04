# ReactorHttpKit 数据库关系模式

## 实体与关系

本项目采用库存订单管理系统作为业务模型，核心实体包括用户、商品、订单、订单明细和库存流水。

```text
users 1 --- N orders
orders 1 --- N order_items
products 1 --- N order_items
products 1 --- N inventory_logs
orders 0/1 --- N inventory_logs
```

## 关系模式

```text
users(
  id PK,
  username UNIQUE,
  password_hash,
  created_at
)

products(
  id PK,
  name,
  description,
  price_cents,
  stock,
  created_at
)

orders(
  id PK,
  user_id FK -> users.id,
  status,
  total_cents,
  created_at,
  updated_at
)

order_items(
  id PK,
  order_id FK -> orders.id,
  product_id FK -> products.id,
  product_name,
  price_cents,
  quantity,
  total_cents
)

inventory_logs(
  id PK,
  product_id FK -> products.id,
  order_id nullable,
  change_amount,
  reason,
  created_at
)
```

## 表设计说明

- `users`：用户表，`username` 建唯一索引，防止重复注册。
- `products`：商品表，保存商品展示信息、价格和实时库存。
- `orders`：订单主表，保存用户、订单状态、订单总金额和时间。
- `order_items`：订单明细表，保存下单时的商品名称、单价、数量和明细金额，避免商品改名后影响历史订单展示。
- `inventory_logs`：库存流水表，记录每次下单扣减和取消恢复库存的变化。

## 索引设计

```text
users.username UNIQUE
products.name
orders(user_id, created_at)
orders(status)
order_items(order_id)
inventory_logs(product_id, created_at)
```

这些索引用于支撑登录注册、商品查询、用户订单查询、订单状态查询和商品库存流水查询。

## 事务设计

创建订单事务：

```text
START TRANSACTION
  SELECT product FOR UPDATE
  判断库存是否充足
  UPDATE products SET stock = stock - quantity WHERE id = ? AND stock >= quantity
  INSERT orders
  INSERT order_items
  INSERT inventory_logs(change_amount = -quantity)
COMMIT
```

取消订单事务：

```text
START TRANSACTION
  SELECT order + order_items FOR UPDATE
  判断订单状态是否为 CREATED
  UPDATE orders SET status = CANCELED
  UPDATE products SET stock = stock + quantity
  INSERT inventory_logs(change_amount = quantity)
COMMIT
```

失败时执行 `ROLLBACK`。下单同时使用 `SELECT ... FOR UPDATE` 和条件扣减 `stock >= quantity`，防止并发下单导致库存超卖。

## SQL 文件

完整建表与演示数据脚本在：

```text
sql/schema.sql
```

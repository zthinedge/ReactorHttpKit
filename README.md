# ReactorHttpKit

ReactorHttpKit is a C++17 epoll Reactor HTTP demo project. It now includes:

- Reactor network primitives: `EventLoop`, `Channel`, `Epoll`, `Socket`
- TCP server layer: `Acceptor`, `TcpConnection`, `TcpServer`, `Buffer`
- HTTP layer: request parsing, response building, route dispatch, static files
- MySQL-backed inventory/order APIs with transactions and inventory logs
- A browser UI under `public/`

## Run

```bash
export RHK_DB_HOST=127.0.0.1
export RHK_DB_PORT=3306
export RHK_DB_USER=root
export RHK_DB_PASSWORD='your-password'
export RHK_DB_NAME=reactor_http_kit
cmake -S . -B build
cmake --build build
./build/examples/reactor_shop_server 8080 public
```

Open:

```text
http://127.0.0.1:8080
```

Demo account:

```text
demo / demo
```

## API

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

The server creates the schema automatically on startup. You can also initialize it manually with `sql/schema.sql`.

Database design notes are in `docs/RELATION-MODEL-CN.md`.

#include "db/MySql.h"
#include "http/HttpServer.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"

#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

using reactor_http_kit::db::MySqlConfig;
using reactor_http_kit::db::MySqlConnection;
using reactor_http_kit::db::MySqlPool;
using reactor_http_kit::db::MySqlRow;
using reactor_http_kit::db::initializeDatabase;
using reactor_http_kit::http::HttpRequest;
using reactor_http_kit::http::HttpResponse;
using reactor_http_kit::http::HttpServer;
using reactor_http_kit::net::EventLoop;
using reactor_http_kit::net::InetAddress;

namespace
{

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value)
    {
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

std::string jsonString(const std::string& value)
{
    return "\"" + jsonEscape(value) + "\"";
}

std::string readJsonString(const std::string& body, const std::string& key)
{
    std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    return std::regex_search(body, match, pattern) ? match[1].str() : "";
}

int readJsonInt(const std::string& body, const std::string& key, int fallback = 0)
{
    std::regex pattern("\"" + key + "\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    return std::regex_search(body, match, pattern) ? std::stoi(match[1].str()) : fallback;
}

std::string ok(const std::string& payload)
{
    return "{\"ok\":true," + payload + "}";
}

HttpResponse apiOk(const std::string& payload)
{
    return HttpResponse::json(200, ok(payload));
}

HttpResponse apiCreated(const std::string& payload)
{
    return HttpResponse::json(201, ok(payload));
}

HttpResponse apiError(int status, const std::string& error)
{
    return HttpResponse::json(status, "{\"ok\":false,\"error\":" + jsonString(error) + "}");
}

int toInt(const MySqlRow& row, const std::string& key)
{
    auto it = row.find(key);
    return it == row.end() || it->second.empty() ? 0 : std::stoi(it->second);
}

std::string valueOf(const MySqlRow& row, const std::string& key)
{
    auto it = row.find(key);
    return it == row.end() ? "" : it->second;
}

std::string userJson(const MySqlRow& row)
{
    return "{\"id\":" + std::to_string(toInt(row, "id")) + ",\"username\":" + jsonString(valueOf(row, "username")) + "}";
}

std::string productJson(const MySqlRow& row)
{
    std::ostringstream stream;
    stream << "{"
           << "\"id\":" << toInt(row, "id") << ","
           << "\"name\":" << jsonString(valueOf(row, "name")) << ","
           << "\"description\":" << jsonString(valueOf(row, "description")) << ","
           << "\"priceCents\":" << toInt(row, "price_cents") << ","
           << "\"stock\":" << toInt(row, "stock")
           << "}";
    return stream.str();
}

std::string orderJson(const MySqlRow& row)
{
    std::ostringstream stream;
    stream << "{"
           << "\"id\":" << toInt(row, "id") << ","
           << "\"userId\":" << toInt(row, "user_id") << ","
           << "\"username\":" << jsonString(valueOf(row, "username")) << ","
           << "\"productId\":" << toInt(row, "product_id") << ","
           << "\"productName\":" << jsonString(valueOf(row, "product_name")) << ","
           << "\"quantity\":" << toInt(row, "quantity") << ","
           << "\"totalCents\":" << toInt(row, "total_cents") << ","
           << "\"status\":" << jsonString(valueOf(row, "status")) << ","
           << "\"createdAt\":" << jsonString(valueOf(row, "created_at"))
           << "}";
    return stream.str();
}

class Transaction
{
public:
    explicit Transaction(MySqlConnection& connection)
        : connection_(connection)
    {
        connection_.begin();
    }

    ~Transaction()
    {
        if (!done_)
        {
            connection_.rollback();
        }
    }

    void commit()
    {
        connection_.commit();
        done_ = true;
    }

private:
    MySqlConnection& connection_;
    bool done_ { false };
};

class ShopStore
{
public:
    explicit ShopStore(MySqlConfig config)
        : config_(std::move(config))
    {
        initializeDatabase(config_);
        pool_ = std::make_unique<MySqlPool>(config_);
    }

    HttpResponse health()
    {
        auto connection = pool_->acquire();
        connection->queryPrepared("SELECT 1 AS ok");
        return apiOk(R"("service":"ReactorHttpKit","status":"running","database":"mysql")");
    }

    HttpResponse registerUser(const HttpRequest& request)
    {
        std::string username = readJsonString(request.body, "username");
        std::string password = readJsonString(request.body, "password");
        if (username.empty() || password.empty())
        {
            return apiError(400, "用户名和密码不能为空");
        }

        auto connection = pool_->acquire();
        auto exists = connection->queryPrepared("SELECT id FROM users WHERE username = ?", { username });
        if (!exists.empty())
        {
            return apiError(409, "用户已存在");
        }

        auto result = connection->executePrepared(
            "INSERT INTO users (username, password_hash) VALUES (?, SHA2(?, 256))",
            { username, password });
        auto rows = connection->queryPrepared("SELECT id, username FROM users WHERE id = ?",
                                             { std::to_string(result.insertId) });
        return apiCreated("\"user\":" + userJson(rows.front()));
    }

    HttpResponse login(const HttpRequest& request)
    {
        std::string username = readJsonString(request.body, "username");
        std::string password = readJsonString(request.body, "password");

        auto connection = pool_->acquire();
        auto rows = connection->queryPrepared(
            "SELECT id, username FROM users WHERE username = ? AND password_hash = SHA2(?, 256)",
            { username, password });
        if (rows.empty())
        {
            return apiError(401, "用户名或密码错误");
        }
        return apiOk("\"user\":" + userJson(rows.front()));
    }

    HttpResponse products()
    {
        auto connection = pool_->acquire();
        auto rows = connection->queryPrepared(
            "SELECT id, name, description, price_cents, stock FROM products ORDER BY id");
        std::string body = "\"products\":[";
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (i > 0)
            {
                body += ",";
            }
            body += productJson(rows[i]);
        }
        body += "]";
        return apiOk(body);
    }

    HttpResponse product(const HttpRequest& request)
    {
        int id = std::stoi(request.pathParams.at("id"));
        auto connection = pool_->acquire();
        auto rows = connection->queryPrepared(
            "SELECT id, name, description, price_cents, stock FROM products WHERE id = ?",
            { std::to_string(id) });
        if (rows.empty())
        {
            return apiError(404, "商品不存在");
        }
        return apiOk("\"product\":" + productJson(rows.front()));
    }

    HttpResponse createOrder(const HttpRequest& request)
    {
        int userId = readJsonInt(request.body, "userId");
        int productId = readJsonInt(request.body, "productId");
        int quantity = readJsonInt(request.body, "quantity", 1);
        if (quantity <= 0)
        {
            return apiError(400, "数量必须大于 0");
        }

        auto connection = pool_->acquire();
        Transaction tx(*connection);

        auto users = connection->queryPrepared(
            "SELECT id, username FROM users WHERE id = ?",
            { std::to_string(userId) });
        if (users.empty())
        {
            return apiError(401, "请先登录");
        }

        auto products = connection->queryPrepared(
            "SELECT id, name, price_cents, stock FROM products WHERE id = ? FOR UPDATE",
            { std::to_string(productId) });
        if (products.empty())
        {
            return apiError(404, "商品不存在");
        }

        const MySqlRow& product = products.front();
        int stock = toInt(product, "stock");
        int priceCents = toInt(product, "price_cents");
        if (stock < quantity)
        {
            return apiError(409, "库存不足");
        }

        auto update = connection->executePrepared(
            "UPDATE products SET stock = stock - ? WHERE id = ? AND stock >= ?",
            { std::to_string(quantity), std::to_string(productId), std::to_string(quantity) });
        if (update.affectedRows != 1)
        {
            return apiError(409, "库存不足");
        }

        int totalCents = priceCents * quantity;
        auto orderResult = connection->executePrepared(
            "INSERT INTO orders (user_id, status, total_cents) VALUES (?, 'CREATED', ?)",
            { std::to_string(userId), std::to_string(totalCents) });
        std::string orderId = std::to_string(orderResult.insertId);

        connection->executePrepared(
            "INSERT INTO order_items (order_id, product_id, product_name, price_cents, quantity, total_cents) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            { orderId, std::to_string(productId), valueOf(product, "name"), std::to_string(priceCents),
              std::to_string(quantity), std::to_string(totalCents) });
        connection->executePrepared(
            "INSERT INTO inventory_logs (product_id, order_id, change_amount, reason) VALUES (?, ?, ?, 'CREATE_ORDER')",
            { std::to_string(productId), orderId, std::to_string(-quantity) });

        tx.commit();
        auto rows = selectOrder(*connection, orderResult.insertId);
        return apiCreated("\"order\":" + orderJson(rows.front()));
    }

    HttpResponse orders(const HttpRequest& request)
    {
        auto connection = pool_->acquire();
        std::string sql = orderSelectSql();
        std::vector<std::string> params;
        auto it = request.query.find("userId");
        if (it != request.query.end() && !it->second.empty())
        {
            sql += " WHERE o.user_id = ?";
            params.push_back(it->second);
        }
        sql += " ORDER BY o.created_at DESC, o.id DESC";

        auto rows = connection->queryPrepared(sql, params);
        std::string body = "\"orders\":[";
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (i > 0)
            {
                body += ",";
            }
            body += orderJson(rows[i]);
        }
        body += "]";
        return apiOk(body);
    }

    HttpResponse order(const HttpRequest& request)
    {
        int id = std::stoi(request.pathParams.at("id"));
        auto connection = pool_->acquire();
        auto rows = selectOrder(*connection, id);
        if (rows.empty())
        {
            return apiError(404, "订单不存在");
        }
        return apiOk("\"order\":" + orderJson(rows.front()));
    }

    HttpResponse cancelOrder(const HttpRequest& request)
    {
        int id = std::stoi(request.pathParams.at("id"));
        auto connection = pool_->acquire();
        Transaction tx(*connection);

        auto rows = connection->queryPrepared(
            orderSelectSql() + " WHERE o.id = ? FOR UPDATE",
            { std::to_string(id) });
        if (rows.empty())
        {
            return apiError(404, "订单不存在");
        }

        const MySqlRow& order = rows.front();
        if (valueOf(order, "status") != "CREATED")
        {
            return apiError(409, "只有已创建订单可以取消");
        }

        connection->executePrepared(
            "UPDATE orders SET status = 'CANCELED' WHERE id = ? AND status = 'CREATED'",
            { std::to_string(id) });
        connection->executePrepared(
            "UPDATE products SET stock = stock + ? WHERE id = ?",
            { valueOf(order, "quantity"), valueOf(order, "product_id") });
        connection->executePrepared(
            "INSERT INTO inventory_logs (product_id, order_id, change_amount, reason) VALUES (?, ?, ?, 'CANCEL_ORDER')",
            { valueOf(order, "product_id"), std::to_string(id), valueOf(order, "quantity") });

        tx.commit();
        auto updated = selectOrder(*connection, id);
        return apiOk("\"order\":" + orderJson(updated.front()));
    }

    HttpResponse summary()
    {
        auto connection = pool_->acquire();
        int users = scalar(*connection, "SELECT COUNT(*) AS value FROM users");
        int products = scalar(*connection, "SELECT COUNT(*) AS value FROM products");
        int orders = scalar(*connection, "SELECT COUNT(*) AS value FROM orders");
        int stock = scalar(*connection, "SELECT COALESCE(SUM(stock), 0) AS value FROM products");

        std::ostringstream body;
        body << "\"summary\":{"
             << "\"users\":" << users << ","
             << "\"products\":" << products << ","
             << "\"orders\":" << orders << ","
             << "\"stock\":" << stock << "}";
        return apiOk(body.str());
    }

private:
    static int scalar(MySqlConnection& connection, const std::string& sql)
    {
        auto rows = connection.queryPrepared(sql);
        return rows.empty() ? 0 : toInt(rows.front(), "value");
    }

    static std::string orderSelectSql()
    {
        return "SELECT o.id, o.user_id, u.username, oi.product_id, oi.product_name, oi.quantity, "
               "o.total_cents, o.status, DATE_FORMAT(o.created_at, '%Y-%m-%d %H:%i:%s') AS created_at "
               "FROM orders o "
               "JOIN users u ON u.id = o.user_id "
               "JOIN order_items oi ON oi.order_id = o.id";
    }

    static reactor_http_kit::db::MySqlRows selectOrder(MySqlConnection& connection, uint64_t id)
    {
        return connection.queryPrepared(orderSelectSql() + " WHERE o.id = ?", { std::to_string(id) });
    }

private:
    MySqlConfig config_;
    std::unique_ptr<MySqlPool> pool_;
};

} // namespace

int main(int argc, char* argv[])
{
    uint16_t port = 8080;
    std::string staticRoot = "public";
    if (argc >= 2)
    {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }
    if (argc >= 3)
    {
        staticRoot = argv[2];
    }

    MySqlConfig dbConfig = MySqlConfig::fromEnv();
    std::cout << "MySQL: " << dbConfig.toLogString() << std::endl;

    try
    {
        EventLoop loop;
        HttpServer server(&loop, InetAddress("0.0.0.0", port), "reactor_shop");
        ShopStore store(dbConfig);

        server.addRoute("GET", "/api/health", [&](const HttpRequest&) { return store.health(); });
        server.addRoute("POST", "/api/register", [&](const HttpRequest& req) { return store.registerUser(req); });
        server.addRoute("POST", "/api/login", [&](const HttpRequest& req) { return store.login(req); });
        server.addRoute("GET", "/api/products", [&](const HttpRequest&) { return store.products(); });
        server.addRoute("GET", "/api/products/{id}", [&](const HttpRequest& req) { return store.product(req); });
        server.addRoute("POST", "/api/orders", [&](const HttpRequest& req) { return store.createOrder(req); });
        server.addRoute("GET", "/api/orders", [&](const HttpRequest& req) { return store.orders(req); });
        server.addRoute("GET", "/api/orders/{id}", [&](const HttpRequest& req) { return store.order(req); });
        server.addRoute("POST", "/api/orders/{id}/cancel", [&](const HttpRequest& req) { return store.cancelOrder(req); });
        server.addRoute("GET", "/api/admin/summary", [&](const HttpRequest&) { return store.summary(); });
        server.setStaticRoot(staticRoot);

        server.start();
        std::cout << "ReactorHttpKit server started: http://127.0.0.1:" << port << std::endl;
        std::cout << "Demo account: demo / demo" << std::endl;
        loop.loop();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Failed to start ReactorHttpKit: " << ex.what() << std::endl;
        std::cerr << "Please set RHK_DB_HOST/RHK_DB_PORT/RHK_DB_USER/RHK_DB_PASSWORD/RHK_DB_NAME." << std::endl;
        return 1;
    }
}

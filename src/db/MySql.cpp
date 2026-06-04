#include "db/MySql.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace reactor_http_kit::db
{

namespace
{

std::string envOr(const char* name, const std::string& fallback)
{
    const char* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? fallback : value;
}

uint16_t envPort(const char* name, uint16_t fallback)
{
    const char* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? fallback : static_cast<uint16_t>(std::stoi(value));
}

size_t envSize(const char* name, size_t fallback)
{
    const char* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? fallback : static_cast<size_t>(std::stoul(value));
}

std::string quoteIdentifier(const std::string& identifier)
{
    if (identifier.empty())
    {
        throw std::invalid_argument("empty MySQL identifier");
    }
    for (char ch : identifier)
    {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_')
        {
            throw std::invalid_argument("unsafe MySQL identifier: " + identifier);
        }
    }
    return "`" + identifier + "`";
}

std::string mysqlError(MYSQL* mysql, const std::string& operation)
{
    return operation + " failed: " + (mysql == nullptr ? "mysql handle is null" : mysql_error(mysql));
}

std::string stmtError(MYSQL_STMT* stmt, const std::string& operation)
{
    return operation + " failed: " + (stmt == nullptr ? "stmt handle is null" : mysql_stmt_error(stmt));
}

void bindParams(MYSQL_STMT* stmt, const MySqlParams& params, std::vector<MYSQL_BIND>& binds,
                std::vector<unsigned long>& lengths)
{
    if (params.empty())
    {
        return;
    }

    unsigned long expected = mysql_stmt_param_count(stmt);
    if (expected != params.size())
    {
        throw std::runtime_error("prepared parameter count mismatch");
    }

    binds.resize(params.size());
    lengths.resize(params.size());
    for (size_t i = 0; i < params.size(); ++i)
    {
        std::memset(&binds[i], 0, sizeof(MYSQL_BIND));
        lengths[i] = static_cast<unsigned long>(params[i].size());
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = const_cast<char*>(params[i].data());
        binds[i].buffer_length = lengths[i];
        binds[i].length = &lengths[i];
    }

    if (mysql_stmt_bind_param(stmt, binds.data()) != 0)
    {
        throw std::runtime_error(stmtError(stmt, "mysql_stmt_bind_param()"));
    }
}

} // namespace

MySqlConfig MySqlConfig::fromEnv()
{
    MySqlConfig config;
    config.host = envOr("RHK_DB_HOST", config.host);
    config.user = envOr("RHK_DB_USER", config.user);
    config.password = envOr("RHK_DB_PASSWORD", config.password);
    config.database = envOr("RHK_DB_NAME", config.database);
    config.port = envPort("RHK_DB_PORT", config.port);
    config.poolSize = std::max<size_t>(1, envSize("RHK_DB_POOL_SIZE", config.poolSize));
    return config;
}

std::string MySqlConfig::toLogString() const
{
    return user + "@tcp(" + host + ":" + std::to_string(port) + ")/" + database
        + ", pool=" + std::to_string(poolSize);
}

MySqlConnection::MySqlConnection()
    : mysql_(mysql_init(nullptr))
{
    if (mysql_ == nullptr)
    {
        throw std::runtime_error("mysql_init() failed");
    }
}

MySqlConnection::~MySqlConnection()
{
    if (mysql_ != nullptr)
    {
        mysql_close(mysql_);
    }
}

void MySqlConnection::connect(const MySqlConfig& config, bool selectDatabase)
{
    unsigned int reconnect = 1;
    mysql_options(mysql_, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    const char* database = selectDatabase ? config.database.c_str() : nullptr;
    if (mysql_real_connect(mysql_, config.host.c_str(), config.user.c_str(), config.password.c_str(),
                           database, config.port, nullptr, CLIENT_MULTI_STATEMENTS) == nullptr)
    {
        throw std::runtime_error(mysqlError(mysql_, "mysql_real_connect()"));
    }
    execute("SET NAMES utf8mb4");
}

void MySqlConnection::execute(const std::string& sql)
{
    if (mysql_query(mysql_, sql.c_str()) != 0)
    {
        throw std::runtime_error(mysqlError(mysql_, sql));
    }
    while (mysql_next_result(mysql_) == 0)
    {
        MYSQL_RES* extra = mysql_store_result(mysql_);
        if (extra != nullptr)
        {
            mysql_free_result(extra);
        }
    }
}

MySqlRows MySqlConnection::query(const std::string& sql)
{
    if (mysql_query(mysql_, sql.c_str()) != 0)
    {
        throw std::runtime_error(mysqlError(mysql_, sql));
    }

    MYSQL_RES* result = mysql_store_result(mysql_);
    if (result == nullptr)
    {
        if (mysql_field_count(mysql_) == 0)
        {
            return {};
        }
        throw std::runtime_error(mysqlError(mysql_, sql));
    }

    int fieldCount = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    MySqlRows rows;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr)
    {
        unsigned long* lengths = mysql_fetch_lengths(result);
        MySqlRow mapped;
        for (int i = 0; i < fieldCount; ++i)
        {
            mapped[fields[i].name] = row[i] == nullptr ? "" : std::string(row[i], lengths[i]);
        }
        rows.push_back(std::move(mapped));
    }
    mysql_free_result(result);
    return rows;
}

MySqlExecResult MySqlConnection::executePrepared(const std::string& sql, const MySqlParams& params)
{
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr)
    {
        throw std::runtime_error(mysqlError(mysql_, "mysql_stmt_init()"));
    }

    try
    {
        if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0)
        {
            throw std::runtime_error(stmtError(stmt, sql));
        }

        std::vector<MYSQL_BIND> paramBinds;
        std::vector<unsigned long> paramLengths;
        bindParams(stmt, params, paramBinds, paramLengths);

        if (mysql_stmt_execute(stmt) != 0)
        {
            throw std::runtime_error(stmtError(stmt, sql));
        }

        MySqlExecResult result;
        result.insertId = mysql_stmt_insert_id(stmt);
        result.affectedRows = mysql_stmt_affected_rows(stmt);
        mysql_stmt_close(stmt);
        return result;
    }
    catch (...)
    {
        mysql_stmt_close(stmt);
        throw;
    }
}

MySqlRows MySqlConnection::queryPrepared(const std::string& sql, const MySqlParams& params)
{
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr)
    {
        throw std::runtime_error(mysqlError(mysql_, "mysql_stmt_init()"));
    }

    try
    {
        my_bool updateMaxLength = 1;
        mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &updateMaxLength);
        if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0)
        {
            throw std::runtime_error(stmtError(stmt, sql));
        }

        std::vector<MYSQL_BIND> paramBinds;
        std::vector<unsigned long> paramLengths;
        bindParams(stmt, params, paramBinds, paramLengths);

        if (mysql_stmt_execute(stmt) != 0)
        {
            throw std::runtime_error(stmtError(stmt, sql));
        }

        MYSQL_RES* metadata = mysql_stmt_result_metadata(stmt);
        if (metadata == nullptr)
        {
            mysql_stmt_close(stmt);
            return {};
        }

        if (mysql_stmt_store_result(stmt) != 0)
        {
            mysql_free_result(metadata);
            throw std::runtime_error(stmtError(stmt, "mysql_stmt_store_result()"));
        }

        unsigned int fieldCount = mysql_num_fields(metadata);
        MYSQL_FIELD* fields = mysql_fetch_fields(metadata);
        std::vector<MYSQL_BIND> resultBinds(fieldCount);
        std::vector<std::vector<char>> buffers(fieldCount);
        std::vector<unsigned long> lengths(fieldCount);
        std::vector<my_bool> isNull(fieldCount);
        std::vector<my_bool> errors(fieldCount);

        for (unsigned int i = 0; i < fieldCount; ++i)
        {
            unsigned long size = std::max<unsigned long>(fields[i].max_length + 1, 256);
            buffers[i].assign(size, '\0');
            std::memset(&resultBinds[i], 0, sizeof(MYSQL_BIND));
            resultBinds[i].buffer_type = MYSQL_TYPE_STRING;
            resultBinds[i].buffer = buffers[i].data();
            resultBinds[i].buffer_length = static_cast<unsigned long>(buffers[i].size());
            resultBinds[i].length = &lengths[i];
            resultBinds[i].is_null = &isNull[i];
            resultBinds[i].error = &errors[i];
        }

        if (mysql_stmt_bind_result(stmt, resultBinds.data()) != 0)
        {
            mysql_free_result(metadata);
            throw std::runtime_error(stmtError(stmt, "mysql_stmt_bind_result()"));
        }

        MySqlRows rows;
        while (true)
        {
            int status = mysql_stmt_fetch(stmt);
            if (status == MYSQL_NO_DATA)
            {
                break;
            }
            if (status == 1)
            {
                mysql_free_result(metadata);
                throw std::runtime_error(stmtError(stmt, "mysql_stmt_fetch()"));
            }

            MySqlRow row;
            for (unsigned int i = 0; i < fieldCount; ++i)
            {
                row[fields[i].name] = isNull[i] ? "" : std::string(buffers[i].data(), lengths[i]);
            }
            rows.push_back(std::move(row));
        }

        mysql_free_result(metadata);
        mysql_stmt_close(stmt);
        return rows;
    }
    catch (...)
    {
        mysql_stmt_close(stmt);
        throw;
    }
}

std::string MySqlConnection::escape(const std::string& value)
{
    std::string escaped(value.size() * 2 + 1, '\0');
    unsigned long length = mysql_real_escape_string(mysql_, escaped.data(), value.data(), value.size());
    escaped.resize(length);
    return escaped;
}

void MySqlConnection::begin()
{
    execute("START TRANSACTION");
}

void MySqlConnection::commit()
{
    execute("COMMIT");
}

void MySqlConnection::rollback()
{
    execute("ROLLBACK");
}

uint64_t MySqlConnection::insertId() const
{
    return mysql_insert_id(mysql_);
}

uint64_t MySqlConnection::affectedRows() const
{
    return mysql_affected_rows(mysql_);
}

MySqlPool::Lease::Lease(MySqlPool* pool, MySqlConnection* connection)
    : pool_(pool)
    , connection_(connection)
{
}

MySqlPool::Lease::~Lease()
{
    release();
}

MySqlPool::Lease::Lease(Lease&& other) noexcept
    : pool_(other.pool_)
    , connection_(other.connection_)
{
    other.pool_ = nullptr;
    other.connection_ = nullptr;
}

MySqlPool::Lease& MySqlPool::Lease::operator=(Lease&& other) noexcept
{
    if (this != &other)
    {
        release();
        pool_ = other.pool_;
        connection_ = other.connection_;
        other.pool_ = nullptr;
        other.connection_ = nullptr;
    }
    return *this;
}

MySqlConnection* MySqlPool::Lease::operator->()
{
    return connection_;
}

MySqlConnection& MySqlPool::Lease::operator*()
{
    return *connection_;
}

bool MySqlPool::Lease::valid() const
{
    return connection_ != nullptr;
}

void MySqlPool::Lease::release()
{
    if (pool_ != nullptr && connection_ != nullptr)
    {
        pool_->release(connection_);
        pool_ = nullptr;
        connection_ = nullptr;
    }
}

MySqlPool::MySqlPool(MySqlConfig config)
    : config_(std::move(config))
{
    for (size_t i = 0; i < config_.poolSize; ++i)
    {
        auto connection = std::make_unique<MySqlConnection>();
        connection->connect(config_);
        idle_.push(connection.get());
        connections_.push_back(std::move(connection));
    }
}

MySqlPool::Lease MySqlPool::acquire()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (idle_.empty())
    {
        auto connection = std::make_unique<MySqlConnection>();
        connection->connect(config_);
        MySqlConnection* raw = connection.get();
        connections_.push_back(std::move(connection));
        return Lease(this, raw);
    }

    MySqlConnection* connection = idle_.front();
    idle_.pop();
    return Lease(this, connection);
}

void MySqlPool::release(MySqlConnection* connection)
{
    std::lock_guard<std::mutex> lock(mutex_);
    idle_.push(connection);
}

void initializeDatabase(const MySqlConfig& config)
{
    MySqlConnection connection;
    connection.connect(config, false);
    const std::string database = quoteIdentifier(config.database);
    connection.execute("CREATE DATABASE IF NOT EXISTS " + database + " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");
    connection.execute("USE " + database);
    connection.execute(R"SQL(
CREATE TABLE IF NOT EXISTS users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
)SQL");
    connection.execute(R"SQL(
CREATE TABLE IF NOT EXISTS products (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  name VARCHAR(128) NOT NULL,
  description VARCHAR(512) NOT NULL,
  price_cents INT NOT NULL,
  stock INT NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_products_name (name),
  CHECK (price_cents >= 0),
  CHECK (stock >= 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
)SQL");
    connection.execute(R"SQL(
CREATE TABLE IF NOT EXISTS orders (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  status ENUM('CREATED','PAID','CANCELED','TIMEOUT') NOT NULL DEFAULT 'CREATED',
  total_cents INT NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_orders_user_created (user_id, created_at),
  INDEX idx_orders_status (status),
  CONSTRAINT fk_orders_user FOREIGN KEY (user_id) REFERENCES users(id)
) ENGINE=InnoDB AUTO_INCREMENT=1001 DEFAULT CHARSET=utf8mb4
)SQL");
    connection.execute(R"SQL(
CREATE TABLE IF NOT EXISTS order_items (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  order_id BIGINT NOT NULL,
  product_id BIGINT NOT NULL,
  product_name VARCHAR(128) NOT NULL,
  price_cents INT NOT NULL,
  quantity INT NOT NULL,
  total_cents INT NOT NULL,
  INDEX idx_order_items_order (order_id),
  CONSTRAINT fk_order_items_order FOREIGN KEY (order_id) REFERENCES orders(id),
  CONSTRAINT fk_order_items_product FOREIGN KEY (product_id) REFERENCES products(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
)SQL");
    connection.execute(R"SQL(
CREATE TABLE IF NOT EXISTS inventory_logs (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  product_id BIGINT NOT NULL,
  order_id BIGINT NULL,
  change_amount INT NOT NULL,
  reason VARCHAR(64) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_inventory_logs_product_created (product_id, created_at),
  CONSTRAINT fk_inventory_logs_product FOREIGN KEY (product_id) REFERENCES products(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
)SQL");
    connection.execute(R"SQL(
INSERT IGNORE INTO users (id, username, password_hash) VALUES
  (1, 'demo', SHA2('demo', 256))
)SQL");
    connection.execute(R"SQL(
INSERT IGNORE INTO products (id, name, description, price_cents, stock) VALUES
  (1, 'Reactor 编程手册', '从 epoll 到 HTTP 服务的 C++ 实战资料包。', 8900, 18),
  (2, '高并发压测券', '用于演示 wrk/ab 压测场景的虚拟商品。', 3900, 42),
  (3, '库存事务演示套件', '面向数据库课程设计的订单一致性案例。', 12900, 8),
  (4, 'Nginx 部署服务包', '反向代理、多实例和静态资源部署演示。', 6900, 16)
)SQL");
}

} // namespace reactor_http_kit::db

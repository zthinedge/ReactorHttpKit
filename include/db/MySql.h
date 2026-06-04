#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <mysql/mysql.h>
#include <queue>
#include <string>
#include <vector>

namespace reactor_http_kit::db
{

using MySqlRow = std::map<std::string, std::string>;
using MySqlRows = std::vector<MySqlRow>;
using MySqlParams = std::vector<std::string>;

struct MySqlExecResult
{
    uint64_t insertId { 0 };
    uint64_t affectedRows { 0 };
};

struct MySqlConfig
{
    std::string host { "127.0.0.1" };
    std::string user { "root" };
    std::string password;
    std::string database { "reactor_http_kit" };
    uint16_t port { 3306 };
    size_t poolSize { 4 };

    static MySqlConfig fromEnv();
    std::string toLogString() const;
};

class MySqlConnection
{
public:
    MySqlConnection();
    ~MySqlConnection();

    MySqlConnection(const MySqlConnection&) = delete;
    MySqlConnection& operator=(const MySqlConnection&) = delete;

    void connect(const MySqlConfig& config, bool selectDatabase = true);
    void execute(const std::string& sql);
    MySqlRows query(const std::string& sql);
    MySqlExecResult executePrepared(const std::string& sql, const MySqlParams& params = {});
    MySqlRows queryPrepared(const std::string& sql, const MySqlParams& params = {});
    std::string escape(const std::string& value);

    void begin();
    void commit();
    void rollback();

    uint64_t insertId() const;
    uint64_t affectedRows() const;

private:
    MYSQL* mysql_ { nullptr };
};

class MySqlPool
{
public:
    class Lease
    {
    public:
        Lease() = default;
        Lease(MySqlPool* pool, MySqlConnection* connection);
        ~Lease();

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;

        MySqlConnection* operator->();
        MySqlConnection& operator*();
        bool valid() const;

    private:
        void release();

    private:
        MySqlPool* pool_ { nullptr };
        MySqlConnection* connection_ { nullptr };
    };

    explicit MySqlPool(MySqlConfig config);

    MySqlPool(const MySqlPool&) = delete;
    MySqlPool& operator=(const MySqlPool&) = delete;

    Lease acquire();

private:
    friend class Lease;
    void release(MySqlConnection* connection);

private:
    MySqlConfig config_;
    std::vector<std::unique_ptr<MySqlConnection>> connections_;
    std::queue<MySqlConnection*> idle_;
    std::mutex mutex_;
};

void initializeDatabase(const MySqlConfig& config);

} // namespace reactor_http_kit::db

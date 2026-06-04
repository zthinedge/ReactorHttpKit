#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "net/InetAddress.h"
#include "net/TcpServer.h"

#include <functional>
#include <string>
#include <vector>

namespace reactor_http_kit::net
{
class EventLoop;
}

namespace reactor_http_kit::http
{

class HttpServer
{
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    HttpServer(net::EventLoop* loop, const net::InetAddress& listenAddress, std::string name);

    void addRoute(std::string method, std::string pattern, Handler handler);
    void setStaticRoot(std::string staticRoot);
    void start();

private:
    struct Route
    {
        std::string method;
        std::string pattern;
        Handler handler;
    };

    void onMessage(const net::TcpConnectionPtr& connection, net::Buffer& buffer);
    HttpResponse dispatch(HttpRequest request) const;
    HttpResponse serveStatic(const std::string& requestPath) const;

    static bool matchRoute(const std::string& pattern, const std::string& path,
                           std::map<std::string, std::string>& params);
    static std::string contentTypeFor(const std::string& path);

private:
    net::TcpServer server_;
    std::vector<Route> routes_;
    std::string staticRoot_;
};

} // namespace reactor_http_kit::http

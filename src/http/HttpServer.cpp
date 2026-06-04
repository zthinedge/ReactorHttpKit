#include "http/HttpServer.h"

#include "net/Buffer.h"
#include "net/EventLoop.h"
#include "net/TcpConnection.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace reactor_http_kit::http
{

namespace
{

std::vector<std::string> splitPath(const std::string& path)
{
    std::vector<std::string> parts;
    std::stringstream stream(path);
    std::string item;
    while (std::getline(stream, item, '/'))
    {
        if (!item.empty())
        {
            parts.push_back(item);
        }
    }
    return parts;
}

} // namespace

HttpServer::HttpServer(net::EventLoop* loop, const net::InetAddress& listenAddress, std::string name)
    : server_(loop, listenAddress, std::move(name))
{
    server_.setMessageCallback([this](const net::TcpConnectionPtr& connection, net::Buffer& buffer) {
        onMessage(connection, buffer);
    });
}

void HttpServer::addRoute(std::string method, std::string pattern, Handler handler)
{
    routes_.push_back(Route { std::move(method), std::move(pattern), std::move(handler) });
}

void HttpServer::setStaticRoot(std::string staticRoot)
{
    staticRoot_ = std::move(staticRoot);
}

void HttpServer::start()
{
    server_.start();
}

void HttpServer::onMessage(const net::TcpConnectionPtr& connection, net::Buffer& buffer)
{
    HttpRequest request;
    size_t consumed = 0;
    if (!tryParseHttpRequest(buffer.toString(), request, consumed))
    {
        return;
    }
    buffer.retrieve(consumed);

    HttpResponse response;
    try
    {
        response = dispatch(std::move(request));
    }
    catch (const std::exception& ex)
    {
        std::cerr << "request failed: " << ex.what() << std::endl;
        response = HttpResponse::json(500, R"({"ok":false,"error":"internal server error"})");
    }

    connection->send(response.toString());
    connection->closeAfterWrite();
}

HttpResponse HttpServer::dispatch(HttpRequest request) const
{
    std::cout << request.method << ' ' << request.path << std::endl;
    for (const auto& route : routes_)
    {
        if (route.method != request.method)
        {
            continue;
        }
        std::map<std::string, std::string> params;
        if (matchRoute(route.pattern, request.path, params))
        {
            request.pathParams = std::move(params);
            return route.handler(request);
        }
    }

    if (request.method == "GET")
    {
        return serveStatic(request.path);
    }
    return HttpResponse::json(404, R"({"ok":false,"error":"route not found"})");
}

HttpResponse HttpServer::serveStatic(const std::string& requestPath) const
{
    if (staticRoot_.empty())
    {
        return HttpResponse::text(404, "not found");
    }

    std::string cleanPath = requestPath == "/" ? "/index.html" : requestPath;
    if (cleanPath.find("..") != std::string::npos)
    {
        return HttpResponse::text(400, "bad path");
    }

    std::filesystem::path filePath = std::filesystem::path(staticRoot_) / cleanPath.substr(1);
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        return HttpResponse::text(404, "not found");
    }

    std::ostringstream body;
    body << file.rdbuf();
    HttpResponse response(200);
    response.setHeader("Content-Type", contentTypeFor(filePath.string()));
    response.setBody(body.str());
    return response;
}

bool HttpServer::matchRoute(const std::string& pattern, const std::string& path,
                            std::map<std::string, std::string>& params)
{
    auto patternParts = splitPath(pattern);
    auto pathParts = splitPath(path);
    if (patternParts.size() != pathParts.size())
    {
        return false;
    }

    for (size_t i = 0; i < patternParts.size(); ++i)
    {
        const std::string& token = patternParts[i];
        if (token.size() > 2 && token.front() == '{' && token.back() == '}')
        {
            params[token.substr(1, token.size() - 2)] = urlDecode(pathParts[i]);
            continue;
        }
        if (token != pathParts[i])
        {
            return false;
        }
    }
    return true;
}

std::string HttpServer::contentTypeFor(const std::string& path)
{
    auto dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (ext == "html")
    {
        return "text/html; charset=utf-8";
    }
    if (ext == "css")
    {
        return "text/css; charset=utf-8";
    }
    if (ext == "js")
    {
        return "application/javascript; charset=utf-8";
    }
    if (ext == "json")
    {
        return "application/json; charset=utf-8";
    }
    if (ext == "svg")
    {
        return "image/svg+xml";
    }
    return "application/octet-stream";
}

} // namespace reactor_http_kit::http

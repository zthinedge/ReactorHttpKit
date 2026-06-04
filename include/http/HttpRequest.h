#pragma once

#include <map>
#include <string>

namespace reactor_http_kit::http
{

struct HttpRequest
{
    std::string method;
    std::string uri;
    std::string path;
    std::string body;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> pathParams;

    std::string header(const std::string& name) const;
};

bool tryParseHttpRequest(const std::string& data, HttpRequest& request, size_t& consumed);
std::string urlDecode(const std::string& value);

} // namespace reactor_http_kit::http

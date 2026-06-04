#pragma once

#include <map>
#include <string>

namespace reactor_http_kit::http
{

class HttpResponse
{
public:
    explicit HttpResponse(int statusCode = 200);

    int statusCode() const;
    void setStatusCode(int statusCode);
    void setHeader(std::string key, std::string value);
    void setBody(std::string body);
    void setJson(std::string body);

    std::string toString() const;

    static HttpResponse text(int statusCode, const std::string& body);
    static HttpResponse json(int statusCode, const std::string& body);

private:
    std::string reasonPhrase() const;

private:
    int statusCode_ { 200 };
    std::map<std::string, std::string> headers_;
    std::string body_;
};

} // namespace reactor_http_kit::http

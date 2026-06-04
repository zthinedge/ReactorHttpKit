#include "http/HttpResponse.h"

#include <sstream>

namespace reactor_http_kit::http
{

HttpResponse::HttpResponse(int statusCode)
    : statusCode_(statusCode)
{
    headers_["Connection"] = "close";
}

int HttpResponse::statusCode() const
{
    return statusCode_;
}

void HttpResponse::setStatusCode(int statusCode)
{
    statusCode_ = statusCode;
}

void HttpResponse::setHeader(std::string key, std::string value)
{
    headers_[std::move(key)] = std::move(value);
}

void HttpResponse::setBody(std::string body)
{
    body_ = std::move(body);
}

void HttpResponse::setJson(std::string body)
{
    headers_["Content-Type"] = "application/json; charset=utf-8";
    body_ = std::move(body);
}

std::string HttpResponse::toString() const
{
    std::ostringstream stream;
    stream << "HTTP/1.1 " << statusCode_ << ' ' << reasonPhrase() << "\r\n";
    for (const auto& [key, value] : headers_)
    {
        stream << key << ": " << value << "\r\n";
    }
    stream << "Content-Length: " << body_.size() << "\r\n\r\n";
    stream << body_;
    return stream.str();
}

HttpResponse HttpResponse::text(int statusCode, const std::string& body)
{
    HttpResponse response(statusCode);
    response.setHeader("Content-Type", "text/plain; charset=utf-8");
    response.setBody(body);
    return response;
}

HttpResponse HttpResponse::json(int statusCode, const std::string& body)
{
    HttpResponse response(statusCode);
    response.setJson(body);
    return response;
}

std::string HttpResponse::reasonPhrase() const
{
    switch (statusCode_)
    {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 404:
        return "Not Found";
    case 409:
        return "Conflict";
    case 500:
        return "Internal Server Error";
    default:
        return "OK";
    }
}

} // namespace reactor_http_kit::http

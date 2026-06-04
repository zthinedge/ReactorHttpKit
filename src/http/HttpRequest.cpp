#include "http/HttpRequest.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace reactor_http_kit::http
{

namespace
{

std::string trim(std::string value)
{
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

void parseQuery(const std::string& uri, HttpRequest& request)
{
    auto question = uri.find('?');
    request.path = question == std::string::npos ? uri : uri.substr(0, question);
    if (question == std::string::npos)
    {
        return;
    }

    std::string query = uri.substr(question + 1);
    std::stringstream stream(query);
    std::string pair;
    while (std::getline(stream, pair, '&'))
    {
        auto equal = pair.find('=');
        std::string key = equal == std::string::npos ? pair : pair.substr(0, equal);
        std::string value = equal == std::string::npos ? "" : pair.substr(equal + 1);
        request.query[urlDecode(key)] = urlDecode(value);
    }
}

} // namespace

std::string HttpRequest::header(const std::string& name) const
{
    auto it = headers.find(lower(name));
    return it == headers.end() ? "" : it->second;
}

bool tryParseHttpRequest(const std::string& data, HttpRequest& request, size_t& consumed)
{
    consumed = 0;
    auto headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        return false;
    }

    std::string headerBlock = data.substr(0, headerEnd);
    std::stringstream stream(headerBlock);
    std::string line;
    if (!std::getline(stream, line))
    {
        return false;
    }
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    std::stringstream requestLine(line);
    std::string version;
    requestLine >> request.method >> request.uri >> version;
    if (request.method.empty() || request.uri.empty())
    {
        return false;
    }
    parseQuery(request.uri, request);

    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }
        request.headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    }

    size_t contentLength = 0;
    auto contentLengthHeader = request.headers.find("content-length");
    if (contentLengthHeader != request.headers.end())
    {
        contentLength = static_cast<size_t>(std::strtoull(contentLengthHeader->second.c_str(), nullptr, 10));
    }

    size_t bodyStart = headerEnd + 4;
    if (data.size() < bodyStart + contentLength)
    {
        return false;
    }

    request.body = data.substr(bodyStart, contentLength);
    consumed = bodyStart + contentLength;
    return true;
}

std::string urlDecode(const std::string& value)
{
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '%' && i + 2 < value.size())
        {
            char hex[3] = { value[i + 1], value[i + 2], '\0' };
            decoded.push_back(static_cast<char>(std::strtol(hex, nullptr, 16)));
            i += 2;
        }
        else if (value[i] == '+')
        {
            decoded.push_back(' ');
        }
        else
        {
            decoded.push_back(value[i]);
        }
    }
    return decoded;
}

} // namespace reactor_http_kit::http

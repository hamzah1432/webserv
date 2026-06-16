#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

Config::Config() : _pos(0) {}

// =============================================================================
// 1. File reading
// =============================================================================

std::string Config::readFile(const std::string &path)
{
    std::ifstream file(path.c_str());
    if (!file.is_open())
        throw std::runtime_error("config: cannot open file '" + path + "'");

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// =============================================================================
// 2. Tokenizer
// =============================================================================


void Config::tokenize(const std::string &content)
{
    std::string current;

    for (size_t i = 0; i < content.size(); ++i)
    {
        char c = content[i];

        if (c == '#')
        {
            if (!current.empty())
            {
                _tokens.push_back(current);
                current.clear();
            }
            while (i < content.size() && content[i] != '\n')
                ++i;
        }
        else if (c == '{' || c == '}' || c == ';')
        {
            if (!current.empty())
            {
                _tokens.push_back(current);
                current.clear();
            }
            _tokens.push_back(std::string(1, c));
        }
        else if (std::isspace(static_cast<unsigned char>(c)))
        {
            if (!current.empty())
            {
                _tokens.push_back(current);
                current.clear();
            }
        }
        else
            current += c;
    }

    if (!current.empty())
        _tokens.push_back(current);
}

void Config::checkBraces()
{
    int depth = 0;

    for (size_t i = 0; i < _tokens.size(); ++i)
    {
        if (_tokens[i] == "{")
            depth++;
        else if (_tokens[i] == "}")
        {
            depth--;
            if (depth < 0)
                throw std::runtime_error("config: unexpected '}'");
        }
    }
    if (depth != 0)
        throw std::runtime_error("config: unmatched '{'");
}

// =============================================================================
// 3. Token cursor helpers
// =============================================================================

bool Config::hasNext() const
{
    return _pos < _tokens.size();
}

const std::string &Config::peek() const
{
    if (_pos >= _tokens.size())
        throw std::runtime_error("config: unexpected end of file");
    return _tokens[_pos];
}

const std::string &Config::next()
{
    if (_pos >= _tokens.size())
        throw std::runtime_error("config: unexpected end of file");
    return _tokens[_pos++];
}

void Config::expect(const std::string &tok)
{
    const std::string &got = next();
    if (got != tok)
        throw std::runtime_error("config: expected '" + tok + "' but got '" + got + "'");
}

// =============================================================================
// 4. Value conversion helpers
// =============================================================================

int Config::parseInt(const std::string &s)
{
    if (s.empty())
        throw std::runtime_error("config: expected a number");
    for (size_t i = 0; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            throw std::runtime_error("config: invalid number '" + s + "'");

    std::istringstream iss(s);
    long value;
    iss >> value;
    if (iss.fail() || value < 0 || value > 2147483647L)
        throw std::runtime_error("config: number out of range '" + s + "'");
    return static_cast<int>(value);
}

size_t Config::parseSize(const std::string &s)
{
    if (s.empty())
        throw std::runtime_error("config: expected a size");
    for (size_t i = 0; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            throw std::runtime_error("config: client_max_body_size must be a raw integer in bytes");

    std::istringstream iss(s);
    size_t value;
    iss >> value;
    if (iss.fail())
        throw std::runtime_error("config: invalid size '" + s + "'");
    return value;
}

// =============================================================================
// 5. Grammar
// =============================================================================

LocationConfig Config::parseLocation()
{
    LocationConfig loc;

    loc.path = next();
    expect("{");

    while (true)
    {
        std::string tok = next();

        if (tok == "}")
            break;
        else if (tok == "root")
        {
            loc.root = next();
            expect(";");
        }
        else if (tok == "index")
        {
            loc.index = next();
            expect(";");
        }
        else if (tok == "autoindex")
        {
            std::string val = next();
            if (val == "on")
                loc.autoindex = true;
            else if (val == "off")
                loc.autoindex = false;
            else
                throw std::runtime_error("config: autoindex must be 'on' or 'off'");
            expect(";");
        }
        else if (tok == "methods")
        {
            while (peek() != ";")
                loc.methods.push_back(next());
            expect(";");
        }
        else if (tok == "redirect")
        {
            loc.redirect = next();
            expect(";");
        }
        else if (tok == "upload_path")
        {
            loc.upload_path = next();
            expect(";");
        }
        else if (tok == "cgi_extension")
        {
            loc.cgi_extension = next();
            expect(";");
        }
        else if (tok == "cgi_path")
        {
            loc.cgi_path = next();
            expect(";");
        }
        else
            throw std::runtime_error("config: unknown directive '" + tok + "' in location block");
    }

    return loc;
}

ServerConfig Config::parseServer()
{
    ServerConfig server;

    while (true)
    {
        std::string tok = next();

        if (tok == "}")
            break;
        else if (tok == "listen")
        {
            std::string val = next();
            size_t colon = val.find(':');
            if (colon != std::string::npos)
            {
                server.host = val.substr(0, colon);
                server.port = parseInt(val.substr(colon + 1));
            }
            else
                server.port = parseInt(val);
            expect(";");
        }
        else if (tok == "server_name")
        {
            while (peek() != ";")
                server.server_names.push_back(next());
            expect(";");
        }
        else if (tok == "client_max_body_size")
        {
            server.client_max_body_size = parseSize(next());
            expect(";");
        }
        else if (tok == "error_page")
        {
            int code = parseInt(next());
            std::string page = next();
            server.error_pages[code] = page;
            expect(";");
        }
        else if (tok == "location")
        {
            server.locations.push_back(parseLocation());
        }
        else
            throw std::runtime_error("config: unknown directive '" + tok + "' in server block");
    }

    return server;
}

// =============================================================================
// 6. Entry point
// =============================================================================

std::vector<ServerConfig> Config::parse(const std::string &path)
{
    std::string content = readFile(path);

    _tokens.clear();
    _pos = 0;
    tokenize(content);
    checkBraces();

    std::vector<ServerConfig> servers;

    while (hasNext())
    {
        std::string tok = next();
        if (tok != "server")
            throw std::runtime_error("config: expected 'server' but got '" + tok + "'");
        expect("{");
        servers.push_back(parseServer());
    }

    if (servers.empty())
        throw std::runtime_error("config: no server block defined");

    return servers;
}

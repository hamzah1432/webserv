#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <cstddef>

struct LocationConfig
{
    std::string              path;
    std::string              root;
    std::vector<std::string> methods;
    bool                     autoindex;
    std::string              index;
    std::string              redirect;
    std::string              upload_path;
    std::string              cgi_extension;
    std::string              cgi_path;

    LocationConfig() : autoindex(false) {}
};

struct ServerConfig
{
    std::string                  host;
    int                          port;
    std::vector<std::string>     server_names;
    std::map<int, std::string>   error_pages;
    size_t                       client_max_body_size;
    std::vector<LocationConfig>  locations;

    ServerConfig() : host("0.0.0.0"), port(0), client_max_body_size(1048576) {}
};

class Config
{
private:
    std::vector<std::string> _tokens;
    size_t                   _pos;

    std::string         readFile(const std::string &path);
    void                tokenize(const std::string &content);
    void                checkBraces();

    bool                hasNext() const;
    const std::string  &peek() const;
    const std::string  &next();
    void                expect(const std::string &tok);

    ServerConfig        parseServer();
    LocationConfig      parseLocation();

    int                 parseInt(const std::string &s);
    size_t              parseSize(const std::string &s);

public:
    Config();

    std::vector<ServerConfig> parse(const std::string &path);
};

#endif

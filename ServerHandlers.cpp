#include "Server.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <cstdio>

// =============================================================================
// Get Request Handling
// =============================================================================

HTTPResponse Server::handleGet(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request)
{
	HTTPResponse response;

	std::string real_filepath = resolvePath(loc, request.getPath());

	struct stat st;
	if (stat(real_filepath.c_str(), &st) != 0)
	{
		response.setStatus(404);
	}
	else if (S_ISDIR(st.st_mode))
	{
		std::string index_path = real_filepath;
		if (!index_path.empty() && index_path[index_path.size() - 1] != '/')
			index_path += '/';
		index_path += loc.index;

		std::string file_content;
		if (!loc.index.empty() && readFile(index_path, file_content))
		{
			response.setStatus(200);
			response.setBody(file_content);
			response.setHeader("Content-Type", getContentType(index_path));
		}
		else if (loc.autoindex)
		{
			response.setStatus(200);
			response.setBody(generateAutoindex(real_filepath, request.getPath()));
			response.setHeader("Content-Type", "text/html");
		}
		else
		{
			response.setStatus(403);
		}
	}
	else
	{
		std::string file_content;
		if (!readFile(real_filepath, file_content))
		{
			response.setStatus(403);
		}
		else
		{
			response.setStatus(200);
			response.setBody(file_content);
			response.setHeader("Content-Type", getContentType(real_filepath));
		}
	}

	if (response.getStatusCode() >= 400)
	{
		response.setBody(getErrorBody(server, response.getStatusCode()));
		response.setHeader("Content-Type", "text/html");
	}

	return response;
}

std::string Server::getContentType(const std::string &path)
{
	std::string::size_type dot = path.rfind('.');
	if (dot == std::string::npos)
		return "application/octet-stream";

	std::string ext = path.substr(dot);
	if (ext == ".html" || ext == ".htm")
		return "text/html";
	if (ext == ".css")
		return "text/css";
	if (ext == ".js")
		return "application/javascript";
	if (ext == ".png")
		return "image/png";
	if (ext == ".jpg" || ext == ".jpeg")
		return "image/jpeg";
	if (ext == ".gif")
		return "image/gif";
	if (ext == ".txt")
		return "text/plain";
	return "application/octet-stream";
}

bool Server::readFile(const std::string &path, std::string &out)
{
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f.is_open())
		return false;
	std::stringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

std::string Server::generateAutoindex(const std::string &dir_path, const std::string &uri)
{
	DIR *dir = opendir(dir_path.c_str());
	if (dir == NULL)
		return "<html><body><h1>Error 500</h1><p>Cannot open directory</p></body></html>";

	std::stringstream ss;

	std::string base_uri = uri;
	if (base_uri.empty() || base_uri[base_uri.size() - 1] != '/')
		base_uri += '/';

	ss << "<html>\n<head>\n<title>Index of " << uri << "</title>\n";
	ss << "<style>\n"
	   << "  body { font-family: monospace; margin: 20px; background-color: #f8f9fa; }\n"
	   << "  h1 { color: #212529; }\n"
	   << "  hr { border: 0; border-top: 1px solid #dee2e6; margin: 10px 0; }\n"
	   << "  ul { list-style-type: none; padding-left: 5px; }\n"
	   << "  li { margin: 6px 0; font-size: 16px; }\n"
	   << "  a { color: #0056b3; text-decoration: none; }\n"
	   << "  a:hover { text-decoration: underline; }\n"
	   << "</style>\n</head>\n<body>\n";

	ss << "<h1>Index of " << uri << "</h1>\n<hr>\n<ul>\n";

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == ".")
			continue;
		std::string href = base_uri + name;
		ss << "    <li><a href=\"" << href << "\">" << name << "</a></li>\n";
	}
	closedir(dir);

	ss << "</ul>\n<hr>\n</body>\n</html>\n";
	return ss.str();
}

// =============================================================================
// Post Request Handling
// =============================================================================

bool Server::writeFile(const std::string &path, const std::string &content)
{
	std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
	if (!f.is_open())
		return false;

	f.write(content.c_str(), content.size());
	return f.good();
}

HTTPResponse Server::handlePost(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request)
{
	HTTPResponse response;

	if (loc.upload_path.empty())
		return makeErrorResponse(server, 403);

	std::string uri = request.getPath();

	if (!uri.empty() && uri[uri.size() - 1] == '/')
		return makeErrorResponse(server, 400);

	std::string::size_type last_slash = uri.rfind('/');
	std::string filename = "";
	if (last_slash == std::string::npos)
	{
		filename = uri;
	}
	else
	{
		filename = uri.substr(last_slash + 1);
	}

	std::string upload_dir = loc.upload_path;
	if (!upload_dir.empty() && upload_dir[upload_dir.size() - 1] != '/')
		upload_dir += '/';

	std::string full_dest_path = upload_dir + filename;

	if (!writeFile(full_dest_path, request.getBody()))
	{
		response.setStatus(500);
	}
	else
	{
		response.setStatus(201);
		response.setBody("<html><body><h1>201 Created</h1><p>File uploaded successfully.</p></body></html>");
		response.setHeader("Content-Type", "text/html");
	}

	if (response.getStatusCode() >= 400)
	{
		response.setBody(getErrorBody(server, response.getStatusCode()));
		response.setHeader("Content-Type", "text/html");
	}

	return response;
}

// =============================================================================
// Delete Request Handling
// =============================================================================

HTTPResponse Server::handleDelete(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request)
{
    HTTPResponse response;

    std::string real_filepath = resolvePath(loc, request.getPath());

    struct stat st;
    if (stat(real_filepath.c_str(), &st) != 0)
    {
        response.setStatus(404);
    }
    else if (S_ISDIR(st.st_mode))
    {
        response.setStatus(403);
    }
    else
    {
        if (std::remove(real_filepath.c_str()) != 0)
        {
            response.setStatus(403);
        }
        else
        {
            response.setStatus(200);
            response.setBody("<html><body><h1>200 OK</h1><p>File deleted successfully.</p></body></html>");
            response.setHeader("Content-Type", "text/html");
        }
    }

    if (response.getStatusCode() >= 400)
    {
        response.setBody(getErrorBody(server, response.getStatusCode()));
        response.setHeader("Content-Type", "text/html");
    }

    return response;
}
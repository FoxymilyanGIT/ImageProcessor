#include "base64.h"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>
#include <boost/json.hpp>
#include <boost/json/serialize.hpp>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>


namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace json = boost::json;
namespace fs = std::filesystem;
using tcp = net::ip::tcp;

namespace constants 
{
const std::string HOST_IP = "localhost";
const std::string PORT = "8000";
const std::string TARGET = "/";
const int VERSION = 11;
const http::verb METHOD = http::verb::post;
const int TIMEOUT_IN_SEC = 5;
} 

std::string read_file_base64(const std::string &path) 
{
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  std::string data = ss.str();
  return base64_encode(reinterpret_cast<const unsigned char *>(data.c_str()),data.size());
}

void save_image_from_base64(const std::string &base64_str, const std::string &out_path) 
{
  std::vector<unsigned char> data = base64_decode(base64_str);
  std::ofstream out(out_path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(data.data()), data.size());
}

std::string request_text_string() 
{
  std::string text;
  std::cout << "Enter text, that you want to add:\n";
  std::cin >> text;
  return text;
}

std::string trim_quotes(const std::string& s)
{
  if (s.size() >=2 && s.front() == '"' && s.back() == '"')
  {
    return s.substr(1,s.size() - 2);
  }
  return s;
}

bool check_extention(const std::string &filename, const std::string &extention) 
{
  std::string trimmed = trim_quotes(filename);
  fs::path path(trimmed);
  if(!fs::exists(path) && !fs::is_regular_file(path))
  {
    return false;
  }

  return boost::iequals(path.extension().string(),extention);
}

std::string request_image_path() 
{
  std::string image_path;
  std::cout << "Image must exist and has .jpg (.jpeg) extention\n";
  std::cout << "Enter image path without quotes:\nExample: D:\\Folder\\image.jpg\n";
  std::cin >> image_path;
  return image_path;
}

std::string validate_path(const std::string& user_input, const fs::path& default_path = "./")
{
  std::string trimmed = trim_quotes(user_input);
  fs::path path(trimmed);
  if (fs::exists(path))
  {
    return fs::absolute(path).string();
  }
  else 
  {
    std::cerr  << "Invalid path: \"" << trimmed << "\", using default: " << default_path << "\n";
    return fs::absolute(default_path).string();
  }
}

std::string request_result_path()
{
  std::string result_path;
  std::cout << "Enter result save path (Default: ./)\n";
  std::cin >> result_path;
  result_path = validate_path(result_path);
  return result_path;
} 

int main() 
{
  std::string text = request_text_string();
  std::string image_path;
  std::string result_path;
  bool success = false;

  image_path = request_image_path();
  if (check_extention(image_path, ".jpg") || check_extention(image_path, ".jpeg")) 
  {
    std::cout << "File: " << image_path << " accepted!\n";
  } 
  else 
  {
    std::cout << "Invalid input, closing application\n";
    std::cin.ignore();
    std::cin.get();
    return 0;
  }

  result_path = request_result_path();
  std::string image_b64 = read_file_base64(trim_quotes(image_path));
  if (image_b64.empty())
  {
    std::cerr << "Image base64 is empty!\n";
    return 1;
  }
  json::object req_json{{"text", text}, {"image", image_b64}};
  std::string body = json::serialize(req_json);

  while(!success)
  {
    try 
    {
      net::io_context ioc;
      tcp::resolver resolver{ioc};
      beast::tcp_stream stream{ioc};

      auto const result = resolver.resolve(constants::HOST_IP, constants::PORT);
      stream.connect(result);

      http::request<http::string_body> req{constants::METHOD,
                                           constants::TARGET,
                                           constants::VERSION};
      req.set(http::field::host, constants::HOST_IP);
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      req.set(http::field::content_type, "application/json");
      req.body() = body;
      req.prepare_payload();

      http::write(stream, req);

      beast::flat_buffer buffer;
      http::response<http::string_body> res;
      http::read(stream, buffer, res);

      auto response_json = json::parse(res.body()).as_object();
      std::string result_string = response_json["result"].as_string().c_str();

      if (res.result() != http::status::ok)
      {
        std::cout << "Error (" << res.result() <<"): " << result_string << " \nRetrying after 5 seconds\n";
        std::this_thread::sleep_for(std::chrono::seconds(constants::TIMEOUT_IN_SEC));
        continue;
      }

      save_image_from_base64(result_string, result_path + "\\result.jpg");
      std::cout << "Image saved into: result.jpg\n";
      success = true;

      beast::error_code ec;
      stream.socket().shutdown(tcp::socket::shutdown_both, ec);
      if (ec.failed())
      {
          std::cerr << "Socket closing error: " << ec.what() << "\n";
      }
    } 
    catch (std::exception &e) 
    {
      std::cerr << "Error:" << e.what() << "\n";
    }
  }
  return 0;
}
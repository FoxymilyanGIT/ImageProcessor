#include <boost/asio/io_context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/json/serialize.hpp>
#include <exception>
#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "base64.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace json = boost::json;
using tcp = net::ip::tcp;

namespace constants 
{
    const std::string HOST_IP = "localhost";
    const std::string PORT = "8000";
    const std::string TARGET = "/";
    const int VERSION = 11;
    const http::verb METHOD = http::verb::post;
}

std::string read_file_base64(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return base64_encode(reinterpret_cast<const unsigned char*>(ss.str().c_str()), ss.str().length());
}

void save_image_from_base64(const std::string& base64_str, const std::string& out_path) 
{
    std::vector<unsigned char> data = base64_decode(base64_str);
    std::ofstream out(out_path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
}

int main() 
{
    try 
    {
        std::string text = "Text";
        std::string image_b64 = read_file_base64("input.jpg");

        json::object req_json
        {
            {"text", text},
            {"image", image_b64}
        };

        std::string body = json::serialize(req_json);

        net::io_context ioc;
        tcp::resolver resolver{ioc};
        beast::tcp_stream stream{ioc};

        auto const result = resolver.resolve(constants::HOST_IP,constants::PORT);
        stream.connect(result);

        http::request<http::string_body> req
        {
            constants::METHOD,
            constants::TARGET,
            constants::VERSION
        };
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
        std::string result_image_b64 = response_json["result"].as_string().c_str();

        save_image_from_base64(result_image_b64, "result.jpg");

        std::cout << "Изображение получено и сохранено в result.jpg\n";

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    } catch (std::exception& e)
    {
        std::cerr << "Ошибка:" << e.what() << "\n";
    }

    return 0;
}
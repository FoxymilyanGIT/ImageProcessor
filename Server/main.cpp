#include <algorithm>
#include <boost/asio/post.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <boost/system/detail/error_code.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <opencv2/core/types.hpp>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <atomic>
#include <opencv4/opencv2/opencv.hpp>
#include <vector>
#include "base64.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace json = boost::json;
using tcp = net::ip::tcp;

namespace constants
{
	constexpr int PORT = 8000;
	constexpr int CONNECTIONS_LIMIT = 5;
	constexpr int FONT_FACE =cv::FONT_HERSHEY_SIMPLEX;
	constexpr double BASE_FONT_SCALE = 2.0;
	constexpr int BASE_FONT_THICKNESS = 2;
	constexpr double BASE_WIDTH = 1079;
}

std::atomic<int> active_connections{ 0 };


void http_session(tcp::socket socket) {
	if (active_connections.load() >= constants::CONNECTIONS_LIMIT)
	{
		std::cerr << "Connection refused: too many active connections\n";
	
		http::response<http::string_body> res{
			http::status::service_unavailable, 11
		};
		res.set(http::field::content_type, "application/json");
		res.body() = json::serialize("Server is busy");
		res.prepare_payload();
		beast::error_code ec;
		http::write(socket, res, ec);
		socket.shutdown(tcp::socket::shutdown_both, ec);
		socket.close(ec);
		return;
	}

	active_connections++;
	try 
	{
		beast::flat_buffer buffer;
		http::request_parser<http::string_body> parser;
		parser.body_limit(20 * 1024 * 1024);
		
		http::read(socket, buffer, parser);
		http::request<http::string_body> req = parser.get();

		if (req.method() == http::verb::post && req[http::field::content_type] == "application/json")
		{
			json::value value = json::parse(req.body());
			json::object obj = value.as_object();
			std::string text = json::value_to<std::string>(obj["text"]);
			std::string image_b64 = json::value_to<std::string>(obj["image"]);
			std::vector<unsigned char> image_data = base64_decode(image_b64);
			if (image_data.empty())
			{
				throw std::runtime_error("Decoded image data is empty!");
			}

			cv::Mat img = cv::imdecode(image_data, cv::IMREAD_COLOR);

			double scale_factor = img.cols / constants::BASE_WIDTH;
			double font_scale = constants::BASE_FONT_SCALE * scale_factor;
			int thickness = std::max(1,static_cast<int>(constants::BASE_FONT_THICKNESS * scale_factor));

			cv::Size text_size = cv::getTextSize(text, constants::FONT_FACE, font_scale, thickness, 0);
			cv::Point text_origin(
				(img.cols - text_size.width) / 2,
				img.rows - text_size.height
			);

			cv::Mat gray;
			cv::cvtColor(img,gray, cv::COLOR_BGR2GRAY);
			double mean_brightness = cv::mean(gray)[0];
			cv::Scalar text_color = (mean_brightness > 127)? cv::Scalar(0,0,0) : cv::Scalar(255,255,255);

			cv::putText(img, text, text_origin, constants::FONT_FACE, font_scale, text_color,thickness, cv::LINE_AA);
			std::vector<uchar> buf;
			cv::imencode(".jpeg", img, buf);
			std::string result_b64 = base64_encode(buf.data(), buf.size());

			json::object result_json;
			result_json["result"] = result_b64;

			http::response<http::string_body> res
			{
				http::status::ok, req.version()
			};
			res.set(http::field::content_type, "application/json");
			res.keep_alive(req.keep_alive());
			res.body() = json::serialize(result_json);
			res.prepare_payload();

			http::write(socket, res);
		}
		else
		{
			http::response<http::string_body> res
			{
				http::status::method_not_allowed, req.version()
			};
			res.set(http::field::content_type, "text/plain");
			res.body() = "Only POST allowed";
			res.prepare_payload();
			http::write(socket, res);
		}
	}
	catch (const std::exception& e) 
	{
		std::cerr << "Session error:" << e.what() << "\n";
	}
	
	auto guard = std::unique_ptr<void, std::function<void(void*)>>(nullptr, [&](void*)
	{
		active_connections--;
	});
}

int main() 
{
	try 
	{
		net::io_context ioc;
		tcp::acceptor acceptor(ioc, { tcp::v4(),constants::PORT });
		std::cout << "Server started at port:" << constants::PORT << "\n";

		std::function<void()> do_accept;
		do_accept = [&]() 
		{
			std::shared_ptr<tcp::socket> socket = std::make_shared<tcp::socket>(ioc);
			acceptor.async_accept(*socket, [&,socket](boost::system::error_code ec)
			{
				if(!ec) 
				{
					boost::asio::post(ioc, [s = std::move(*socket)]() mutable {
						http_session(std::move(s));
					});
				}
				do_accept();
			});
		};

		do_accept();

		std::vector<std::thread> threads;
		const unsigned int thread_count = constants::CONNECTIONS_LIMIT;
		for (unsigned int i = 0; i < thread_count; ++i) 
		{
			threads.emplace_back([&ioc](){
				ioc.run();
			});
		}

		for (std::thread& t : threads)
		{
			t.join();
		}
	
	}
	catch (std::exception& e) 
	{
		std::cerr << "Error:" << e.what() << "\n";
	}
	return 0;
}
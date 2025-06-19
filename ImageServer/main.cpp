#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <thread>
#include <iostream>
#include <fstream>
#include <atomic>
#include <opencv4/opencv2/opencv.hpp>
#include "base64.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace json = boost::json;
using tcp = net::ip::tcp;

namespace constants
{
	const int PORT = 8000;
	const int CONNECTIONS_LIMIT = 10;
}

std::atomic<int> active_connections{ 0 };


void http_session(tcp::socket socket) {
	if (active_connections.load() >= constants::CONNECTIONS_LIMIT)
	{
		std::cerr << "Connection refused: too many active connections\n";
	

		http::response<http::string_body> res{
			http::status::service_unavailable, 11
		};
		res.set(http::field::content_type, "text/plain");
		res.body() = "Server is buys. Please try again later.";
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
			auto image_data = base64_decode(image_b64);

			std::cout << "Text:" << text << "\n";

			std::ofstream out("images/uploaded_image.jpeg", std::ios::binary);
			if (out.is_open())
			{
				out.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());
				out.close();
				std::cout << "Saved uploaded_image.jpeg (" << req.body().size() << " bytes)\n";
			}
			else
			{
				std::cerr << "Failed to open output stream\n";
			}

			cv::Mat img = cv::imdecode(image_data, cv::IMREAD_COLOR);

			int font_face = cv::FONT_HERSHEY_SIMPLEX;
			double font_scale = 1.0;
			int thickness = 2;

			cv::Size text_size = cv::getTextSize(text, font_face, font_scale, thickness, 0);

			cv::Point text_origin(
				(img.cols - text_size.width) / 2,
				img.rows - 10
			);


			cv::putText(img, text, text_origin, font_face, font_scale, CV_RGB(255,255,255),thickness, cv::LINE_AA);
			std::vector<uchar> buf;
			cv::imencode(".jpeg", img, buf);
			std::string result_b64 = base64_encode(buf.data(), buf.size());


			json::object result_json;
			result_json["result"] = result_b64;

			http::response<http::string_body> res;
		
			res.set(http::field::content_type, "application/json");
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
	catch (std::exception& e) 
	{
		std::cerr << "Session error:" << e.what() << "\n";
	}
	active_connections--;
}

int main() 
{
	try 
	{
		net::io_context ioc;

		tcp::acceptor acceptor(ioc, { tcp::v4(),constants::PORT });
		std::cout << "Server started at port:" << constants::PORT << "\n";

		for (;;) 
		{
			tcp::socket socket = acceptor.accept();
			std::thread{ http_session, std::move(socket),  }.detach();
		}		
	}
	catch (std::exception& e) 
	{
		std::cerr << "Error:" << e.what() << "\n";
	}
	return 0;
}
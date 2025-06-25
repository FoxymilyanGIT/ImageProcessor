#include <algorithm>
#include <boost/asio/post.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/system/detail/error_code.hpp>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <opencv2/core/types.hpp>
#include <stdexcept>
#include <iostream>
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

class ConnectionManager {
	private:
		const int max_connections;
		int active_connections;
		std::mutex m;
	public:
		ConnectionManager(int max)
		: max_connections(max)
		{
			active_connections = 0;
		}

	bool allow()
	{
		std::lock_guard<std::mutex> lock(m);
		if (active_connections >= max_connections)
		{
			return false;		
		}
	++active_connections;
	return true;
	}

	void release()
	{
		std::lock_guard<std::mutex> lock(m);
		--active_connections;
	}
};


beast::error_code send_response(tcp::socket& socket ,const http::status& response_status,const std::string& message)
{
	json::object json_message;
	json_message["result"] = message;
	http::response<http::string_body> res
	{
		response_status, 11
	};
	res.set(http::field::content_type, "application/json");
	res.body() = json::serialize(json_message);
	res.prepare_payload();
	beast::error_code ec;
	http::write(socket,res,ec);
	return ec;
}

void http_session(std::shared_ptr<tcp::socket> socket) 
{
	beast::flat_buffer buffer;
	beast::error_code ec;
	http::request_parser<http::string_body> parser;
	parser.body_limit(20 * 1024 * 1024);

	http::read(*socket, buffer, parser);
	std::cout << "[DEBUG] Data read";

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

		ec = send_response(*socket, http::status::ok, result_b64);

	}
	else
	{
		ec = send_response(*socket, http::status::method_not_allowed, "Only POST allowed");
	}
	socket->shutdown(tcp::socket::shutdown_both,ec);
	socket->close(ec);

}

int main() 
{
	try 
	{
		net::io_context ioc;
		tcp::acceptor acceptor(ioc, { tcp::v4(),constants::PORT });
		net::thread_pool thread_pool(constants::CONNECTIONS_LIMIT + 1);

		std::shared_ptr<ConnectionManager> conn_mgr = std::make_shared<ConnectionManager>(constants::CONNECTIONS_LIMIT);

		std::cout << "Server started at port:" << constants::PORT << "\n";


		std::function<void()> do_accept;
		do_accept = [&]() 
		{
			std::shared_ptr<tcp::socket> socket = std::make_shared<tcp::socket>(ioc);
			acceptor.async_accept(*socket, [&, socket](boost::system::error_code ec)
			{
				if(!ec) 
				{
					if (conn_mgr->allow())
					{
						std::cout << "[DEBUG] Connected!\n";
						net::post(thread_pool, [socket,  conn_mgr](){
							try
							{
								std::cout << "[DEBUG] Session started\n";
								http_session(socket);
								std::cout << "[DEBUG] Session ended\n";
							}
							catch (const std::exception& e)
							{
								std::cerr << "Session error: " << e.what() << "\n";
								beast::error_code ec = send_response(*socket, http::status::internal_server_error, "Internal Server Error");
								socket->shutdown(tcp::socket::shutdown_both,ec);
								socket->close(ec);
							}
							conn_mgr->release();
						});
						std::cout << "[DEBUG] Posted session to thread pool\n";
					}
					else 
					{
						std::cout << "[DEBUG] Limit exceeded\n";
						net::post(thread_pool, [socket]() {
							beast::error_code ec = send_response(*socket, http::status::service_unavailable, "Server is busy");
							socket->shutdown(tcp::socket::shutdown_both,ec);
							socket->close(ec);
						});
					}
				}
				do_accept();
			});
		};

		do_accept();

		std::thread io_thread([&ioc](){ioc.run();});

		io_thread.join();
		thread_pool.join();
	
	}
	catch (std::exception& e) 
	{
		std::cerr << "Error:" << e.what() << "\n";
	}
	return 0;
}

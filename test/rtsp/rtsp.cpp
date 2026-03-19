#include "rtsp/rtspserver.h"
#include "net/asio_io_context_pool.h"
#include "log/logmanager.h"

int main() {
	LoggerConfig config("rtsp", spdlog::level::debug);
	config.write_to_console = true;
	config.write_to_main_log = true;
	LogManager::getInstance().RegisterLogger(config);
	LogManager::getInstance().Init();

	boost::asio::io_context io_context;
	auto& worker_pool = AsioIOContextPool::GetInstance(AsioIOContextPool::ServiceType::HTTP);
	rtsp:: RtspServer server(io_context, worker_pool, 8554);
	server.Start();
	io_context.run();
	return 0;
}

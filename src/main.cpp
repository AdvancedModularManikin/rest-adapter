#include <iostream>
#include <thread>
#include <signal.h>
#include <filesystem>
#include <limits.h>
#include <unistd.h>

#include "amm/BaseLogger.h"

#include "core/Config.h"
#include "core/MoHSESManager.h"

#include "api/Router.h"

#include "db/DatabaseConnection.h"
#include "db/DatabaseConfig.h"

#include "services/StatusService.h"
#include "services/ModuleService.h"
#include "services/SystemService.h"

#include "utils/Exceptions.h"

namespace {
	volatile sig_atomic_t g_running = 1;

	void signalHandler(int signum) {
		if (signum == SIGINT) {
			std::cout << "\nReceived Ctrl-C, initiating graceful shutdown...\n";
		} else if (signum == SIGTERM) {
			std::cout << "\nReceived termination signal, initiating graceful shutdown...\n";
		}
		g_running = 0;
	}

	void setupSignalHandling() {
		struct sigaction sa;
		sa.sa_handler = signalHandler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;

		if (sigaction(SIGINT, &sa, NULL) == -1) {
			LOG_ERROR << "Failed to set up SIGINT handler";
		}
		if (sigaction(SIGTERM, &sa, NULL) == -1) {
			LOG_ERROR << "Failed to set up SIGTERM handler";
		}
		signal(SIGPIPE, SIG_IGN);
	}

	void setupLogging() {
		static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
		plog::init(plog::verbose, &consoleAppender);
	}

	void showUsage(const std::string& name) {
		std::cerr << "Usage: " << name << " <option(s)>\n"
		          << "Options:\n"
		          << "\t-h,--help\t\tShow this help message\n"
		          << "\t-d\t\t\tRun as daemon\n"
		          << "\t-nodiscovery\t\tDisable discovery service\n"
		          << "\t-p,--port PORT\t\tSpecify port number (default: 9080)\n"
		          << "\t-t,--threads N\t\tSpecify number of threads (default: 2)\n"
		          << "\t-c,--config PATH\t\tSpecify config file path\n"
		          << std::endl;
	}

}

class ServerApplication {
public:
	ServerApplication() = default;

	bool initialize(int argc, char* argv[]) {
		try {
			parseCommandLine(argc, argv);

			setupLogging();

			setupSignalHandling();

			Config::getInstance().initialize(m_configPath);


			DatabaseConfig::getInstance().initialize();
			m_db = std::make_unique<DatabaseConnection>();

			MoHSESManager::getInstance().initialize(m_config.mohsesConfigFile);

			StatusService::getInstance().resetLabs();

			SystemService::getInstance().initialize();

			m_server = std::make_unique<Pistache::Http::Endpoint>(
					Pistache::Address(m_config.bindAddress, m_config.port)
			);

			auto opts = Pistache::Http::Endpoint::options()
					.threads(m_config.threads)
					.flags(Pistache::Tcp::Options::ReuseAddr);

			m_server->init(opts);

			m_router = std::make_unique<Router>();
			m_router->initializeRoutes(*m_server);

			LOG_INFO << "Server initialized on " << m_config.bindAddress
			         << ":" << m_config.port
			         << " with " << m_config.threads << " threads";

			// Get hostname for instance identification
			char hostname[HOST_NAME_MAX];
			gethostname(hostname, HOST_NAME_MAX);
			LOG_INFO << "Server hostname: " << hostname;

			return true;
		}
		catch (const std::exception& e) {
			LOG_FATAL << "Initialization failed: " << e.what();
			return false;
		}
	}

	void run() {
		try {
			LOG_INFO << "Starting server listener loop...";
			LOG_INFO << "Press Ctrl-C to shutdown gracefully";
			std::thread serverThread([this]() {
				m_server->serve();
			});

			// Main loop
			while (g_running) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			m_server->shutdown();
			if (serverThread.joinable()) {
				serverThread.join();
			}

			shutdown();
		}
		catch (const std::exception& e) {
			LOG_FATAL << "Runtime error: " << e.what();
			m_server->shutdown();
			shutdown();
		}
	}

	void shutdown() {
		LOG_INFO << "Shutting down server...";

		// Set timeout for shutdown operations
		const auto timeout = std::chrono::seconds(5);
		auto start = std::chrono::steady_clock::now();

		if (m_server) {
			// Make sure server is shut down with timeout
			while (m_server && std::chrono::steady_clock::now() - start < timeout) {
				try {
					m_server->shutdown();
					break;
				} catch (const std::exception &e) {
					LOG_ERROR << "Error during server shutdown: " << e.what();
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		}
		m_router.reset();
		MoHSESManager::getInstance().shutdown();
		m_db.reset();

		LOG_INFO << "Server shutdown complete";
	}

private:
	struct ServerConfig {
		std::string bindAddress = "*";
		int port = 9080;
		int threads = 2;
		bool daemonize = false;
		bool discovery = true;
		std::string mohsesConfigFile = "config/rest_adapter_mohses.xml";
	};

	void parseCommandLine(int argc, char* argv[]) {
		for (int i = 1; i < argc; ++i) {
			std::string arg = argv[i];
			if (arg == "-h" || arg == "--help") {
				showUsage(argv[0]);
				exit(0);
			}
			else if (arg == "-d") {
				m_config.daemonize = true;
			}
			else if (arg == "-nodiscovery") {
				m_config.discovery = false;
			}
			else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
				m_config.port = std::stoi(argv[++i]);
			}
			else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
				m_config.threads = std::stoi(argv[++i]);
			}
			else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
				m_configPath = argv[++i];
			}
		}

		// Validate configuration
		if (m_config.port < 1 || m_config.port > 65535) {
			throw ConfigException("Invalid port number");
		}
		if (m_config.threads < 1) {
			throw ConfigException("Invalid thread count");
		}
	}

	ServerConfig m_config;
	std::string m_configPath = "config/server.config";
	std::unique_ptr<Pistache::Http::Endpoint> m_server;
	std::unique_ptr<Router> m_router;
	std::unique_ptr<DatabaseConnection> m_db;
};

int main(int argc, char* argv[]) {
	try {
		ServerApplication app;

		if (!app.initialize(argc, argv)) {
			return 1;
		}

		app.run();
		return 0;
	}
	catch (const std::exception& e) {
		LOG_FATAL << "Fatal error: " << e.what();
		return 1;
	}
}
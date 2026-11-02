#include "gommanna/app.hpp"
#include <csignal>
#include <iostream>
#include <thread>

namespace { volatile std::sig_atomic_t stopping = 0; void stop_signal(int) { stopping = 1; } }
int main() {
    try {
        auto config = gm::Config::load();
        gm::App app(config);
        std::signal(SIGINT,stop_signal);
        std::signal(SIGTERM,stop_signal);
#ifndef _WIN32
        std::signal(SIGPIPE,SIG_IGN);
#endif
        std::jthread signal_watcher([&](std::stop_token stop) {
            while (!stop.stop_requested() && !stopping) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (stopping) app.stop();
        });
        std::jthread cleaner([&](std::stop_token stop) {
            for (int ticks = 0; !stop.stop_requested(); ++ticks) {
                if (ticks % 60 == 0) {
                    try { app.cleanup(); }
                    catch (...) { std::cerr << "Expired-room cleanup unavailable; will retry\n"; }
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
        std::cout << "GomManna server listening on " << config.host << ':' << config.port << std::endl;
        const bool success = app.listen();
        cleaner.request_stop();
        signal_watcher.request_stop();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Startup failed: " << e.what() << '\n';
        return 1;
    }
}

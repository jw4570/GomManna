#pragma once
#include "gommanna/db.hpp"
#include <httplib.h>
#include <functional>

namespace gm {
class App {
    Config config_;
    Auth auth_;
    Pool pool_;
    RateLimiter limits_;
    httplib::Server http_;
    using Action = std::function<Json(Db&, const std::string&)>;
    void respond(const httplib::Request&, httplib::Response&, const Action&, int status = 200, bool authenticated = true);
    void routes();
public:
    explicit App(Config config);
    bool listen();
    void stop() { http_.stop(); }
    void cleanup();
};
}

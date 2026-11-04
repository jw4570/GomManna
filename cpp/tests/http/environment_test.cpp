#include "../../http/environment.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void set_variable(const char* name, const std::string& value) {
#ifdef _WIN32
    if (_putenv_s(name, value.c_str()) != 0) throw std::runtime_error("could not set test variable");
#else
    if (setenv(name, value.c_str(), 1) != 0) throw std::runtime_error("could not set test variable");
#endif
}
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
} // namespace

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto file = std::filesystem::temp_directory_path() /
                      ("gommanna-env-test-" + std::to_string(suffix) + ".env");
    try {
        {
            std::ofstream out(file, std::ios::binary);
            out << "# Example only\r\nGOMMANNA_SAMPLE=hello%40world?x=a=b#fragment\r\n"
                   "GOMMANNA_QUOTED=\"quoted value\"\r\n";
        }
        set_variable("GOMMANNA_ENV_FILE", file.string());
        const auto values = gommanna::http_config::load_file();
        check(gommanna::http_config::get(values, "GOMMANNA_SAMPLE") == "hello%40world?x=a=b#fragment", "value parsing");
        check(gommanna::http_config::get(values, "GOMMANNA_QUOTED") == "quoted value", "quoted parsing");
        set_variable("GOMMANNA_SAMPLE", "process-value");
        check(gommanna::http_config::get(values, "GOMMANNA_SAMPLE") == "process-value", "process precedence");
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << "invalid line containing TEST_SECRET_VALUE\n";
        }
        bool rejected = false;
        try { (void)gommanna::http_config::load_file(); }
        catch (const std::exception& e) {
            rejected = std::string(e.what()).find("TEST_SECRET_VALUE") == std::string::npos;
        }
        check(rejected, "malformed input must fail without leaking values");
        std::filesystem::remove(file);
        std::cout << "PASS: environment file and process override\n";
        return 0;
    } catch (const std::exception& e) {
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}

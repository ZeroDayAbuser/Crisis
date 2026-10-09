#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>

namespace rise {
    inline void log_error(const std::string& msg) {
        std::cerr << "[Rise Error] " << msg << std::endl;
    }
}

struct AuthResponse {
    bool success;
    long long start_unix;
    long long expiry_unix;
    std::string access_token;
};

AuthResponse tcp_auth(const std::string& key, const std::string& hwid) {
    AuthResponse response = { true, 1718812800, 1721491200, "short-lived-bearer-token" };
    return response;
}

void run_auth(const std::string& key, const std::string& hwid) {
    AuthResponse resp = tcp_auth(key, hwid);

    if (!resp.success) {
        rise::log_error("Authentication failed: success=false or key expired");
        std::exit(1);
    }

    long long duration = resp.expiry_unix - resp.start_unix;
    long long now = static_cast<long long>(std::time(nullptr));

    if (now >= resp.expiry_unix || resp.access_token.empty()) {
        rise::log_error("Key expired");
        std::exit(1);
    }

    std::cout << "Successfully logged in!" << std::endl;
    std::cout << "  Duration: " << duration << " seconds" << std::endl;
    std::cout << "  Start:    " << resp.start_unix << std::endl;
    std::cout << "  Expiry:   " << resp.expiry_unix << std::endl;
}

int main() {
    std::string key = "YOUR_KEY_HERE";
    std::string hwid = "YOUR_HWID_HERE";
    run_auth(key, hwid);
    return 0;
}

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

// A real child process: never echo the supplied token to output or disk.
int main()
{
    const char *value = std::getenv("EZ4CONNECT_TEST_AUTH_MODE");
    const std::string mode = value ? value : "atrust";
    const std::string prompt = mode == "easyconnect"
        ? "Please enter your TOTP code:" : "Please enter the TOTP token:";
    std::cerr << "Continue authentication: service=auth/totp smsMode=0\n";
    if (mode == "partial")
    {
        std::cerr << prompt.substr(0, 17) << std::flush;
        return 0;
    }
    std::cerr << "2026/10/11 17:59:12 " << prompt.substr(0, 17) << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::cerr << prompt.substr(17) << std::flush;
    if (mode == "exit")
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 0;
    }
    // Deliberately duplicate the outstanding challenge on both channels.
    std::cout << prompt << '\n' << std::flush;
    std::cerr << '\n' << prompt << '\n' << std::flush;
    for (const std::string expected : {"123456", "654321"})
    {
        std::string input;
        if (!std::getline(std::cin, input) || input != expected)
        {
            return 2;
        }
        if (expected == "123456")
        {
            std::cerr << "Token incorrect or expired; retry\n" << prompt << std::flush;
        }
    }
    std::cout << "VPN client started\n" << std::flush;
    std::string unused;
    std::getline(std::cin, unused);
    return 0;
}

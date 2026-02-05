#include "urlHandler.hpp"
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

// --------------------------------------------------
// Logging
// --------------------------------------------------
void redirect_stdio_to_file()
{
    std::string path = std::string(getenv("HOME")) +
                       "/Library/Logs/ElgatoMarketplaceConnect.log";

    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;

    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
}

namespace UrlHandler
{
    // --------------------------------------------------
    // OBS / Unix socket
    // --------------------------------------------------
    static const char* SOCKET_BASE_PATH = "/tmp/elgato_cloud";
    static const int MAX_SOCKET_INDEX = 10;

    static int send_to_obs_socket(const std::string& payload,
                                int retries,
                                int sleep_seconds)
    {
        for (int attempt = 0; attempt < retries; ++attempt) {
            for (int i = 0; i < MAX_SOCKET_INDEX; ++i) {
                std::string socket_path = std::string(SOCKET_BASE_PATH) + std::to_string(i);

                int fd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (fd < 0) continue;

                sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);

                if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                    ssize_t written = write(fd, payload.c_str(), payload.size());
                    close(fd);
                    if (written == static_cast<ssize_t>(payload.size())) {
                        std::cerr << "[INFO] Sent payload to OBS: " << payload << "\n";
                        return 0;
                    }
                }
                close(fd);
            }
            sleep(sleep_seconds);
        }
        std::cerr << "[ERROR] Could not connect to OBS socket\n";
        return 1;
    }

    // --------------------------------------------------
    // OBS launch
    // --------------------------------------------------
    static bool is_obs_running()
    {
        FILE* pipe = popen("pgrep -x OBS", "r");
        if (!pipe) return false;
        char buffer[16];
        bool running = fgets(buffer, sizeof(buffer), pipe) != nullptr;
        pclose(pipe);
        return running;
    }

    static int launch_obs_if_needed()
    {
        if (is_obs_running()) return 0;
        std::cerr << "[INFO] Launching OBS\n";
        pid_t pid = fork();
        if (pid == 0) {
            execl("/Applications/OBS.app/Contents/MacOS/OBS", "OBS", nullptr);
            _exit(1);
        }
        return 0;
    }

    int handleUrl(const std::string& url)
    {
        std::cerr << url << "\n";
        
        // OAuth return -> send immediately
        if (url.rfind("elgatolink://auth", 0) == 0) {
            return send_to_obs_socket(url, 6, 2);
        }

        // Otherwise: ensure OBS is running, then send open command
        if (launch_obs_if_needed() != 0) return 1;

        return send_to_obs_socket("elgatolink://open", 60, 1);
    }
}

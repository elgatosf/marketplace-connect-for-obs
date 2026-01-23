#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <spawn.h>
#include <errno.h>

extern char **environ;

// ---- Configuration ----

//static const char* OBS_APP_PATH = "/Applications/OBS.app";
static const char* OBS_BINARY_PATH = "/Applications/OBS.app/Contents/MacOS/OBS";

// Match this with your OBS plugin
static const char* SOCKET_BASE_PATH =
    "/tmp/elgato_mp_connect";

static const int MAX_SOCKET_INDEX = 10;
static const int CONNECT_RETRIES_SHORT = 6;
static const int CONNECT_RETRIES_LONG  = 60;

bool is_obs_running() {
    FILE* pipe = popen("pgrep -x OBS", "r");
    if (!pipe) return false;

    char buffer[16];
    bool running = fgets(buffer, sizeof(buffer), pipe) != nullptr;
    pclose(pipe);
    return running;
}

int launch_obs() {
    if (is_obs_running()) {
        return 0;
    }

    pid_t pid;
    char* args[] = {
        const_cast<char*>(OBS_BINARY_PATH),
        nullptr
    };

    int status = posix_spawn(
        &pid,
        OBS_BINARY_PATH,
        nullptr,
        nullptr,
        args,
        environ
    );

    if (status != 0) {
        std::cerr << "[ERROR] Failed to launch OBS: " << strerror(status) << std::endl;
        return 1;
    }

    return 0;
}

int connect_and_send(const std::string& payload, int retries, int sleep_seconds) {
    for (int attempt = 0; attempt < retries; ++attempt) {
        for (int i = 0; i < MAX_SOCKET_INDEX; ++i) {
            std::string socket_path =
                std::string(SOCKET_BASE_PATH) + std::to_string(i);

            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0)
                continue;

            sockaddr_un addr {};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, socket_path.c_str(),
                         sizeof(addr.sun_path) - 1);

            if (connect(fd, reinterpret_cast<sockaddr*>(&addr),
                        sizeof(addr)) == 0) {
                ssize_t written = write(fd, payload.c_str(), payload.size());
                close(fd);

                if (written == static_cast<ssize_t>(payload.size())) {
                    std::cout << "Connected to OBS plugin\n";
                    return 0;
                }
            }

            close(fd);
        }

        sleep(sleep_seconds);
    }

    std::cerr << "[ERROR] Could not connect to OBS plugin socket.\n";
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "[ERROR] No argument provided.\n";
        return 1;
    }

    std::string payload = argv[1];

    // OAuth callback
    if (payload.rfind("elgatolink://auth", 0) == 0) {
        return connect_and_send(payload, CONNECT_RETRIES_SHORT, 2);
    }

    // Otherwise: ensure OBS is running, then open plugin window
    if (launch_obs() != 0) {
        return 1;
    }

    std::string open_payload = "elgatolink://open";
    return connect_and_send(open_payload, CONNECT_RETRIES_LONG, 1);
}

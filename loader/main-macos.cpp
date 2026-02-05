#include <Carbon/Carbon.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <string>
#include <atomic>

// --------------------------------------------------
// Logging
// --------------------------------------------------
static void redirect_stdio_to_file()
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

// --------------------------------------------------
// Apple Event handling
// --------------------------------------------------
static std::string g_payload;
static std::atomic<bool> g_received{false};

static OSStatus handle_get_url(const AppleEvent* event,
                               AppleEvent* /*reply*/,
                               long /*refcon*/)
{
    AEDesc desc;
    if (AEGetParamDesc(event, keyDirectObject, typeChar, &desc) == noErr) {
        Size size = AEGetDescDataSize(&desc);
        if (size > 0 && size < 16 * 1024) {
            std::string url(size, '\0');
            AEGetDescData(&desc, url.data(), size);
            g_payload = url;
            g_received = true;
            std::cerr << "[INFO] Received URL via Apple Event: " << g_payload << "\n";
        }
        AEDisposeDesc(&desc);
    }
    return noErr;
}

static OSStatus generic_apple_event_handler(const AppleEvent* event,
                                            AppleEvent* /*reply*/,
                                            long /*refcon*/)
{
    if (!event) return noErr;

    // Use AEGetAttributeDesc and AEGetDescData to read class/id
    AEDesc classDesc;
    AEDesc idDesc;
    AEEventClass eventClass = 0;
    AEEventID eventID = 0;

    if (AEGetAttributeDesc(event, keyEventClassAttr, typeType, &classDesc) == noErr) {
        AEGetDescData(&classDesc, &eventClass, sizeof(eventClass));
        AEDisposeDesc(&classDesc);
    }

    if (AEGetAttributeDesc(event, keyEventIDAttr, typeType, &idDesc) == noErr) {
        AEGetDescData(&idDesc, &eventID, sizeof(eventID));
        AEDisposeDesc(&idDesc);
    }

    std::cerr << "[DEBUG] Apple Event received: class=0x"
              << std::hex << eventClass
              << " id=0x" << eventID << "\n";

    return noErr;
}


static void install_event_handlers()
{
    // Log any application open events
    AEInstallEventHandler(kCoreEventClass, kAEOpenApplication,
                          reinterpret_cast<AEEventHandlerUPP>(generic_apple_event_handler),
                          0, false);

    // Install the main URL handler
    AEInstallEventHandler(kInternetEventClass, kAEGetURL,
                          reinterpret_cast<AEEventHandlerUPP>(handle_get_url),
                          0, false);
}

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

// --------------------------------------------------
// Main
// --------------------------------------------------
int main()
{
    // Transform process to foreground GUI app
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToForegroundApplication);

    redirect_stdio_to_file();
    std::cerr << "[INFO] Helper launched\n";

    install_event_handlers();

    // Run CFRunLoop until URL is received or timeout (5 seconds)
    int wait_iterations = 500; // 500 * 10ms = 5s
    for (int i = 0; i < wait_iterations && !g_received; ++i) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
    }

    if (!g_received || g_payload.empty()) {
        std::cerr << "[ERROR] No URL payload received\n";
        return 1;
    }

    std::cerr << "[INFO] Final payload: " << g_payload << "\n";

    // OAuth return → send immediately
    if (g_payload.rfind("elgatolink://auth", 0) == 0) {
        return send_to_obs_socket(g_payload, 6, 2);
    }

    // Otherwise: ensure OBS is running, then send open command
    if (launch_obs_if_needed() != 0) return 1;

    return send_to_obs_socket("elgatolink://open", 60, 1);
}

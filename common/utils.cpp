#ifdef OSX_BUILD
#include <mach-o/dyld.h>
#endif

#include <stdexcept>
#include <string>
#include <ctime>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "socket.hpp"

#if defined(__APPLE__)
// for _NSGetExecutablePath
#include <mach-o/dyld.h>
#endif

// Convert a domain name to an in_addr using gethostbyname
in_addr_t GetAddrFromDomain(const std::string& domain) {
    struct hostent* he = gethostbyname(domain.c_str());
    if (he == nullptr) {
        he = gethostbyname("127.0.0.1");
    }
    auto addr_list = reinterpret_cast<in_addr**>(he->h_addr_list);
    return addr_list[0]->s_addr;
}

// Monotonic nanosecond timer, portable across Windows / macOS / Linux.
// The original implementation used clock_gettime(CLOCK_REALTIME) which is
// POSIX-only; on Windows clang-cl that fails with "use of undeclared
// identifier 'CLOCK_REALTIME'". std::chrono::steady_clock gives us the
// same semantics everywhere in a single line.
static uint64_t clock_elapsed_ns(void) {
    static const auto start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count());
}

float clock_elapsed(void) {
    return (clock_elapsed_ns() / 1000000000.0f);
}

std::string getExecutablePath() {
    char path[0xFF];
#if defined(_WIN32)
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) != 0) {
        return std::string(path);
    }
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return std::string(path);
    }
#elif defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::string(path);
    }
#endif
    return "";
}

static std::string readFileData(const std::string &filepath) {
    if (filepath == "") { return ""; }
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open file.");
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string data(fileSize, '\0');
    if (!file.read(&data[0], fileSize)) throw std::runtime_error("Cannot read file data.");
    return data;
}

std::size_t hashFile(const std::string &filepath = getExecutablePath()) {
    const std::string data = readFileData(filepath);
    if (data == "") { return 0; }
    return std::hash<std::string>{}(data);
}

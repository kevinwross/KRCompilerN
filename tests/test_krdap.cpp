#include <doctest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace {

// Send a DAP message to krdap and read the response body.
std::string dap_roundtrip(const std::string& request_json,
                          const std::string& krdap_path) {
    (void)request_json;
    (void)krdap_path;
#ifdef _WIN32
    // Windows: subprocess not supported in this test.
    return "";
#else
    int to_child[2];
    int from_child[2];
    if (pipe(to_child) != 0 || pipe(from_child) != 0) {
        return "";
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child: krdap process
        close(to_child[1]);  // close write end of to_child
        close(from_child[0]); // close read end of from_child
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        close(to_child[0]);
        close(from_child[1]);
        execl(krdap_path.c_str(), "krdap", nullptr);
        _exit(127);
    }

    // Parent: test harness
    close(to_child[0]);
    close(from_child[1]);

    // Build Content-Length prefixed message.
    std::string header =
        "Content-Length: " + std::to_string(request_json.size()) + "\r\n\r\n";
    std::string message = header + request_json;

    // Write request.
    ssize_t written = write(to_child[1], message.data(), message.size());
    close(to_child[1]);
    (void)written;

    // Read response.
    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = read(from_child[0], buf, sizeof(buf))) > 0) {
        response.append(buf, n);
    }
    close(from_child[0]);

    int status;
    waitpid(pid, &status, 0);

    // Extract body after Content-Length header.
    size_t body_start = response.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        return response;
    }
    return response.substr(body_start + 4);
#endif
}

} // namespace

TEST_CASE("krdap DAP initialize response") {
#ifdef _WIN32
    // Skip on Windows (subprocess not supported in this test).
    return;
#else
    // Find krdap binary path.
    std::string krdap_path =
        std::string(PROJECT_BINARY_DIR) + "/src/tools/krdap/krdap";

    std::string request = R"({"id":1,"method":"initialize","params":{}})";
    std::string response = dap_roundtrip(request, krdap_path);

    // DAP responses use "body" for the result content.
    std::string id1 = "\"id\":1";
    std::string body = "\"body\"";
    std::string capabilities = "\"capabilities\"";
    std::string success = "\"success\":true";
    std::string type_response = "\"type\":\"response\"";
    CHECK(response.find(id1) != std::string::npos);
    CHECK(response.find(body) != std::string::npos);
    CHECK(response.find(capabilities) != std::string::npos);
    CHECK(response.find(success) != std::string::npos);
    CHECK(response.find(type_response) != std::string::npos);
#endif
}

TEST_CASE("krdap DAP launch response") {
#ifdef _WIN32
    return;
#else
    std::string krdap_path =
        std::string(PROJECT_BINARY_DIR) + "/src/tools/krdap/krdap";

    std::string request = R"({"id":2,"method":"launch","params":{}})";
    std::string response = dap_roundtrip(request, krdap_path);

    std::string id2 = "\"id\":2";
    std::string body = "\"body\"";
    std::string success = "\"success\":true";
    std::string type_response = "\"type\":\"response\"";
    CHECK(response.find(id2) != std::string::npos);
    CHECK(response.find(body) != std::string::npos);
    CHECK(response.find(success) != std::string::npos);
    CHECK(response.find(type_response) != std::string::npos);
#endif
}

TEST_CASE("krdap DAP disconnect terminates session") {
#ifdef _WIN32
    return;
#else
    std::string krdap_path =
        std::string(PROJECT_BINARY_DIR) + "/src/tools/krdap/krdap";

    std::string request = R"({"id":3,"method":"disconnect","params":{}})";
    std::string response = dap_roundtrip(request, krdap_path);

    std::string id3 = "\"id\":3";
    std::string body = "\"body\"";
    std::string success = "\"success\":true";
    std::string type_response = "\"type\":\"response\"";
    CHECK(response.find(id3) != std::string::npos);
    CHECK(response.find(body) != std::string::npos);
    CHECK(response.find(success) != std::string::npos);
    CHECK(response.find(type_response) != std::string::npos);
#endif
}

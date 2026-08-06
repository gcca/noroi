#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

#include <gtest/gtest.h>

#define noroi_conf_port 18080
#include "noroi.h"

namespace {

static const char kResponse[] = "HTTP/1.0 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 14\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "Hello, World!\n";

bool HttpGetOnce(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
    close(fd);
    return false;
  }

  if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return false;
  }

  static const char req[] = "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n";
  if (write(fd, req, sizeof(req) - 1) < 0) {
    close(fd);
    return false;
  }

  char buf[1024];
  size_t total = 0;
  const size_t want = sizeof(kResponse) - 1;
  while (total < sizeof(buf)) {
    const ssize_t n = read(fd, buf + total, sizeof(buf) - total);
    if (n < 0) {
      close(fd);
      return false;
    }
    if (n == 0)
      break;
    total += static_cast<size_t>(n);
    if (total >= want)
      break;
  }
  close(fd);

  return total == want && std::memcmp(buf, kResponse, want) == 0;
}

bool WaitUntilAccepting(int port, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (HttpGetOnce(port))
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

struct ServerThread {
  std::atomic<int> run_rc{-1};
  std::thread thread;

  ServerThread() : thread([this] { run_rc = noroi_run(); }) {}

  ~ServerThread() {
    noroi_stop();
    if (thread.joinable())
      thread.join();
  }
};

} // namespace

extern "C" void noroi_handle(const struct noroi_req_t *,
                             struct noroi_res_t *res) {
  static char xbuf[4096];
  static const char body[] = "Hello, World!\n";
  noroi_res_set_content_cstr(res, xbuf, sizeof(xbuf), body, sizeof(body) - 1);
}

TEST(NoroiRun, HandlesAtLeast100ConnectionsPerSecond) {
  ServerThread server;

  ASSERT_TRUE(WaitUntilAccepting(noroi_conf_port, std::chrono::seconds(5)))
      << "server did not accept on port " << noroi_conf_port;

  constexpr int N = 100;
  int ok = 0;
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < N; ++i) {
    if (HttpGetOnce(noroi_conf_port))
      ++ok;
  }
  const auto t1 = std::chrono::steady_clock::now();
  const double elapsed = std::chrono::duration<double>(t1 - t0).count();

  noroi_stop();
  server.thread.join();

  EXPECT_EQ(server.run_rc.load(), 0);
  EXPECT_EQ(ok, N);
  ASSERT_GT(elapsed, 0.0);
  EXPECT_GE(static_cast<double>(N) / elapsed, 100.0)
      << "rps=" << (static_cast<double>(N) / elapsed);
}

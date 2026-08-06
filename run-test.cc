#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uv.h>

#define noroi_conf_port 18080
#include "noroi.h"

// Test-side definitions matching the private layouts in noroi.c. The public
// header intentionally exposes these types only as opaque declarations.
struct noroi_req_t {
  const uv_buf_t* buf;
  size_t nread;
};

struct noroi_res_t {
  uv_buf_t buf;
};

namespace {

bool fail_next_bufalloc = false;
bool fail_next_snfmt = false;

size_t TestBufalloc(struct noroi_res_t* res, size_t len) {
  if (std::exchange(fail_next_bufalloc, false)) {
    res->buf = uv_buf_init(nullptr, 0);
    return 1;
  }
  return noroi_res_bufalloc(res, len);
}

size_t TestSnfmt(struct noroi_res_t* res,
                 ptrdiff_t offset,
                 const char* fmt,
                 ...) {
  va_list args;
  va_start(args, fmt);
  const size_t body_size = va_arg(args, size_t);
  va_end(args);
  if (std::exchange(fail_next_snfmt, false))
    return 0;
  return noroi_res_snfmt(res, offset, fmt, body_size);
}

}  // namespace

#define noroi_res_bufalloc TestBufalloc
#define noroi_res_snfmt TestSnfmt
#define main noroi_app_main
#include "run.cc"
#undef main
#undef noroi_res_snfmt
#undef noroi_res_bufalloc

namespace {

using namespace std::chrono_literals;

std::string MakeRequest(std::string_view method, std::string_view target) {
  return std::string(method) + " " + std::string(target) +
         " HTTP/1.0\r\nHost: localhost\r\n\r\n";
}

std::string TextResponse(std::string_view status, std::string_view body) {
  return "HTTP/1.0 " + std::string(status) +
         "\r\nContent-Type: text/plain\r\nContent-Length: " +
         std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" +
         std::string(body);
}

std::string FixedBodyResponse(std::string_view status, std::string_view body) {
  return "HTTP/1.0 " + std::string(status) +
         "\r\nContent-Length: " + std::to_string(body.size()) +
         "\r\nConnection: close\r\n\r\n" + std::string(body);
}

std::string RedirectResponse(std::string_view location) {
  return "HTTP/1.0 302 Found\r\nLocation: " + std::string(location) +
         "\r\nConnection: close\r\n\r\n";
}

std::string WelcomeResponse() {
  mst::mustache welcome_template{noroi::templates::kWelcome};
  EXPECT_TRUE(welcome_template.is_valid());
  const std::string body = welcome_template.render(MakeWelcomeData());
  EXPECT_TRUE(welcome_template.is_valid());
  return "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
         "Content-Length: " +
         std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

std::string TakeResponse(struct noroi_res_t* res) {
  if (!res->buf.base)
    return {};
  std::string response(res->buf.base, res->buf.len);
  std::free(res->buf.base);
  res->buf = uv_buf_init(nullptr, 0);
  return response;
}

std::string Dispatch(std::string raw_request) {
  uv_buf_t request_buffer = uv_buf_init(raw_request.data(), raw_request.size());
  const struct noroi_req_t req = {&request_buffer, raw_request.size()};
  struct noroi_res_t res{};
  noroi_handle(&req, &res);
  return TakeResponse(&res);
}

std::optional<std::string> ExchangeWithServer(std::string_view request) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return std::nullopt;

#ifdef SO_NOSIGPIPE
  int no_sigpipe = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
  const timeval timeout = {.tv_sec = 1, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(noroi_conf_port);
  if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1 ||
      connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) !=
          0) {
    close(fd);
    return std::nullopt;
  }

  size_t sent = 0;
  while (sent < request.size()) {
    const ssize_t n = write(fd, request.data() + sent, request.size() - sent);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      close(fd);
      return std::nullopt;
    }
    sent += static_cast<size_t>(n);
  }
  shutdown(fd, SHUT_WR);

  std::string response;
  char chunk[4096];
  for (;;) {
    const ssize_t n = read(fd, chunk, sizeof(chunk));
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0) {
      close(fd);
      return std::nullopt;
    }
    if (n == 0)
      break;
    response.append(chunk, static_cast<size_t>(n));
  }
  close(fd);
  return response;
}

std::optional<std::string> WaitForResponse(std::string_view request,
                                           std::chrono::seconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto response = ExchangeWithServer(request))
      return response;
    std::this_thread::sleep_for(10ms);
  }
  return std::nullopt;
}

void CloseWalkedHandle(uv_handle_t* handle, void*) {
  if (!uv_is_closing(handle))
    uv_close(handle, nullptr);
}

void ShutdownServer(uv_async_t*) {
  uv_walk(uv_default_loop(), CloseWalkedHandle, nullptr);
}

int OccupyServerPort() {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(noroi_conf_port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0 ||
      listen(fd, 1) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

}  // namespace

TEST(RunData, MakeObjectStoresFields) {
  const mst::data empty = MakeObject({});
  EXPECT_TRUE(empty.is_empty_object());

  const mst::data object =
      MakeObject({{"label", "Overview"}, {"href", "#overview"}});
  ASSERT_TRUE(object.is_object());
  const mst::data* label = object.get("label");
  const mst::data* href = object.get("href");
  ASSERT_NE(label, nullptr);
  ASSERT_NE(href, nullptr);
  EXPECT_EQ(label->string_value(), "Overview");
  EXPECT_EQ(href->string_value(), "#overview");
  EXPECT_EQ(object.get("missing"), nullptr);
}

TEST(RunData, MakeWelcomeDataBuildsSelectedScalarsAndLists) {
  const mst::data data = MakeWelcomeData();
  ASSERT_TRUE(data.is_object());

  const mst::data* title = data.get("page_title");
  ASSERT_NE(title, nullptr);
  EXPECT_EQ(title->string_value(), "Noroi — Small server, serious velocity");

  const mst::data* nav_items = data.get("nav_items");
  ASSERT_NE(nav_items, nullptr);
  ASSERT_TRUE(nav_items->is_list());
  ASSERT_EQ(nav_items->list_value().size(), 4u);
  const mst::data& overview = nav_items->list_value().front();
  ASSERT_NE(overview.get("current"), nullptr);
  EXPECT_TRUE(overview.get("current")->is_true());

  const mst::data* features = data.get("features");
  ASSERT_NE(features, nullptr);
  ASSERT_TRUE(features->is_list());
  ASSERT_EQ(features->list_value().size(), 4u);
  const mst::data* escaped_icon = features->list_value()[2].get("icon");
  ASSERT_NE(escaped_icon, nullptr);
  EXPECT_EQ(escaped_icon->string_value(), "<>");

  const mst::data* footer_links = data.get("footer_links");
  ASSERT_NE(footer_links, nullptr);
  ASSERT_TRUE(footer_links->is_list());
  EXPECT_EQ(footer_links->list_value().size(), 3u);
}

TEST(RunHandlers, SetTemplateErrorBuildsExactResponse) {
  struct noroi_res_t res{};
  SetTemplateError(&res);
  EXPECT_EQ(TakeResponse(&res),
            TextResponse("200 OK", "Failed to render welcome template\n"));
}

TEST(RunHandlers, SetTemplateErrorHandlesAllocationFailure) {
  struct noroi_res_t res{};
  fail_next_bufalloc = true;
  SetTemplateError(&res);
  EXPECT_EQ(TakeResponse(&res), "");
}

TEST(NoroiHandle, IndexRedirectsToWelcome) {
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/")), RedirectResponse("/welcome"));
}

TEST(NoroiHandle, WelcomeGetBuildsExactHtmlResponse) {
  const std::string expected = WelcomeResponse();
  EXPECT_NE(
      expected.find("<title>Noroi — Small server, serious velocity</title>"),
      std::string::npos);
  EXPECT_NE(expected.find("&lt;&gt;"), std::string::npos);
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/welcome")), expected);
}

TEST(NoroiHandle, QueryIsPartOfTheExactRouteTarget) {
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/welcome?source=test")),
            FixedBodyResponse("404 Not Found", "Not Found\n"));
}

TEST(NoroiHandle, PostWelcomeIsMethodNotAllowed) {
  EXPECT_EQ(
      Dispatch(MakeRequest("POST", "/welcome")),
      FixedBodyResponse("405 Method Not Allowed", "Method Not Allowed\n"));
}

TEST(NoroiHandle, ThreeByteNonGetMethodIsMethodNotAllowed) {
  EXPECT_EQ(
      Dispatch(MakeRequest("PUT", "/welcome")),
      FixedBodyResponse("405 Method Not Allowed", "Method Not Allowed\n"));
}

TEST(NoroiHandle, HealthcheckBuildsExactResponse) {
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/healthcheck")),
            TextResponse("200 OK", "🍻\n"));
}

TEST(NoroiHandle, MiscBuildsExactTeapotResponse) {
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/misc")),
            TextResponse("418 I'm a teapot", "Misc"));
}

TEST(NoroiHandle, UnknownRouteIsNotFound) {
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/missing")),
            FixedBodyResponse("404 Not Found", "Not Found\n"));
}

class RoutePrefixTest : public testing::TestWithParam<const char*> {};

TEST_P(RoutePrefixTest, DoesNotMatchAFullRoute) {
  EXPECT_EQ(Dispatch(MakeRequest("GET", GetParam())),
            FixedBodyResponse("404 Not Found", "Not Found\n"));
}

INSTANTIATE_TEST_SUITE_P(ShortPrefixes,
                         RoutePrefixTest,
                         testing::Values("/w", "/wel", "/health", "/mis"));

TEST(NoroiHandleFailures, IndexHandlesAllocationFailure) {
  fail_next_bufalloc = true;
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/")), "");
}

TEST(NoroiHandleFailures, RejectedWelcomeMethodHandlesAllocationFailure) {
  fail_next_bufalloc = true;
  EXPECT_EQ(Dispatch(MakeRequest("POST", "/welcome")), "");
}

TEST(NoroiHandleFailures, WelcomeHandlesAllocationFailure) {
  fail_next_bufalloc = true;
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/welcome")), "");
}

TEST(NoroiHandleFailures, WelcomeHandlesFormattedHeaderFailure) {
  fail_next_snfmt = true;
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/welcome")),
            TextResponse("200 OK", "Failed to render welcome template\n"));
}

TEST(NoroiHandleFailures, HealthcheckHandlesAllocationFailure) {
  fail_next_bufalloc = true;
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/healthcheck")), "");
}

TEST(NoroiHandleFailures, MiscHandlesAllocationFailure) {
  fail_next_bufalloc = true;
  testing::internal::CaptureStderr();
  const std::string response = Dispatch(MakeRequest("GET", "/misc"));
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_EQ(response, "");
  EXPECT_EQ(error, "Fail allocating\n");
}

TEST(NoroiHandleFailures, NotFoundHandlesAllocationFailure) {
  fail_next_bufalloc = true;
  EXPECT_EQ(Dispatch(MakeRequest("GET", "/missing")), "");
}

TEST(NoroiRunIntegration, ReportsAnOccupiedPort) {
  const int occupied_fd = OccupyServerPort();
  ASSERT_GE(occupied_fd, 0);

  testing::internal::CaptureStderr();
  const int run_result = noroi_run();
  const std::string error = testing::internal::GetCapturedStderr();

  close(occupied_fd);
  EXPECT_EQ(run_result, EXIT_FAILURE);
  EXPECT_NE(error.find("listen:"), std::string::npos);
}

TEST(NoroiRunIntegration, ServesAnExactResponseAndStopsCleanly) {
  uv_async_t shutdown_async{};
  ASSERT_EQ(uv_async_init(uv_default_loop(), &shutdown_async, ShutdownServer),
            0);

  std::atomic<int> run_result{-1};
  std::thread server([&run_result] { run_result = noroi_app_main(); });

  const std::string request = MakeRequest("GET", "/welcome");
  const std::optional<std::string> response = WaitForResponse(request, 5s);

  EXPECT_EQ(uv_async_send(&shutdown_async), 0);
  server.join();

  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(*response, WelcomeResponse());
  EXPECT_EQ(run_result.load(), 0);
}

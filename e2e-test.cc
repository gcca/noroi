#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uv.h>

#define noroi_conf_port 18081
#include "noroi.h"
#define main noroi_app_main
#include "run.cc"
#undef main

namespace {

using namespace std::chrono_literals;

constexpr char kDatabasePath[] = "build/e2e-test.db";

constexpr long kExchangeTimeoutSeconds = 5;

std::string MakeRequest(std::string_view method, std::string_view target) {
  std::string request;
  request.append(method).append(" ").append(target).append(" HTTP/1.1\r\n");
  request.append("Host: 127.0.0.1:")
      .append(std::to_string(noroi_conf_port))
      .append("\r\n");
  request.append("User-Agent: noroi-e2e-test/1.0\r\n");
  request.append("Accept-Encoding: gzip, deflate\r\n");
  request.append("Accept: */*\r\n");
  request.append("Connection: keep-alive\r\n");
  if (method != "GET")
    request.append("Content-Length: 0\r\n");
  request.append("\r\n");
  return request;
}

std::optional<std::string> Exchange(std::string_view request) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return std::nullopt;

#ifdef SO_NOSIGPIPE
  int no_sigpipe = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
  const timeval timeout = {.tv_sec = kExchangeTimeoutSeconds, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(noroi_conf_port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) !=
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

struct Response {
  std::string raw;
  std::string status_line;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
};

std::optional<Response> ParseResponse(std::string raw) {
  const size_t head_end = raw.find("\r\n\r\n");
  if (head_end == std::string::npos)
    return std::nullopt;

  Response response;
  response.body = raw.substr(head_end + 4);

  const size_t status_end = raw.find("\r\n");
  response.status_line = raw.substr(0, status_end);

  size_t line_start = status_end + 2;
  while (line_start < head_end) {
    const size_t line_end = raw.find("\r\n", line_start);
    const size_t colon = raw.find(':', line_start);
    if (colon == std::string::npos || colon > line_end)
      return std::nullopt;
    size_t value_start = colon + 1;
    while (value_start < line_end && raw[value_start] == ' ')
      ++value_start;
    response.headers.emplace_back(
        raw.substr(line_start, colon - line_start),
        raw.substr(value_start, line_end - value_start));
    line_start = line_end + 2;
  }

  response.raw = std::move(raw);
  return response;
}

std::optional<Response> Send(std::string_view method, std::string_view target) {
  const std::optional<std::string> raw = Exchange(MakeRequest(method, target));
  if (!raw)
    return std::nullopt;
  return ParseResponse(*raw);
}

std::optional<std::string> Header(const Response& response,
                                  std::string_view name) {
  for (const auto& [field, value] : response.headers)
    if (field == name)
      return value;
  return std::nullopt;
}

void ExpectClosedFraming(const Response& response) {
  EXPECT_EQ(Header(response, "Connection").value_or(""), "close");
  const std::optional<std::string> length = Header(response, "Content-Length");
  ASSERT_TRUE(length.has_value()) << "missing Content-Length";
  EXPECT_EQ(*length, std::to_string(response.body.size()));
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

struct DatabaseFacts {
  long long users = 0;
  long long active_users = 0;
  long long apps = 0;
  long long bindings = 0;
  std::string usernames;
  std::string appnames;
};

std::optional<std::string> QueryText(sqlite3* db, const char* sql) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return std::nullopt;
  std::optional<std::string> value;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    value = ColumnText(stmt, 0);
  sqlite3_finalize(stmt);
  return value;
}

std::optional<long long> QueryCount(sqlite3* db, const char* sql) {
  const std::optional<std::string> value = QueryText(db, sql);
  if (!value)
    return std::nullopt;
  return std::strtoll(value->c_str(), nullptr, 10);
}

std::optional<DatabaseFacts> ReadDatabaseFacts() {
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(kDatabasePath, &db, SQLITE_OPEN_READONLY, nullptr) !=
      SQLITE_OK) {
    sqlite3_close(db);
    return std::nullopt;
  }
  const DatabaseGuard guard{db};

  const std::optional<long long> users =
      QueryCount(db, "SELECT COUNT(*) FROM auth_user;");
  const std::optional<long long> active_users =
      QueryCount(db, "SELECT COUNT(*) FROM auth_user WHERE is_active = 1;");
  const std::optional<long long> apps =
      QueryCount(db, "SELECT COUNT(*) FROM dash_app;");
  const std::optional<long long> bindings =
      QueryCount(db, "SELECT COUNT(*) FROM dash_binding;");
  const std::optional<std::string> usernames =
      QueryText(db,
                "SELECT group_concat(username, ',')"
                "  FROM (SELECT username FROM auth_user"
                "         ORDER BY username);");
  const std::optional<std::string> appnames =
      QueryText(db,
                "SELECT group_concat(appname, ',')"
                "  FROM (SELECT appname FROM dash_app ORDER BY appname);");
  if (!users || !active_users || !apps || !bindings || !usernames || !appnames)
    return std::nullopt;

  return DatabaseFacts{*users,    *active_users, *apps,
                       *bindings, *usernames,    *appnames};
}

class JsonDocument {
 public:
  explicit JsonDocument(const std::string& text)
      : doc_(yyjson_read(text.data(), text.size(), 0)) {}
  JsonDocument(const JsonDocument&) = delete;
  JsonDocument& operator=(const JsonDocument&) = delete;
  ~JsonDocument() { yyjson_doc_free(doc_); }

  yyjson_val* root() const { return yyjson_doc_get_root(doc_); }

 private:
  yyjson_doc* const doc_;
};

std::string StringField(yyjson_val* object, const char* key) {
  const char* const value = yyjson_get_str(yyjson_obj_get(object, key));
  return value ? value : "";
}

long long IntField(yyjson_val* object, const char* key) {
  return yyjson_get_sint(yyjson_obj_get(object, key));
}

std::string JoinNames(yyjson_val* array, const char* key) {
  std::string names;
  size_t index, max;
  yyjson_val* entry;
  yyjson_arr_foreach(array, index, max, entry) {
    if (index)
      names += ',';
    names += StringField(entry, key);
  }
  return names;
}

void CloseWalkedHandle(uv_handle_t* handle, void*) {
  if (!uv_is_closing(handle))
    uv_close(handle, nullptr);
}

void ShutdownServer(uv_async_t*) {
  uv_walk(uv_default_loop(), CloseWalkedHandle, nullptr);
}

class ServerEnvironment : public testing::Environment {
 public:
  void SetUp() override {
    ASSERT_FALSE(ReadFile(SampleDataPath()).empty())
        << "Cannot read " << SampleDataPath()
        << "; run the e2e test from the repository root.";
    std::remove(kDatabasePath);
    ASSERT_EQ(setenv("NOROI_DATABASE", kDatabasePath, 1), 0);

    ASSERT_EQ(uv_async_init(uv_default_loop(), &shutdown_, ShutdownServer), 0);
    server_ = std::thread([this] { status_ = noroi_app_main(); });
    ASSERT_TRUE(WaitUntilServing(10s))
        << "the server never answered on port " << noroi_conf_port;
  }

  void TearDown() override {
    EXPECT_EQ(uv_async_send(&shutdown_), 0);
    server_.join();
    EXPECT_EQ(status_.load(), 0);
    std::remove(kDatabasePath);
  }

 private:
  bool WaitUntilServing(std::chrono::seconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      if (Exchange(MakeRequest("GET", "/healthcheck")))
        return true;
      std::this_thread::sleep_for(10ms);
    }
    return false;
  }

  uv_async_t shutdown_{};
  std::thread server_;
  std::atomic<int> status_{-1};
};

[[maybe_unused]] const testing::Environment* const kServerEnvironment =
    testing::AddGlobalTestEnvironment(new ServerEnvironment);

}  // namespace

TEST(NoroiE2E, PostWelcomeIsMethodNotAllowed) {
  const std::optional<Response> response = Send("POST", "/welcome");
  ASSERT_TRUE(response.has_value());

  EXPECT_EQ(response->status_line, "HTTP/1.0 405 Method Not Allowed");
  ExpectClosedFraming(*response);
  EXPECT_FALSE(Header(*response, "Content-Type").has_value());
  EXPECT_FALSE(Header(*response, "Allow").has_value());
  EXPECT_EQ(response->body, "Method Not Allowed\n");
  EXPECT_EQ(response->raw, FixedBodyResponse("405 Method Not Allowed",
                                             "Method Not Allowed\n"));
}

TEST(NoroiE2E, HealthcheckServesPlainTextBody) {
  const std::optional<Response> response = Send("GET", "/healthcheck");
  ASSERT_TRUE(response.has_value());

  EXPECT_EQ(response->status_line, "HTTP/1.0 200 OK");
  EXPECT_EQ(Header(*response, "Content-Type").value_or(""), "text/plain");
  ExpectClosedFraming(*response);
  EXPECT_EQ(response->body, "🍻\n");
  EXPECT_EQ(response->body.size(), 5u);
  EXPECT_EQ(response->raw, TextResponse("200 OK", "🍻\n"));
}

TEST(NoroiE2E, WelcomeServesRenderedHtml) {
  const std::optional<Response> response = Send("GET", "/welcome");
  ASSERT_TRUE(response.has_value());

  EXPECT_EQ(response->status_line, "HTTP/1.0 200 OK");
  EXPECT_EQ(Header(*response, "Content-Type").value_or(""),
            "text/html; charset=utf-8");
  ExpectClosedFraming(*response);

  const std::string& body = response->body;
  EXPECT_TRUE(body.starts_with("<!doctype html>\n"));
  EXPECT_TRUE(body.ends_with("</html>\n"));
  EXPECT_NE(body.find("<title>Noroi — Small server, serious velocity</title>"),
            std::string::npos);
  EXPECT_NE(body.find("&lt;&gt;"), std::string::npos);
  EXPECT_EQ(body.find("{{"), std::string::npos) << "unrendered Mustache tag";

  mst::mustache welcome_template{noroi::templates::kWelcome};
  ASSERT_TRUE(welcome_template.is_valid());
  const std::string expected = welcome_template.render(MakeWelcomeData());
  ASSERT_TRUE(welcome_template.is_valid());
  EXPECT_EQ(body, expected);
}

TEST(NoroiE2E, DashboardServesJsonBuiltFromSqlite) {
  const std::optional<DatabaseFacts> facts = ReadDatabaseFacts();
  ASSERT_TRUE(facts.has_value()) << "cannot read " << kDatabasePath;
  ASSERT_GT(facts->users, 0) << "the sample database is empty";

  const std::optional<Response> response = Send("GET", "/dashboard");
  ASSERT_TRUE(response.has_value());

  EXPECT_EQ(response->status_line, "HTTP/1.0 200 OK");
  EXPECT_EQ(Header(*response, "Content-Type").value_or(""),
            "application/json; charset=utf-8");
  ExpectClosedFraming(*response);

  const JsonDocument document(response->body);
  yyjson_val* const root = document.root();
  ASSERT_TRUE(yyjson_is_obj(root)) << "body is not a JSON object";
  EXPECT_EQ(StringField(root, "service"), "noroi");
  EXPECT_EQ(StringField(root, "endpoint"), "/api/dashboard");

  yyjson_val* const source = yyjson_obj_get(root, "source");
  ASSERT_TRUE(yyjson_is_obj(source));
  EXPECT_EQ(StringField(source, "engine"), "sqlite");
  EXPECT_EQ(StringField(source, "library_version"), sqlite3_libversion());
  EXPECT_EQ(StringField(source, "database"), kDatabasePath);
  EXPECT_EQ(StringField(source, "script"), SampleDataPath());

  yyjson_val* const totals = yyjson_obj_get(root, "totals");
  ASSERT_TRUE(yyjson_is_obj(totals));
  EXPECT_EQ(IntField(totals, "users"), facts->users);
  EXPECT_EQ(IntField(totals, "active_users"), facts->active_users);
  EXPECT_EQ(IntField(totals, "apps"), facts->apps);
  EXPECT_EQ(IntField(totals, "bindings"), facts->bindings);

  yyjson_val* const users = yyjson_obj_get(root, "users");
  ASSERT_TRUE(yyjson_is_arr(users));
  EXPECT_EQ(yyjson_arr_size(users), static_cast<size_t>(facts->users));
  EXPECT_EQ(JoinNames(users, "username"), facts->usernames);

  yyjson_val* const apps = yyjson_obj_get(root, "apps");
  ASSERT_TRUE(yyjson_is_arr(apps));
  EXPECT_EQ(yyjson_arr_size(apps), static_cast<size_t>(facts->apps));
  EXPECT_EQ(JoinNames(apps, "appname"), facts->appnames);

  long long apps_per_user = 0;
  size_t index, max;
  yyjson_val* user;
  yyjson_arr_foreach(users, index, max, user) {
    SCOPED_TRACE(StringField(user, "username"));
    EXPECT_NE(StringField(user, "email"), "");
    EXPECT_TRUE(yyjson_is_bool(yyjson_obj_get(user, "is_active")));
    EXPECT_GT(IntField(user, "created_at"), 0);
    yyjson_val* const bound = yyjson_obj_get(user, "apps");
    ASSERT_TRUE(yyjson_is_arr(bound));
    EXPECT_EQ(IntField(user, "app_count"),
              static_cast<long long>(yyjson_arr_size(bound)));
    apps_per_user += static_cast<long long>(yyjson_arr_size(bound));
  }
  EXPECT_EQ(apps_per_user, facts->bindings);

  long long users_per_app = 0;
  yyjson_val* app;
  yyjson_arr_foreach(apps, index, max, app) {
    SCOPED_TRACE(StringField(app, "appname"));
    EXPECT_NE(StringField(app, "description"), "");
    yyjson_val* const bound = yyjson_obj_get(app, "users");
    ASSERT_TRUE(yyjson_is_arr(bound));
    EXPECT_EQ(IntField(app, "user_count"),
              static_cast<long long>(yyjson_arr_size(bound)));
    users_per_app += static_cast<long long>(yyjson_arr_size(bound));
  }
  EXPECT_EQ(users_per_app, facts->bindings);
}

TEST(NoroiE2E, UnroutedPathIsNotFound) {
  const std::optional<Response> response = Send("GET", "/not-found");
  ASSERT_TRUE(response.has_value());

  EXPECT_EQ(response->status_line, "HTTP/1.0 404 Not Found");
  ExpectClosedFraming(*response);
  EXPECT_FALSE(Header(*response, "Content-Type").has_value());
  EXPECT_EQ(response->body, "Not Found\n");
  EXPECT_EQ(response->raw, FixedBodyResponse("404 Not Found", "Not Found\n"));
}

TEST(NoroiE2E, MiscServesTheTeapotStatus) {
  const std::optional<Response> response = Send("GET", "/misc");
  ASSERT_TRUE(response.has_value());

  EXPECT_EQ(response->status_line, "HTTP/1.0 418 I'm a teapot");
  EXPECT_EQ(Header(*response, "Content-Type").value_or(""), "text/plain");
  ExpectClosedFraming(*response);
  EXPECT_EQ(response->body, "Misc");
  EXPECT_EQ(response->raw, TextResponse("418 I'm a teapot", "Misc"));
}

TEST(NoroiE2E, GetEchoesQueryPairsInOrder) {
  const std::optional<Response> response =
      Send("GET", "/get?v1=abc&v2=123&v1=def");
  ASSERT_TRUE(response.has_value());

  EXPECT_EQ(response->status_line, "HTTP/1.0 200 OK");
  EXPECT_EQ(Header(*response, "Content-Type").value_or(""), "text/plain");
  ExpectClosedFraming(*response);
  EXPECT_EQ(response->body, R"([["v1","abc"],["v2","123"],["v1","def"]])");

  const JsonDocument document(response->body);
  yyjson_val* const root = document.root();
  ASSERT_TRUE(yyjson_is_arr(root)) << "body is not a JSON array";
  ASSERT_EQ(yyjson_arr_size(root), 3u);

  static constexpr const char* kExpected[][2] = {
      {"v1", "abc"}, {"v2", "123"}, {"v1", "def"}};
  size_t index, max;
  yyjson_val* pair;
  yyjson_arr_foreach(root, index, max, pair) {
    SCOPED_TRACE(index);
    ASSERT_TRUE(yyjson_is_arr(pair));
    ASSERT_EQ(yyjson_arr_size(pair), 2u);
    EXPECT_STREQ(yyjson_get_str(yyjson_arr_get(pair, 0)), kExpected[index][0]);
    EXPECT_STREQ(yyjson_get_str(yyjson_arr_get(pair, 1)), kExpected[index][1]);
  }
}

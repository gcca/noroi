#include <cstring>

#include <gtest/gtest.h>

#include "noroi.h"

extern "C" void noroi_handle(const struct noroi_req_t*, struct noroi_res_t*) {}

namespace {

void ParseReqLine(const char* raw, size_t nread, const char** method,
                  size_t* method_size, const char** path, size_t* path_size,
                  const char** query, size_t* query_size) {
  uv_buf_t buf = uv_buf_init(const_cast<char*>(raw), nread);
  struct noroi_req_t req = {&buf, nread};
  noroi_req_parse_url(&req, method, method_size, path, path_size,
                                query, query_size);
}

} // namespace

TEST(NoroiReqParseUrl, HappyGet) {
  static const char raw[] = "GET / HTTP/1.0\r\n\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 0;
  size_t path_size = 0;
  size_t query_size = 99;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 3u);
  EXPECT_EQ(path_size, 1u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, raw);
  EXPECT_EQ(path, raw + 4);
  EXPECT_EQ(query, nullptr);
  EXPECT_EQ(std::memcmp(method, "GET", 3), 0);
  EXPECT_EQ(std::memcmp(path, "/", 1), 0);
}

TEST(NoroiReqParseUrl, PathWithQuery) {
  static const char raw[] = "GET /a?b=1 HTTP/1.1\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* query = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;
  size_t query_size = 0;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 3u);
  EXPECT_EQ(path_size, 2u);
  EXPECT_EQ(query_size, 3u);
  EXPECT_EQ(method, raw);
  EXPECT_EQ(path, raw + 4);
  EXPECT_EQ(query, raw + 7);
  EXPECT_EQ(std::memcmp(method, "GET", 3), 0);
  EXPECT_EQ(std::memcmp(path, "/a", 2), 0);
  EXPECT_EQ(std::memcmp(query, "b=1", 3), 0);
}

TEST(NoroiReqParseUrl, EmptyQuery) {
  static const char raw[] = "GET /a? HTTP/1.1\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* query = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;
  size_t query_size = 99;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 3u);
  EXPECT_EQ(path_size, 2u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(path, raw + 4);
  EXPECT_EQ(query, raw + 7);
  EXPECT_EQ(std::memcmp(path, "/a", 2), 0);
}

TEST(NoroiReqParseUrl, Post) {
  static const char raw[] = "POST /api HTTP/1.0\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 0;
  size_t path_size = 0;
  size_t query_size = 99;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 4u);
  EXPECT_EQ(path_size, 4u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, raw);
  EXPECT_EQ(path, raw + 5);
  EXPECT_EQ(query, nullptr);
  EXPECT_EQ(std::memcmp(method, "POST", 4), 0);
  EXPECT_EQ(std::memcmp(path, "/api", 4), 0);
}

TEST(NoroiReqParseUrl, IgnoresInitialSizes) {
  static const char raw[] = "GET / HTTP/1.0\r\n\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* query = nullptr;
  size_t method_size = 2;
  size_t path_size = 1;
  size_t query_size = 1;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 3u);
  EXPECT_EQ(path_size, 1u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(std::memcmp(method, "GET", 3), 0);
  EXPECT_EQ(std::memcmp(path, "/", 1), 0);
}

TEST(NoroiReqParseUrl, MalformedNoSpaces) {
  static const char raw[] = "GET/\r\n";
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, MalformedNoSecondSp) {
  static const char raw[] = "GET /only\r\n";
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, NullReq) {
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  noroi_req_parse_url(nullptr, &method, &method_size, &path, &path_size, &query,
                      &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, NullBuf) {
  struct noroi_req_t req = {nullptr, 0};
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  noroi_req_parse_url(&req, &method, &method_size, &path, &path_size, &query,
                      &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, NullBase) {
  uv_buf_t buf = uv_buf_init(nullptr, 0);
  struct noroi_req_t req = {&buf, 0};
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  noroi_req_parse_url(&req, &method, &method_size, &path, &path_size, &query,
                      &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, NullMethodOut) {
  static const char raw[] = "GET / HTTP/1.0\r\n";
  uv_buf_t buf = uv_buf_init(const_cast<char*>(raw), sizeof(raw) - 1);
  struct noroi_req_t req = {&buf, sizeof(raw) - 1};
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  noroi_req_parse_url(&req, nullptr, &method_size, &path, &path_size, &query,
                      &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, NullPathOut) {
  static const char raw[] = "GET / HTTP/1.0\r\n";
  uv_buf_t buf = uv_buf_init(const_cast<char*>(raw), sizeof(raw) - 1);
  struct noroi_req_t req = {&buf, sizeof(raw) - 1};
  const char* method = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  noroi_req_parse_url(&req, &method, &method_size, nullptr, &path_size, &query,
                      &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, EmptyMethod) {
  static const char raw[] = " / HTTP/1.0\r\n";
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, EmptyPath) {
  static const char raw[] = "GET  HTTP/1.0\r\n";
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, QueryMissingTrailingSp) {
  static const char raw[] = "GET /a?b=1";
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, NreadCappedAt2048) {
  char raw[2064];
  std::memcpy(raw, "GET /", 5);
  std::memset(raw + 5, 'x', 2044);
  std::memcpy(raw + 2049, " HTTP/1.0\r\n", 11);
  const size_t nread = 2060;
  const char* method = reinterpret_cast<const char*>(1);
  const char* path = reinterpret_cast<const char*>(1);
  const char* query = reinterpret_cast<const char*>(1);
  size_t method_size = 99;
  size_t path_size = 99;
  size_t query_size = 99;
  ParseReqLine(raw, nread, &method, &method_size, &path, &path_size, &query,
               &query_size);
  EXPECT_EQ(method_size, 0u);
  EXPECT_EQ(path_size, 0u);
  EXPECT_EQ(query_size, 0u);
  EXPECT_EQ(method, nullptr);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(query, nullptr);
}

TEST(NoroiReqParseUrl, NullQuerySizeSkipsQuery) {
  static const char raw[] = "GET /a?b=1 HTTP/1.1\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* query = reinterpret_cast<const char*>(0xdead);
  size_t method_size = 0;
  size_t path_size = 0;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, nullptr);
  EXPECT_EQ(method_size, 3u);
  EXPECT_EQ(path_size, 2u);
  EXPECT_EQ(method, raw);
  EXPECT_EQ(path, raw + 4);
  EXPECT_EQ(query, nullptr);
  EXPECT_EQ(std::memcmp(method, "GET", 3), 0);
  EXPECT_EQ(std::memcmp(path, "/a", 2), 0);
}

TEST(NoroiReqParseUrl, RootWithQuery) {
  static const char raw[] = "GET /?x=1 HTTP/1.1\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* query = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;
  size_t query_size = 0;
  ParseReqLine(raw, sizeof(raw) - 1, &method, &method_size, &path, &path_size,
               &query, &query_size);
  EXPECT_EQ(method_size, 3u);
  EXPECT_EQ(path_size, 1u);
  EXPECT_EQ(query_size, 3u);
  EXPECT_EQ(method, raw);
  EXPECT_EQ(path, raw + 4);
  EXPECT_EQ(query, raw + 6);
  EXPECT_EQ(std::memcmp(method, "GET", 3), 0);
  EXPECT_EQ(std::memcmp(path, "/", 1), 0);
  EXPECT_EQ(std::memcmp(query, "x=1", 3), 0);
}

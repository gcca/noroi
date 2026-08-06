#include <cstring>
#include <string_view>

#include <gtest/gtest.h>
#include <uv.h>

#include "noroi.h"

struct noroi_req_t {
  const uv_buf_t* buf;
  size_t nread;
};

extern "C" void noroi_handle(const struct noroi_req_t*, struct noroi_res_t*) {}

namespace {

void ParseReqLineMp(const char* raw,
                    size_t nread,
                    const char** method,
                    size_t* method_size,
                    const char** path,
                    size_t* path_size) {
  uv_buf_t buf = uv_buf_init(const_cast<char*>(raw), nread);
  struct noroi_req_t req = {&buf, nread};
  noroi_req_parse_url_mp(&req, method, method_size, path, path_size);
}

void ParseReqLineMpq(const char* raw,
                     size_t nread,
                     const char** method,
                     size_t* method_size,
                     const char** path,
                     size_t* path_size,
                     const char** query,
                     size_t* query_size) {
  uv_buf_t buf = uv_buf_init(const_cast<char*>(raw), nread);
  struct noroi_req_t req = {&buf, nread};
  noroi_req_parse_url_mpq(&req, method, method_size, path, path_size, query,
                          query_size);
}

void ExpectSlice(const char* actual,
                 size_t actual_size,
                 const char* expected_start,
                 std::string_view expected) {
  ASSERT_EQ(actual, expected_start);
  ASSERT_EQ(actual_size, expected.size());
  EXPECT_EQ(std::memcmp(actual, expected.data(), expected.size()), 0);
}

}  // namespace

TEST(NoroiReqParseUrlMp, ExtractsRootRoute) {
  static const char raw[] = "GET / HTTP/1.0\r\n\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;

  ParseReqLineMp(raw, sizeof(raw) - 1, &method, &method_size, &path,
                 &path_size);

  ExpectSlice(method, method_size, raw, "GET");
  ExpectSlice(path, path_size, raw + 4, "/");
}

TEST(NoroiReqParseUrlMp, ExtractsSampleApplicationRoute) {
  static const char raw[] = "POST /welcome HTTP/1.1\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;

  ParseReqLineMp(raw, sizeof(raw) - 1, &method, &method_size, &path,
                 &path_size);

  ExpectSlice(method, method_size, raw, "POST");
  ExpectSlice(path, path_size, raw + 5, "/welcome");
}

TEST(NoroiReqParseUrlMp, IncludesQueryInRequestTarget) {
  static const char raw[] = "GET /welcome?source=test HTTP/1.1\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;

  ParseReqLineMp(raw, sizeof(raw) - 1, &method, &method_size, &path,
                 &path_size);

  ExpectSlice(method, method_size, raw, "GET");
  ExpectSlice(path, path_size, raw + 4, "/welcome?source=test");
}

TEST(NoroiReqParseUrlMp, AcceptsUnvalidatedFields) {
  static const char raw[] = "BREW dashboard HTTP/0.9\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;

  ParseReqLineMp(raw, sizeof(raw) - 1, &method, &method_size, &path,
                 &path_size);

  ExpectSlice(method, method_size, raw, "BREW");
  ExpectSlice(path, path_size, raw + 5, "dashboard");
}

TEST(NoroiReqParseUrlMp, MatchesMpqMethodAndRequestTarget) {
  static const char raw[] = "PATCH /items/42?draft=1 HTTP/1.1 rest";
  const char* mp_method = nullptr;
  const char* mp_path = nullptr;
  size_t mp_method_size = 0;
  size_t mp_path_size = 0;
  ParseReqLineMp(raw, sizeof(raw) - 1, &mp_method, &mp_method_size, &mp_path,
                 &mp_path_size);

  const char* mpq_method = nullptr;
  const char* mpq_path = nullptr;
  const char* following = nullptr;
  size_t mpq_method_size = 0;
  size_t mpq_path_size = 0;
  size_t following_size = 0;
  ParseReqLineMpq(raw, sizeof(raw) - 1, &mpq_method, &mpq_method_size,
                  &mpq_path, &mpq_path_size, &following, &following_size);

  EXPECT_EQ(mp_method, mpq_method);
  EXPECT_EQ(mp_method_size, mpq_method_size);
  EXPECT_EQ(mp_path, mpq_path);
  EXPECT_EQ(mp_path_size, mpq_path_size);
  ExpectSlice(following, following_size, raw + 24, "HTTP/1.1");
}

TEST(NoroiReqParseUrlMpq, ExtractsFollowingHttpVersionField) {
  static const char raw[] = "GET /misc HTTP/1.0 next";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* following = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;
  size_t following_size = 0;

  ParseReqLineMpq(raw, sizeof(raw) - 1, &method, &method_size, &path,
                  &path_size, &following, &following_size);

  ExpectSlice(method, method_size, raw, "GET");
  ExpectSlice(path, path_size, raw + 4, "/misc");
  ExpectSlice(following, following_size, raw + 10, "HTTP/1.0");
}

TEST(NoroiReqParseUrlMpq, ScansAcrossCrLfUntilSupportedDelimiter) {
  static const char raw[] =
      "GET /welcome HTTP/1.0\r\n"
      "Host: localhost\r\n"
      "\r\n";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* following = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;
  size_t following_size = 0;

  ParseReqLineMpq(raw, sizeof(raw) - 1, &method, &method_size, &path,
                  &path_size, &following, &following_size);

  ExpectSlice(method, method_size, raw, "GET");
  ExpectSlice(path, path_size, raw + 4, "/welcome");
  ExpectSlice(following, following_size, raw + 13, "HTTP/1.0\r\nHost:");
}

TEST(NoroiReqParseUrlMpq, KeepsUriQueryInRequestTarget) {
  static const char raw[] = "GET /a?b=1 HTTP/1.1 next";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* following = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;
  size_t following_size = 0;

  ParseReqLineMpq(raw, sizeof(raw) - 1, &method, &method_size, &path,
                  &path_size, &following, &following_size);

  ExpectSlice(method, method_size, raw, "GET");
  ExpectSlice(path, path_size, raw + 4, "/a?b=1");
  ExpectSlice(following, following_size, raw + 11, "HTTP/1.1");
}

TEST(NoroiReqParseUrlMpq, QuestionMarkTerminatesThirdSlice) {
  static const char raw[] = "GET / HTTP/1.1?ignored";
  const char* method = nullptr;
  const char* path = nullptr;
  const char* following = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;
  size_t following_size = 0;

  ParseReqLineMpq(raw, sizeof(raw) - 1, &method, &method_size, &path,
                  &path_size, &following, &following_size);

  ExpectSlice(method, method_size, raw, "GET");
  ExpectSlice(path, path_size, raw + 4, "/");
  ExpectSlice(following, following_size, raw + 6, "HTTP/1.1");
}

TEST(NoroiReqParseUrlMpq, OverwritesInitialOutputs) {
  static const char raw[] = "POST /dashboard HTTP/1.0 next";
  const char* const sentinel = reinterpret_cast<const char*>(1);
  const char* method = sentinel;
  const char* path = sentinel;
  const char* following = sentinel;
  size_t method_size = 99;
  size_t path_size = 99;
  size_t following_size = 99;

  ParseReqLineMpq(raw, sizeof(raw) - 1, &method, &method_size, &path,
                  &path_size, &following, &following_size);

  ExpectSlice(method, method_size, raw, "POST");
  ExpectSlice(path, path_size, raw + 5, "/dashboard");
  ExpectSlice(following, following_size, raw + 16, "HTTP/1.0");
}

TEST(NoroiReqParseUrlMpq, DelimiterMayBeAtLowElevenBitThreshold) {
  char raw[2055];
  std::memset(raw, 'x', sizeof(raw));
  std::memcpy(raw, "G / HT ", 7);
  constexpr size_t nread = 2054;
  static_assert((nread & 0x7ff) == 6);

  const char* method = nullptr;
  const char* path = nullptr;
  const char* following = nullptr;
  size_t method_size = 0;
  size_t path_size = 0;
  size_t following_size = 0;

  ParseReqLineMpq(raw, nread, &method, &method_size, &path, &path_size,
                  &following, &following_size);

  ExpectSlice(method, method_size, raw, "G");
  ExpectSlice(path, path_size, raw + 2, "/");
  ExpectSlice(following, following_size, raw + 4, "HT");
}

#include <cstring>

#include <gtest/gtest.h>

#include "noroi.h"

extern "C" void noroi_handle(const struct noroi_req_t*, struct noroi_res_t*) {}

TEST(NoroiResSetContentCstr, BuildsExactMessage) {
  static const char body[] = "Hello, World!\n";
  const size_t body_len = sizeof(body) - 1;
  char xbuf[256];
  struct noroi_res_t res;
  noroi_res_set_content_cstr(&res, xbuf, sizeof(xbuf), body, body_len);

  static const char expected[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 14\r\n"
      "Connection: close\r\n"
      "\r\n"
      "Hello, World!\n";
  const size_t expected_len = sizeof(expected) - 1;

  EXPECT_EQ(res.buf.base, xbuf);
  EXPECT_EQ(res.buf.len, expected_len);
  EXPECT_EQ(std::memcmp(res.buf.base, expected, expected_len), 0);
}

TEST(NoroiResNotFound, BuildsExactMessage) {
  char xbuf[256];
  struct noroi_res_t res;
  noroi_res_not_found(&res, xbuf, sizeof(xbuf));

  static const char expected[] =
      "HTTP/1.0 404 Not Found\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 10\r\n"
      "Connection: close\r\n"
      "\r\n"
      "Not Found\n";
  const size_t expected_len = sizeof(expected) - 1;

  EXPECT_EQ(res.buf.base, xbuf);
  EXPECT_EQ(res.buf.len, expected_len);
  EXPECT_EQ(std::memcmp(res.buf.base, expected, expected_len), 0);
}

TEST(NoroiResRedirect, BuildsExactMessage) {
  static const char url[] = "/welcome";
  const size_t url_len = sizeof(url) - 1;
  char xbuf[256];
  struct noroi_res_t res;
  noroi_res_redirect(&res, xbuf, sizeof(xbuf), url, url_len);

  static const char expected[] =
      "HTTP/1.0 302 Found\r\n"
      "Location: /welcome\r\n"
      "Content-Length: 0\r\n"
      "Connection: close\r\n"
      "\r\n";
  const size_t expected_len = sizeof(expected) - 1;

  EXPECT_EQ(res.buf.base, xbuf);
  EXPECT_EQ(res.buf.len, expected_len);
  EXPECT_EQ(std::memcmp(res.buf.base, expected, expected_len), 0);
}

TEST(NoroiResSetContentCstr, EmptyBody) {
  char xbuf[256];
  struct noroi_res_t res;
  noroi_res_set_content_cstr(&res, xbuf, sizeof(xbuf), "", 0);

  static const char expected[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 0\r\n"
      "Connection: close\r\n"
      "\r\n";
  const size_t expected_len = sizeof(expected) - 1;

  EXPECT_EQ(res.buf.base, xbuf);
  EXPECT_EQ(res.buf.len, expected_len);
  EXPECT_EQ(std::memcmp(res.buf.base, expected, expected_len), 0);
}

TEST(NoroiResSetContentCstr, ThreeDigitContentLength) {
  char body[100];
  std::memset(body, 'a', sizeof(body));
  char xbuf[512];
  struct noroi_res_t res;
  noroi_res_set_content_cstr(&res, xbuf, sizeof(xbuf), body, sizeof(body));

  static const char expected_prefix[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 100\r\n"
      "Connection: close\r\n"
      "\r\n";
  const size_t prefix_len = sizeof(expected_prefix) - 1;
  const size_t expected_len = prefix_len + sizeof(body);

  EXPECT_EQ(res.buf.base, xbuf);
  EXPECT_EQ(res.buf.len, expected_len);
  EXPECT_EQ(std::memcmp(res.buf.base, expected_prefix, prefix_len), 0);
  EXPECT_EQ(std::memcmp(res.buf.base + prefix_len, body, sizeof(body)), 0);
}

TEST(NoroiResRedirect, EmptyLocation) {
  char xbuf[256];
  struct noroi_res_t res;
  noroi_res_redirect(&res, xbuf, sizeof(xbuf), "", 0);

  static const char expected[] =
      "HTTP/1.0 302 Found\r\n"
      "Location: \r\n"
      "Content-Length: 0\r\n"
      "Connection: close\r\n"
      "\r\n";
  const size_t expected_len = sizeof(expected) - 1;

  EXPECT_EQ(res.buf.base, xbuf);
  EXPECT_EQ(res.buf.len, expected_len);
  EXPECT_EQ(std::memcmp(res.buf.base, expected, expected_len), 0);
}

TEST(NoroiResRedirect, AbsoluteUrl) {
  static const char url[] = "https://example.com/x";
  const size_t url_len = sizeof(url) - 1;
  char xbuf[256];
  struct noroi_res_t res;
  noroi_res_redirect(&res, xbuf, sizeof(xbuf), url, url_len);

  static const char expected[] =
      "HTTP/1.0 302 Found\r\n"
      "Location: https://example.com/x\r\n"
      "Content-Length: 0\r\n"
      "Connection: close\r\n"
      "\r\n";
  const size_t expected_len = sizeof(expected) - 1;

  EXPECT_EQ(res.buf.base, xbuf);
  EXPECT_EQ(res.buf.len, expected_len);
  EXPECT_EQ(std::memcmp(res.buf.base, expected, expected_len), 0);
}

#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <uv.h>

#include "noroi.h"

// Test-only mirror of the private layout in noroi.c. The public header keeps
// this type opaque, but response tests need to inspect the serialized bytes.
struct noroi_res_t {
  uv_buf_t buf;
};

extern "C" void noroi_handle(const struct noroi_req_t*, struct noroi_res_t*) {}

namespace {

constexpr unsigned char kFill = 0xa5;
constexpr unsigned char kGuard = 0x5a;
constexpr char kTextPlain[] = "text/plain";

struct OwnedResponse {
  struct noroi_res_t res = {};

  OwnedResponse() = default;
  OwnedResponse(const OwnedResponse&) = delete;
  OwnedResponse& operator=(const OwnedResponse&) = delete;

  ~OwnedResponse() { std::free(res.buf.base); }

  size_t Allocate(size_t len) { return noroi_res_bufalloc(&res, len); }
};

void ExpectBytes(const struct noroi_res_t& res, std::string_view expected) {
  ASSERT_NE(res.buf.base, nullptr);
  ASSERT_EQ(res.buf.len, expected.size());
  EXPECT_EQ(std::memcmp(res.buf.base, expected.data(), expected.size()), 0);
}

template <typename Builder>
void ExpectExactResponse(std::string_view expected, Builder builder) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(expected.size() + 1), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);

  response.res.buf.len = expected.size();
  response.res.buf.base[expected.size()] = static_cast<char>(kGuard);

  testing::internal::CaptureStderr();
  builder(&response.res);
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_TRUE(error.empty()) << error;
  ExpectBytes(response.res, expected);
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[expected.size()]),
            kGuard);
}

}  // namespace

TEST(NoroiResSetOkCstr, BuildsExactMessage) {
  static constexpr char body[] = "Hello, World!\n";
  static constexpr char expected[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 14\r\n"
      "Connection: close\r\n"
      "\r\n"
      "Hello, World!\n";

  ExpectExactResponse(std::string_view(expected, sizeof(expected) - 1),
                      [](struct noroi_res_t* res) {
                        noroi_res_set_ok_cstr(res, kTextPlain,
                                              sizeof(kTextPlain) - 1, body,
                                              sizeof(body) - 1);
                      });
}

TEST(NoroiResSetOkCstr, EmptyBody) {
  static constexpr char expected[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 0\r\n"
      "Connection: close\r\n"
      "\r\n";

  ExpectExactResponse(std::string_view(expected, sizeof(expected) - 1),
                      [](struct noroi_res_t* res) {
                        noroi_res_set_ok_cstr(res, kTextPlain,
                                              sizeof(kTextPlain) - 1, "ignored",
                                              0);
                      });
}

TEST(NoroiResSetOkCstr, CopiesExactByteSliceIncludingNull) {
  static constexpr char body[] = {'A', '\0', 'B', 'x'};
  static constexpr char expected[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 3\r\n"
      "Connection: close\r\n"
      "\r\n"
      "A\0B";

  ExpectExactResponse(std::string_view(expected, sizeof(expected) - 1),
                      [](struct noroi_res_t* res) {
                        noroi_res_set_ok_cstr(res, kTextPlain,
                                              sizeof(kTextPlain) - 1, body, 3);
                      });
}

TEST(NoroiResSetOkCstr, SerializesThreeDigitContentLength) {
  char body[100];
  std::memset(body, 'a', sizeof(body));
  static constexpr char expected_prefix[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 100\r\n"
      "Connection: close\r\n"
      "\r\n";
  const size_t expected_size = sizeof(expected_prefix) - 1 + sizeof(body);

  OwnedResponse response;
  ASSERT_EQ(response.Allocate(expected_size), 0u);
  noroi_res_set_ok_cstr(&response.res, kTextPlain, sizeof(kTextPlain) - 1, body,
                        sizeof(body));

  ASSERT_EQ(response.res.buf.len, expected_size);
  EXPECT_EQ(std::memcmp(response.res.buf.base, expected_prefix,
                        sizeof(expected_prefix) - 1),
            0);
  EXPECT_EQ(std::memcmp(response.res.buf.base + sizeof(expected_prefix) - 1,
                        body, sizeof(body)),
            0);
}

TEST(NoroiResSetOkCstrStatic, DerivesHealthcheckArrayLengths) {
  static constexpr char content_type[] = "text/plain";
  static constexpr char body[] = "\xf0\x9f\x8d\xbb\n";
  static constexpr char expected[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 5\r\n"
      "Connection: close\r\n"
      "\r\n"
      "\xf0\x9f\x8d\xbb\n";

  ExpectExactResponse(std::string_view(expected, sizeof(expected) - 1),
                      [](struct noroi_res_t* res) {
                        noroi_res_set_ok_cstr_static(res, content_type, body);
                      });
}

TEST(NoroiResSetCstr, CopiesExactContentTypeAndBodySlices) {
  static constexpr char content_type[] = {'t', 'e', 'x', 't', '/', 'x', 'x'};
  static constexpr char body[] = {'A', '\0', 'B', 'x'};
  static constexpr char expected[] =
      "HTTP/1.0 201 Created\r\n"
      "Content-Type: text/x\r\n"
      "Content-Length: 3\r\n"
      "Connection: close\r\n"
      "\r\n"
      "A\0B";

  ExpectExactResponse(std::string_view(expected, sizeof(expected) - 1),
                      [](struct noroi_res_t* res) {
                        noroi_res_set_cstr(res, 201, content_type, 6, body, 3);
                      });
}

TEST(NoroiResSetCstrStatic, BuildsRunMiscResponse) {
  static constexpr char content_type[] = "text/plain";
  static constexpr char body[] = "Misc";
  static constexpr char expected[] =
      "HTTP/1.0 418 I'm a teapot\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 4\r\n"
      "Connection: close\r\n"
      "\r\n"
      "Misc";

  ExpectExactResponse(std::string_view(expected, sizeof(expected) - 1),
                      [](struct noroi_res_t* res) {
                        noroi_res_set_cstr_static(res, 418, content_type, body);
                      });
}

struct StatusReasonCase {
  size_t status;
  const char* reason;
};

class NoroiResSetCstrStatusTest
    : public testing::TestWithParam<StatusReasonCase> {};

TEST_P(NoroiResSetCstrStatusTest, UsesBuiltInReasonPhrase) {
  const StatusReasonCase test_case = GetParam();
  std::string expected = "HTTP/1.0 " + std::to_string(test_case.status) + " " +
                         test_case.reason +
                         "\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: 0\r\n"
                         "Connection: close\r\n"
                         "\r\n";

  ExpectExactResponse(expected, [test_case](struct noroi_res_t* res) {
    noroi_res_set_cstr(res, test_case.status, kTextPlain,
                       sizeof(kTextPlain) - 1, "ignored", 0);
  });
}

INSTANTIATE_TEST_SUITE_P(
    SupportedRanges,
    NoroiResSetCstrStatusTest,
    testing::Values(StatusReasonCase{100, "Continue"},
                    StatusReasonCase{103, "Early Hints"},
                    StatusReasonCase{200, "OK"},
                    StatusReasonCase{226, "IM Used"},
                    StatusReasonCase{300, "Multiple Choices"},
                    StatusReasonCase{308, "Permanent Redirect"},
                    StatusReasonCase{400, "Bad Request"},
                    StatusReasonCase{419, "Fire In The Hole"},
                    StatusReasonCase{451, "Unavailable For Legal Reasons"},
                    StatusReasonCase{500, "Internal Server Error"},
                    StatusReasonCase{511, "Network Authentication Required"}),
    [](const testing::TestParamInfo<StatusReasonCase>& info) {
      return "Status" + std::to_string(info.param.status);
    });

TEST(NoroiResNotFound, BuildsExactMessage) {
  static constexpr char expected[] =
      "HTTP/1.0 404 Not Found\r\n"
      "Content-Length: 10\r\n"
      "Connection: close\r\n"
      "\r\n"
      "Not Found\n";

  ExpectExactResponse(
      std::string_view(expected, sizeof(expected) - 1),
      [](struct noroi_res_t* res) { noroi_res_not_found(res); });
}

TEST(NoroiResRedirect, BuildsExactMessage) {
  static constexpr char url[] = "/welcome";
  static constexpr char expected[] =
      "HTTP/1.0 302 Found\r\n"
      "Location: /welcome\r\n"
      "Connection: close\r\n"
      "\r\n";

  ExpectExactResponse(std::string_view(expected, sizeof(expected) - 1),
                      [](struct noroi_res_t* res) {
                        noroi_res_redirect(res, url, sizeof(url) - 1);
                      });
}

TEST(NoroiResRedirect, UsesUrlLengthInsteadOfNullTermination) {
  static constexpr char url[] = {'/', 'o', 'k', 'x'};
  static constexpr char expected[] =
      "HTTP/1.0 302 Found\r\n"
      "Location: /ok\r\n"
      "Connection: close\r\n"
      "\r\n";

  ExpectExactResponse(
      std::string_view(expected, sizeof(expected) - 1),
      [](struct noroi_res_t* res) { noroi_res_redirect(res, url, 3); });
}

TEST(NoroiResRedirect, AllowsEmptyLocation) {
  static constexpr char expected[] =
      "HTTP/1.0 302 Found\r\n"
      "Location: \r\n"
      "Connection: close\r\n"
      "\r\n";

  ExpectExactResponse(
      std::string_view(expected, sizeof(expected) - 1),
      [](struct noroi_res_t* res) { noroi_res_redirect(res, "ignored", 0); });
}

TEST(NoroiResMethodNotAllowed, BuildsExactMessage) {
  static constexpr char expected[] =
      "HTTP/1.0 405 Method Not Allowed\r\n"
      "Content-Length: 19\r\n"
      "Connection: close\r\n"
      "\r\n"
      "Method Not Allowed\n";

  ExpectExactResponse(
      std::string_view(expected, sizeof(expected) - 1),
      [](struct noroi_res_t* res) { noroi_res_method_not_allowed(res); });
}

TEST(NoroiResponseBuilder, TruncatesWithoutCrossingCapacity) {
  static constexpr char body[] = "Hello, World!\n";
  static constexpr char expected[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 14\r\n"
      "Connection: close\r\n"
      "\r\n"
      "Hello, World!\n";
  constexpr size_t expected_size = sizeof(expected) - 1;
  constexpr size_t capacity = expected_size - 1;

  OwnedResponse response;
  ASSERT_EQ(response.Allocate(capacity + 1), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);
  response.res.buf.len = capacity;
  response.res.buf.base[capacity] = static_cast<char>(kGuard);

  testing::internal::CaptureStderr();
  noroi_res_set_ok_cstr(&response.res, kTextPlain, sizeof(kTextPlain) - 1, body,
                        sizeof(body) - 1);
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_EQ(error, "Buffer overflow in _noroi_append\n");
  ASSERT_EQ(response.res.buf.len, capacity);
  EXPECT_EQ(std::memcmp(response.res.buf.base, expected, capacity), 0);
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[capacity]),
            kGuard);
}

TEST(NoroiResBufalloc, AllocatesRequestedCapacity) {
  OwnedResponse response;

  ASSERT_EQ(response.Allocate(32), 0u);
  EXPECT_NE(response.res.buf.base, nullptr);
  EXPECT_EQ(response.res.buf.len, 32u);

  std::memset(response.res.buf.base, kFill, response.res.buf.len);
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[0]), kFill);
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[31]), kFill);
}

TEST(NoroiResBufalloc, FailureLeavesRecordedLengthUnchanged) {
  OwnedResponse response;
  response.res.buf.len = 123;

  EXPECT_EQ(response.Allocate(std::numeric_limits<size_t>::max()), 1u);
  EXPECT_EQ(response.res.buf.base, nullptr);
  EXPECT_EQ(response.res.buf.len, 123u);
}

TEST(NoroiResSnfmt, FormatsArgumentsWithExactRemainingCapacity) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(7), 0u);

  testing::internal::CaptureStderr();
  const size_t written = noroi_res_snfmt(&response.res, 0, "%s:%d", "item", 7);
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_TRUE(error.empty()) << error;
  EXPECT_EQ(written, 6u);
  EXPECT_EQ(std::memcmp(response.res.buf.base, "item:7\0", 7), 0);
}

TEST(NoroiResSnfmt, WritesAtNonzeroOffsetAndPreservesPrefix) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(9), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);
  std::memcpy(response.res.buf.base, "HEAD", 4);

  const size_t written = noroi_res_snfmt(&response.res, 4, "%02d", 7);

  EXPECT_EQ(written, 2u);
  EXPECT_EQ(std::memcmp(response.res.buf.base, "HEAD07\0", 7), 0);
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[7]), kFill);
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[8]), kFill);
}

TEST(NoroiResSnfmt, EmptyOutputIsSuccessfulAndWritesTerminator) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(3), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);

  testing::internal::CaptureStderr();
  const size_t written = noroi_res_snfmt(&response.res, 1, "");
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_TRUE(error.empty()) << error;
  EXPECT_EQ(written, 0u);
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[0]), kFill);
  EXPECT_EQ(response.res.buf.base[1], '\0');
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[2]), kFill);
}

TEST(NoroiResSnfmt, TruncationReturnsZeroAndPreservesGuard) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(5), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);
  response.res.buf.len = 4;
  response.res.buf.base[4] = static_cast<char>(kGuard);

  testing::internal::CaptureStderr();
  const size_t written = noroi_res_snfmt(&response.res, 0, "%s", "abcd");
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_EQ(written, 0u);
  EXPECT_EQ(error, "Buffer overflow in noroi_res_snfmt\n");
  EXPECT_EQ(std::memcmp(response.res.buf.base, "abc\0", 4), 0);
  EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[4]), kGuard);
}

TEST(NoroiResSnfmt, NoCapacityAtEndReturnsZeroWithoutWriting) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(8), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);

  testing::internal::CaptureStderr();
  const size_t written =
      noroi_res_snfmt(&response.res, response.res.buf.len, "%s", "x");
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_EQ(written, 0u);
  EXPECT_EQ(error, "Buffer overflow in noroi_res_snfmt\n");
  for (size_t i = 0; i < response.res.buf.len; ++i)
    EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[i]), kFill);
}

TEST(NoroiResMcpy, CopiesExactlyThroughEndOfBuffer) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(4), 0u);

  testing::internal::CaptureStderr();
  noroi_res_mcpy(&response.res, 0, "ABCD", 4);
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_TRUE(error.empty()) << error;
  EXPECT_EQ(std::memcmp(response.res.buf.base, "ABCD", 4), 0);
}

TEST(NoroiResMcpy, CopiesEmbeddedNullAtNonzeroOffset) {
  static constexpr char source[] = {'A', '\0', 'B'};
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(8), 0u);
  std::memset(response.res.buf.base, '.', response.res.buf.len);

  noroi_res_mcpy(&response.res, 2, source, sizeof(source));

  static constexpr char expected[] = {'.', '.', 'A', '\0', 'B', '.', '.', '.'};
  EXPECT_EQ(std::memcmp(response.res.buf.base, expected, sizeof(expected)), 0);
}

TEST(NoroiResMcpy, ZeroLengthAtEndDoesNotWrite) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(4), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);

  testing::internal::CaptureStderr();
  noroi_res_mcpy(&response.res, 4, "ignored", 0);
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_TRUE(error.empty()) << error;
  for (size_t i = 0; i < response.res.buf.len; ++i)
    EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[i]), kFill);
}

TEST(NoroiResMcpy, OutOfRangeCopyIsRejectedWithoutWriting) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(8), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);

  testing::internal::CaptureStderr();
  noroi_res_mcpy(&response.res, 7, "xy", 2);
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_EQ(error, "Buffer overflow in noroi_res_mcpy\n");
  for (size_t i = 0; i < response.res.buf.len; ++i)
    EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[i]), kFill);
}

TEST(NoroiResMcpy, HugeNonwrappingLengthIsRejectedWithoutWriting) {
  OwnedResponse response;
  ASSERT_EQ(response.Allocate(8), 0u);
  std::memset(response.res.buf.base, kFill, response.res.buf.len);
  static constexpr char source = 'x';

  testing::internal::CaptureStderr();
  noroi_res_mcpy(&response.res, 1, &source,
                 std::numeric_limits<size_t>::max() - 1);
  const std::string error = testing::internal::GetCapturedStderr();

  EXPECT_EQ(error, "Buffer overflow in noroi_res_mcpy\n");
  for (size_t i = 0; i < response.res.buf.len; ++i)
    EXPECT_EQ(static_cast<unsigned char>(response.res.buf.base[i]), kFill);
}

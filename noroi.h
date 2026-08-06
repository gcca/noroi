#pragma once

/**
 * @file noroi.h
 * @brief Public interface for the Noroi HTTP server.
 *
 * Applications provide one `noroi_handle` callback, build a response in that
 * callback, and start the blocking server loop with `noroi_run`.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef noroi_conf_ip
/**
 * @brief IPv4 address on which the server listens.
 *
 * The default is all IPv4 interfaces. Override the macro when compiling
 * `noroi.c`, for example with `-Dnoroi_conf_ip='"127.0.0.1"'`.
 */
#define noroi_conf_ip "0.0.0.0"
#endif

#ifndef noroi_conf_port
/**
 * @brief TCP port on which the server listens.
 *
 * The default is port 8000. Override the macro when compiling `noroi.c`, for
 * example with `-Dnoroi_conf_port=8080`.
 */
#define noroi_conf_port 8000
#endif

/**
 * @brief Opaque view of bytes received from a client.
 *
 * Noroi creates this object and passes it to `noroi_handle`. Applications use
 * `noroi_req_parse_url_mp` or `noroi_req_parse_url_mpq` to inspect it and must
 * not retain its pointer after the callback returns.
 */
struct noroi_req_t;

/**
 * @brief Opaque HTTP response under construction.
 *
 * Noroi supplies this object to `noroi_handle`. Applications must not
 * instantiate it, access its fields, or retain its pointer. Allocate its
 * storage with `noroi_res_bufalloc` and populate it with a response builder or
 * the bounded write helpers.
 */
struct noroi_res_t;

/**
 * @brief Handles bytes received from a client and constructs an HTTP response.
 *
 * The application must define this callback exactly once. It runs
 * synchronously while the received bytes are valid. Before returning normally,
 * it must allocate and populate `res`. This function has no return value; its
 * result is the response written through `res`.
 *
 * @param[in] req Request bytes owned by the server.
 * @param[out] res Response to allocate and populate.
 *
 * Example:
 * @code{.c}
 * #include <stdlib.h>
 * #include <string.h>
 *
 * void noroi_handle(const struct noroi_req_t* req,
 *                   struct noroi_res_t* res) {
 *   const char *method, *path;
 *   size_t method_size, path_size;
 *   noroi_req_parse_url_mp(req, &method, &method_size, &path, &path_size);
 *
 *   if (noroi_res_bufalloc(res, 512) != 0)
 *     abort();
 *
 *   if (path_size == 12 && memcmp(path, "/healthcheck", 12) == 0) {
 *     static const char content_type[] = "text/plain";
 *     static const char body[] = "Hello, World!\n";
 *     noroi_res_set_ok_cstr(res, content_type, sizeof(content_type) - 1,
 *                           body, sizeof(body) - 1);
 *   } else {
 *     noroi_res_not_found(res);
 *   }
 * }
 * @endcode
 */
extern void noroi_handle(const struct noroi_req_t* req,
                         struct noroi_res_t* res);

/**
 * @brief Extracts the first two space-delimited request fields.
 *
 * The returned method and request-target slices borrow the request storage and
 * are not null-terminated. The request target includes a query beginning with
 * `?`, if present; this function does not split it from the path.
 *
 * This is a minimal delimiter scanner, not a validating HTTP parser. `req`, its
 * buffer, and all four output pointers must be valid, and both fields must be
 * followed by a space within the scanner's threshold. Violating those
 * preconditions has undefined behavior. The threshold is computed from the low
 * 11 bits of the received-byte count (`nread & 0x7ff`), rather than by capping
 * the count at 2048 bytes. The function has no failure result.
 *
 * @param[in] req Request bytes whose leading fields are scanned.
 * @param[out] method Start of the method slice.
 * @param[out] method_size Length of the method slice in bytes.
 * @param[out] path Start of the complete request-target slice.
 * @param[out] path_size Length of the request-target slice in bytes.
 */
void noroi_req_parse_url_mp(const struct noroi_req_t* req,
                            const char* method[],
                            size_t* method_size,
                            const char* path[],
                            size_t* path_size);

/**
 * @brief Extracts three sequential fields with the low-level request scanner.
 *
 * All outputs are borrowed, non-null-terminated slices into the request
 * storage. The first slice ends at the first space, and the second ends at the
 * next space. The third begins after that second space and ends at the next
 * space or `?`. Consequently, the second slice contains the complete request
 * target, including any URI query, while on a conventional HTTP request line
 * the third slice begins with the HTTP-version token; it does not represent the
 * URI query despite the parameter name.
 *
 * This function performs no validation or output initialization. `req`, its
 * buffer, and every output pointer must be valid, and each scanned field must
 * be followed by a supported delimiter within the scanner's threshold;
 * otherwise behavior is undefined. The threshold is computed from the low 11
 * bits of the received-byte count (`nread & 0x7ff`), rather than by capping the
 * count at 2048 bytes. The function has no failure result.
 *
 * @param[in] req Request bytes whose leading fields are scanned.
 * @param[out] method Start of the method slice.
 * @param[out] method_size Length of the method slice in bytes.
 * @param[out] path Start of the complete request-target slice.
 * @param[out] path_size Length of the request-target slice in bytes.
 * @param[out] query Start of the field following the request target.
 * @param[out] query_size Length of that following field in bytes.
 */
void noroi_req_parse_url_mpqhb(const struct noroi_req_t* req,
                               const char* method[],
                               size_t* method_size,
                               const char* path[],
                               size_t* path_size,
                               const char* query[],
                               size_t* query_size,
                               const char* headers_begin[],
                               const char* headers_end[],
                               const char* body_start[],
                               const char* body_end[]);

/**
 * @brief Builds a `200 OK` response with an exact content type and body.
 *
 * Exactly `content_type_len` bytes are written as the `Content-Type` value, and
 * exactly `content_len` bytes are copied into the body. Neither byte sequence
 * need be null-terminated. The response uses HTTP/1.0 and includes
 * `Content-Length` and `Connection: close` headers. The content type is copied
 * without validation or escaping; it must be a valid single HTTP field value
 * without carriage-return or line-feed bytes.
 *
 * Before calling, allocate `res` with room for the HTTP headers and body. The
 * function changes the response length from buffer capacity to the number of
 * serialized bytes. An undersized buffer produces a truncated, potentially
 * malformed response and reports truncation to standard error. This function
 * has no return value.
 *
 * @param[in,out] res Previously allocated response to populate.
 * @param[in] content_type Content-Type field-value bytes to copy.
 * @param[in] content_type_len Number of content-type bytes to copy.
 * @param[in] content Body bytes to copy.
 * @param[in] content_len Number of body bytes to copy.
 *
 * Example:
 * @code{.c}
 * static const char content_type[] = "text/plain";
 * static const char body[] = "OK\n";
 * if (noroi_res_bufalloc(res, 512) != 0)
 *   abort();
 * noroi_res_set_ok_cstr(res, content_type, sizeof(content_type) - 1, body,
 *                       sizeof(body) - 1);
 * @endcode
 */
void noroi_res_set_ok_cstr(struct noroi_res_t* res,
                           const char* const content_type,
                           const size_t content_type_len,
                           const char* const content,
                           const size_t content_len);

/**
 * @brief Builds a `200 OK` response from array-sized strings.
 *
 * This convenience macro calls `noroi_res_set_ok_cstr`, deriving the
 * content-type and body lengths with `sizeof(argument) - 1`. `content_type`
 * and `content` must therefore be string literals or character arrays whose
 * final element is the terminating null byte; passing a pointer produces an
 * incorrect length. Allocate `res` before calling, as required by
 * `noroi_res_set_ok_cstr`.
 *
 * @param[in,out] res Previously allocated response to populate.
 * @param[in] content_type Content-Type string literal or character array.
 * @param[in] content Body string literal or character array.
 *
 * Example:
 * @code{.c}
 * static const char content_type[] = "text/plain";
 * static const char body[] = "OK\n";
 * if (noroi_res_bufalloc(res, 512) != 0)
 *   abort();
 * noroi_res_set_ok_cstr_static(res, content_type, body);
 * @endcode
 */
#define noroi_res_set_ok_cstr_static(res, content_type, content)           \
  noroi_res_set_ok_cstr((res), (content_type), (sizeof(content_type) - 1), \
                        (content), (sizeof(content) - 1))

/**
 * @brief Builds a response with an explicit status, content type, and body.
 *
 * The response uses HTTP/1.0 and the built-in reason phrase for `status_code`.
 * It includes `Content-Type`, `Content-Length`, and `Connection: close`
 * headers, followed by exactly `content_len` body bytes. The content type and
 * body need not be null-terminated. The content type is copied without
 * validation or escaping and must be a valid single HTTP field value without
 * carriage-return or line-feed bytes.
 *
 * The reason-phrase table covers status ranges 100 through 103, 200 through
 * 226, 300 through 308, 400 through 451, and 500 through 511. Unassigned codes
 * inside those ranges use the table's `Fire In The Hole` fallback; values
 * outside those ranges are unsupported.
 *
 * Before calling, allocate `res` with room for the complete serialized
 * response. This is a strict precondition because the 13-byte
 * `HTTP/1.0 nnn ` prefix is written without a bounds check. The function
 * changes the response length from buffer capacity to the number of serialized
 * bytes and has no return value.
 *
 * @param[in,out] res Previously allocated response to populate.
 * @param[in] status_code Supported three-digit HTTP status code.
 * @param[in] content_type Content-Type field-value bytes to copy.
 * @param[in] content_type_len Number of content-type bytes to copy.
 * @param[in] content Body bytes to copy.
 * @param[in] content_len Number of body bytes to copy.
 *
 * Example:
 * @code{.c}
 * static const char content_type[] = "text/plain";
 * static const char body[] = "Misc";
 * if (noroi_res_bufalloc(res, 128) != 0)
 *   abort();
 * noroi_res_set_cstr(res, 418, content_type, sizeof(content_type) - 1, body,
 *                    sizeof(body) - 1);
 * @endcode
 */
void noroi_res_set_cstr(struct noroi_res_t* res,
                        const size_t status_code,
                        const char* content_type,
                        const size_t content_type_len,
                        const char* content,
                        const size_t content_len);

/**
 * @brief Builds an explicit-status response from array-sized strings.
 *
 * This convenience macro calls `noroi_res_set_cstr`, deriving the content-type
 * and body lengths with `sizeof(argument) - 1`. `content_type` and `content`
 * must be string literals or character arrays whose final element is the
 * terminating null byte; passing a pointer produces an incorrect length.
 * `status_code` must be an integer constant expression. The macro rejects
 * constants outside 100 through 511 at compile time, but the code must also
 * satisfy the supported-status precondition of `noroi_res_set_cstr`. Allocate
 * `res` before calling, as required by that function.
 *
 * @param[in,out] res Previously allocated response to populate.
 * @param[in] status_code Supported constant HTTP status code.
 * @param[in] content_type Content-Type string literal or character array.
 * @param[in] content Body string literal or character array.
 *
 * Example:
 * @code{.c}
 * static const char content_type[] = "text/plain";
 * static const char body[] = "Misc";
 * if (noroi_res_bufalloc(res, 128) != 0)
 *   abort();
 * noroi_res_set_cstr_static(res, 418, content_type, body);
 * @endcode
 */
#define noroi_res_set_cstr_static(res, status_code, content_type, content) \
  do {                                                                     \
    _Static_assert((status_code) >= 100 && (status_code) < 512,            \
                   "Invalid HTTP status code");                            \
    noroi_res_set_cstr((res), (status_code), (content_type),               \
                       (sizeof(content_type) - 1), (content),              \
                       (sizeof(content) - 1));                             \
  } while (0)

/**
 * @brief Builds a `404 Not Found` response with a fixed textual body.
 *
 * The response body is `Not Found\n`; no `Content-Type` header is added.
 * Allocate `res` before calling; the function records the number of serialized
 * bytes as the response length. An undersized buffer produces a truncated,
 * potentially malformed response and reports truncation to standard error.
 * This function has no return value.
 *
 * @param[in,out] res Previously allocated response to populate.
 *
 * Example:
 * @code{.c}
 * if (noroi_res_bufalloc(res, 512) != 0)
 *   abort();
 * noroi_res_not_found(res);
 * @endcode
 */
void noroi_res_not_found(struct noroi_res_t* res);

/**
 * @brief Builds a `302 Found` response that redirects to `url`.
 *
 * Exactly `url_len` bytes are written as the `Location` value, so `url` need
 * not be null-terminated. The value is copied without validation or escaping;
 * it must be a valid single HTTP field value without carriage-return or
 * line-feed bytes. The response has an empty body and does not include a
 * `Content-Length` header. Allocate `res` before calling; the function records
 * the number of serialized bytes as the response length. An undersized buffer
 * produces a truncated, potentially malformed response and reports truncation
 * to standard error. This function has no return value.
 *
 * @param[in,out] res Previously allocated response to populate.
 * @param[in] url Relative or absolute redirect target.
 * @param[in] url_len Length of the redirect target in bytes.
 *
 * Example:
 * @code{.c}
 * static const char url[] = "/welcome";
 * if (noroi_res_bufalloc(res, 512) != 0)
 *   abort();
 * noroi_res_redirect(res, url, sizeof(url) - 1);
 * @endcode
 */
void noroi_res_redirect(struct noroi_res_t* res,
                        const char* const url,
                        const size_t url_len);

/**
 * @brief Builds a `405 Method Not Allowed` response with a fixed textual body.
 *
 * The response body is `Method Not Allowed\n`; neither a `Content-Type` nor an
 * `Allow` header is added. Allocate `res` before calling; the function records
 * the number of serialized bytes as the response length. An undersized buffer
 * produces a truncated, potentially malformed response and reports truncation
 * to standard error. This function has no return value.
 *
 * @param[in,out] res Previously allocated response to populate.
 *
 * Example:
 * @code{.c}
 * if (noroi_res_bufalloc(res, 256) != 0)
 *   abort();
 * noroi_res_method_not_allowed(res);
 * @endcode
 */
void noroi_res_method_not_allowed(struct noroi_res_t* res);

/**
 * @brief Allocates storage for an HTTP response.
 *
 * Call this once for a response, before any other `noroi_res_*` function; they
 * all assume `res` already owns storage. This function does not resize or free
 * an existing allocation, so calling it again for the same response leaks that
 * storage. `len` must include capacity for both HTTP headers and the response
 * body. Inside `noroi_handle`, the server takes ownership of a successful
 * allocation when the callback returns and releases it after writing the
 * response. The application must not retain or release that storage itself.
 *
 * @param[out] res Response whose buffer is initialized on success.
 * @param[in] len Requested capacity in bytes.
 * @return `0` on success, after which `res` owns `len` writable bytes; `1` if
 * the allocation fails. On failure the buffer pointer is null and its recorded
 * length is left unchanged, so a failed response must not be passed to another
 * `noroi_res_*` function nor handed back to the server.
 *
 * Example:
 * @code{.c}
 * // Reserve room for the status line, headers, and body together.
 * if (noroi_res_bufalloc(res, 512) != 0)
 *   abort();
 * noroi_res_not_found(res);
 * @endcode
 */
[[nodiscard]] size_t noroi_res_bufalloc(struct noroi_res_t* res, size_t len);

/**
 * @brief Starts the HTTP server on the default libuv event loop.
 *
 * The server initializes a TCP handle, attempts to bind to
 * `noroi_conf_ip:noroi_conf_port`, listens for connections, and invokes
 * `noroi_handle` for received data. The call blocks while the event loop has
 * work. Return values from TCP initialization, address conversion, and binding
 * are not checked; only a listening failure is handled explicitly.
 *
 * @return `EXIT_FAILURE` if listening fails; otherwise the event loop's status
 * when it exits.
 *
 * Example:
 * @code{.c}
 * int main(void) {
 *   return noroi_run();
 * }
 * @endcode
 */
int noroi_run(void);

/**
 * @brief Writes formatted text into an allocated response buffer.
 *
 * Call `noroi_res_bufalloc` first. Formatting begins at the byte `offset` and
 * uses the capacity remaining after that offset, including space for the
 * terminating null byte written by `vsnprintf`; bytes before `offset` are left
 * untouched, so headers and body can be composed in successive calls. `offset`
 * is not validated against the allocated capacity, and a value beyond it writes
 * out of bounds; pass only an offset the caller has already accounted for.
 * Truncation is detected, reported to standard error, and may leave a
 * null-terminated prefix in the destination.
 *
 * A successful call needs one byte after the formatted text for that null
 * terminator. In an exact-length wire response, that byte must belong to a
 * later segment and be overwritten before `noroi_handle` returns, as the
 * example does with `noroi_res_mcpy`.
 * This helper does not set the final wire length; when composing a raw response
 * with the bounded write helpers, allocate exactly the final number of bytes
 * that the server should send.
 *
 * @param[in,out] res Response containing previously allocated storage.
 * @param[in] offset Byte offset at which formatting begins.
 * @param[in] fmt `printf`-style format string.
 * @param[in] ... Values consumed by `fmt`.
 * @return The number of formatted bytes, excluding the terminating null byte,
 * on nonempty success; `0` if the output is empty, formatting fails, or the
 * formatted text does not fit.
 *
 * Example:
 * @code{.c}
 * #include <stdio.h>
 * #include <stdlib.h>
 *
 * static const char body[] = "Hello, World!\n";
 * static const char headers[] =
 *     "HTTP/1.0 200 OK\r\nContent-Length: %zu\r\n\r\n";
 * const size_t body_size = sizeof(body) - 1;
 * const int header_size = snprintf(nullptr, 0, headers, body_size);
 * if (header_size <= 0)
 *   abort();
 * if (noroi_res_bufalloc(res, (size_t)header_size + body_size) != 0)
 *   abort();
 * const size_t written = noroi_res_snfmt(res, 0, headers, body_size);
 * if (written != (size_t)header_size)
 *   abort();
 * noroi_res_mcpy(res, written, body, body_size);
 * @endcode
 */
size_t noroi_res_snfmt(struct noroi_res_t* res,
                       size_t offset,
                       const char* const fmt,
                       ...);

/**
 * @brief Copies bytes into an allocated response buffer.
 *
 * Call `noroi_res_bufalloc` first. Exactly `len` bytes are copied beginning at
 * byte `offset`; `s` need not be null-terminated, so this is the way to append
 * a body that may contain embedded null bytes. A copy whose `offset + len`
 * exceeds the allocated capacity is rejected without writing and reports the
 * overflow to standard error; the check assumes that sum does not wrap, so it
 * does not catch an `offset` or `len` near `SIZE_MAX`. The source and
 * destination ranges must not overlap. A successful copy does not set the
 * final wire length; when composing a raw response, allocate exactly the
 * number of bytes that the server should send. This function has no return
 * value.
 *
 * @param[in,out] res Response containing previously allocated storage.
 * @param[in] offset Byte offset at which copying begins.
 * @param[in] s Source bytes.
 * @param[in] len Number of bytes to copy.
 *
 * Example:
 * @code{.c}
 * #include <stdlib.h>
 *
 * static const char response[] =
 *     "HTTP/1.0 204 No Content\r\nContent-Length: 0\r\n\r\n";
 * if (noroi_res_bufalloc(res, sizeof(response) - 1) != 0)
 *   abort();
 * noroi_res_mcpy(res, 0, response, sizeof(response) - 1);
 * @endcode
 */
void noroi_res_mcpy(struct noroi_res_t* res,
                    size_t offset,
                    const char* const s,
                    size_t len);

#ifdef __cplusplus
}
#endif

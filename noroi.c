/**
 * @file noroi.c
 * @brief Noroi request scanning, response serialization, and libuv server
 * implementation.
 *
 * Public API contracts and examples are documented once in `noroi.h`. The
 * documentation in this file covers implementation-private helpers and state.
 */

#include "noroi.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#ifdef NOROI_MIMALLOC
#include <mimalloc.h>
#define malloc(size) mi_malloc(size)
#define free(ptr) mi_free(ptr)
#endif

// Private layout backing the opaque request type declared in noroi.h.
struct noroi_req_t {
  // libuv buffer containing the received bytes.
  const uv_buf_t* buf;
  // Number of readable bytes in buf->base.
  size_t nread;
};

// Private layout backing the opaque response type declared in noroi.h.
struct noroi_res_t {
  // Owned response storage and its capacity or final wire length.
  uv_buf_t buf;
};

/**
 * @brief Captures a nonempty slice ending at the next space.
 *
 * Scanning begins at the byte after `start`. A space must occur no later than
 * `thresh`; the loop dereferences before checking the threshold, so violating
 * this precondition reads beyond it. All pointer arguments must be valid.
 *
 * @param[in] start First byte of the slice.
 * @param[in] thresh Last position at which the delimiter may occur.
 * @param[out] value Start of the captured slice.
 * @param[out] value_size Length of the captured slice in bytes.
 * @return The byte immediately after the delimiting space.
 */
[[nodiscard]] inline static __attribute__((always_inline)) const char*
_noroi_req_parse_url_adv_m(const char* start,
                           const char* const thresh,
                           const char* value[],
                           size_t* value_size) {
  *value = start;
  while (*++start != ' ' && start <= thresh)
    ;
  *value_size = start - *value;
  return ++start;
}

/**
 * @brief Captures a nonempty slice ending at the next space or question mark.
 *
 * Scanning begins at the byte after `start`. A supported delimiter must occur
 * no later than `thresh`; the loop dereferences before checking the threshold,
 * so violating this precondition reads beyond it. All pointer arguments must
 * be valid.
 *
 * @param[in] start First byte of the slice.
 * @param[in] thresh Last position at which a delimiter may occur.
 * @param[out] value Start of the captured slice.
 * @param[out] value_size Length of the captured slice in bytes.
 * @return The byte immediately after the delimiter.
 */
[[nodiscard]] inline static __attribute__((always_inline)) const char*
_noroi_req_parse_url_adv_p(const char* start,
                           const char* const thresh,
                           const char* value[],
                           size_t* value_size) {
  *value = start;
  while (*++start != ' ' && *start != '?' && start <= thresh)
    ;
  *value_size = start - *value;
  return start;
}

[[nodiscard]] inline static __attribute__((always_inline)) const char*
_noroi_req_parse_url_adv_f(const char* start,
                           const char* const thresh,
                           const char* value[],
                           size_t* value_size) {
  *value = ++start;
  while (*++start != ' ' && *start != '\r' && start <= thresh)
    ;
  *value_size = start - *value;
  return ++start;
}

[[nodiscard]] inline static __attribute__((always_inline)) const char*
_noroi_req_parse_url_adv_h(const char* start,
                           const char* const thresh,
                           const char* begin[],
                           const char* end[]) {
  *begin = ++start;

  const char* dots[128][2][2] = {};
  size_t i = 0;

  do {
    dots[i][0][0] = start;
    while (*++start != ':' && start <= thresh)
      ;
    dots[i][0][1] = start++;
    dots[i][1][0] = ++start;
    while (*++start != '\r' && start <= thresh)
      ;
    dots[i][1][1] = start;
    ++i;
    start += 2;
    if (*start == '\r')
      break;
  } while (i < 128 && start <= thresh);

  *end = start - 2;
  return start + 2;
}

void noroi_req_parse_url_mp(const struct noroi_req_t* req,
                            const char* method[],
                            size_t* method_size,
                            const char* path[],
                            size_t* path_size) {
  const char *base = req->buf->base, *thresh = base + (req->nread & 0x7FFull);
  base = _noroi_req_parse_url_adv_m(base, thresh, method, method_size);
  (void)_noroi_req_parse_url_adv_p(base, thresh, path, path_size);
}

// Scans three sequential fields. The second scan stops only at a space, so it
// returns the complete request target, including any query. The third scan
// then begins at the HTTP-version token on a conventional request line.
// `thresh` uses the low 11 bits of nread; it is not a conventional 2048-byte
// cap.
void noroi_req_parse_url_mpqhb(const struct noroi_req_t* req,
                               const char* method[],
                               size_t* method_size,
                               const char* path[],
                               size_t* path_size,
                               const char* query[],
                               size_t* query_size,
                               const char* headers_begin[],
                               const char* headers_end[],
                               const char* body_begin[],
                               const char* body_end[]) {
  const char *base = req->buf->base, *thresh = base + (req->nread & 0x7FFull);
  base = _noroi_req_parse_url_adv_m(base, thresh, method, method_size);
  base = _noroi_req_parse_url_adv_p(base, thresh, path, path_size);
  if (*base == ' ')
    *query_size = 0;
  else
    base = _noroi_req_parse_url_adv_f(base, thresh, query, query_size);
  while (*++base != '\n' && base <= thresh)
    ;
  base = _noroi_req_parse_url_adv_h(base, thresh, headers_begin, headers_end);
  *body_begin = base;
  *body_end = req->buf->base + req->nread;
}

// HTTP response serialization helpers.

/**
 * @brief Appends a byte sequence to a bounded output buffer.
 *
 * If the sequence does not fit, the prefix that fits is copied and a
 * diagnostic is written to standard error.
 *
 * @param[in,out] base Next output position.
 * @param[in] end One-past-the-end of the output buffer.
 * @param[in] s Source bytes.
 * @param[in] len Number of source bytes.
 * @return The next output position, equal to `end` after truncation.
 */
static inline __attribute__((always_inline)) char*
_noroi_append(char* base, const char* end, const char* s, size_t len) {
  while (base < end && len-- > 0)
    *base++ = *s++;
  return base;
}

/**
 * @brief Appends the successful HTTP status line.
 *
 * @param[in,out] base Next output position.
 * @param[in] end One-past-the-end of the output buffer.
 * @return The next output position.
 */
static inline __attribute__((always_inline)) char* _noroi_append_http_status_ok(
    char* base,
    const char* const end) {
  static const char status[] = "HTTP/1.0 200 OK\r\n";
  return _noroi_append(base, end, status, sizeof(status) - 1);
}

/**
 * @brief Appends one carriage-return/line-feed sequence.
 *
 * @param[in,out] base Next output position.
 * @param[in] end One-past-the-end of the output buffer.
 * @return The next output position.
 */
static inline __attribute__((always_inline)) char* _noroi_append_http_crlf(
    char* base,
    const char* end) {
  static const char crlf[] = "\r\n";
  return _noroi_append(base, end, crlf, sizeof(crlf) - 1);
}

/**
 * @brief Appends an HTTP/1.0 status line using the built-in reason table.
 *
 * The fixed `HTTP/1.0 nnn ` prefix is copied without consulting `end`, so at
 * least 13 writable bytes must remain at `base`. To keep the table lookup in
 * bounds, `code` must have a hundreds digit from 1 through 5 and final two
 * digits no greater than 51. A table slot without a reason phrase serializes
 * an empty phrase.
 *
 * @param[in,out] base Next output position with at least 13 writable bytes.
 * @param[in] end One-past-the-end of the output buffer.
 * @param[in] code Three-digit status code selecting a reason-table entry.
 * @return The next output position after the status-line terminator, or `end`
 * after bounded suffix truncation.
 */
static inline __attribute__((always_inline)) char* _noroi_append_http_status(
    char* base,
    const char* const end,
    const size_t code) {
  typedef struct {
    const char* s;
    size_t len;
  } _reason_t;
  static const _reason_t statuses[][52] = {
      /* 1xx */
      {
          {"Continue", 8},             /* 100 */
          {"Switching Protocols", 19}, /* 101 */
          {"Processing", 10},          /* 102 */
          {"Early Hints", 11}          /* 103 */
      },
      /* 2xx */
      {
          {"OK", 2},                             /* 200 */
          {"Created", 7},                        /* 201 */
          {"Accepted", 8},                       /* 202 */
          {"Non-Authoritative Information", 29}, /* 203 */
          {"No Content", 10},                    /* 204 */
          {"Reset Content", 13},                 /* 205 */
          {"Partial Content", 15},               /* 206 */
          {"Multi-Status", 12},                  /* 207 */
          {"Already Reported", 16},              /* 208 */
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"IM Used", 7} /* 226 */
      },
      /* 3xx */
      {
          {"Multiple Choices", 16},   /* 300 */
          {"Moved Permanently", 17},  /* 301 */
          {"Found", 5},               /* 302 */
          {"See Other", 9},           /* 303 */
          {"Not Modified", 12},       /* 304 */
          {"Use Proxy", 9},           /* 305 */
          {"Fire In The Hole", 16},   /* 306 (unused) */
          {"Temporary Redirect", 18}, /* 307 */
          {"Permanent Redirect", 18}  /* 308 */
      },
      /* 4xx */
      {
          {"Bad Request", 11},                   /* 400 */
          {"Unauthorized", 12},                  /* 401 */
          {"Payment Required", 16},              /* 402 */
          {"Forbidden", 9},                      /* 403 */
          {"Not Found", 9},                      /* 404 */
          {"Method Not Allowed", 18},            /* 405 */
          {"Not Acceptable", 14},                /* 406 */
          {"Proxy Authentication Required", 29}, /* 407 */
          {"Request Timeout", 15},               /* 408 */
          {"Conflict", 8},                       /* 409 */
          {"Gone", 4},                           /* 410 */
          {"Length Required", 15},               /* 411 */
          {"Precondition Failed", 19},           /* 412 */
          {"Content Too Large", 17},             /* 413 */
          {"URI Too Long", 12},                  /* 414 */
          {"Unsupported Media Type", 22},        /* 415 */
          {"Range Not Satisfiable", 21},         /* 416 */
          {"Expectation Failed", 18},            /* 417 */
          {"I'm a teapot", 12},                  /* 418 */
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Misdirected Request", 19},   /* 421 */
          {"Unprocessable Content", 21}, /* 422 */
          {"Locked", 6},                 /* 423 */
          {"Failed Dependency", 17},     /* 424 */
          {"Too Early", 9},              /* 425 */
          {"Upgrade Required", 16},      /* 426 */
          {"Fire In The Hole", 16},
          {"Precondition Required", 21}, /* 428 */
          {"Too Many Requests", 17},     /* 429 */
          {"Fire In The Hole", 16},
          {"Request Header Fields Too Large", 31}, /* 431 */
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Fire In The Hole", 16},
          {"Unavailable For Legal Reasons", 29} /* 451 */
      },
      /* 5xx */
      {
          {"Internal Server Error", 21},      /* 500 */
          {"Not Implemented", 15},            /* 501 */
          {"Bad Gateway", 11},                /* 502 */
          {"Service Unavailable", 19},        /* 503 */
          {"Gateway Timeout", 15},            /* 504 */
          {"HTTP Version Not Supported", 26}, /* 505 */
          {"Variant Also Negotiates", 23},    /* 506 */
          {"Insufficient Storage", 20},       /* 507 */
          {"Loop Detected", 13},              /* 508 */
          {"Fire In The Hole", 16},
          {"Not Extended", 12},                   /* 510 */
          {"Network Authentication Required", 31} /* 511 */
      }};
  static char prefix[] = "HTTP/1.0 ";
  const _reason_t reason = statuses[code / 100 - 1][code % 100];
  char status[4];
  status[0] = '0' + code / 100;
  status[1] = '0' + (code / 10) % 10;
  status[2] = '0' + code % 10;
  status[3] = ' ';
  base = stpncpy(base, prefix, sizeof(prefix) - 1);
  base = stpncpy(base, status, 4);
  base = _noroi_append(base, end, reason.s, reason.len);
  return _noroi_append_http_crlf(base, end);
}

/**
 * @brief Appends a `Content-Type` header with an exact byte-slice value.
 *
 * `val` is copied without validation or escaping and need not be
 * null-terminated.
 *
 * @param[in,out] base Next output position.
 * @param[in] end One-past-the-end of the output buffer.
 * @param[in] val Header-value bytes to append.
 * @param[in] len Number of header-value bytes to append.
 * @return The next output position.
 */
static inline char* _noroi_append_http_content_type(char* base,
                                                    const char* const end,
                                                    const char* const val,
                                                    const size_t len) {
  static const char name[] = "Content-Type: ";
  base = _noroi_append(base, end, name, sizeof(name) - 1);
  base = _noroi_append(base, end, val, len);
  return _noroi_append_http_crlf(base, end);
}

/**
 * @brief Appends the `Connection: close` header.
 *
 * @param[in,out] base Next output position.
 * @param[in] end One-past-the-end of the output buffer.
 * @return The next output position.
 */
static inline char* _noroi_append_http_connection_close(char* base,
                                                        const char* const end) {
  static const char connection_close[] = "Connection: close\r\n";
  return _noroi_append(base, end, connection_close,
                       sizeof(connection_close) - 1);
}

/**
 * @brief Appends a decimal `Content-Length` header.
 *
 * @param[in,out] base Next output position.
 * @param[in] end One-past-the-end of the output buffer.
 * @param[in] len Body length to encode.
 * @return The next output position.
 */
static inline char* _noroi_append_http_content_length(char* base,
                                                      const char* const end,
                                                      size_t len) {
  static const char content_length[] = "Content-Length: ";
  base = _noroi_append(base, end, content_length, sizeof(content_length) - 1);
  char len_str[32];
  int len_str_len = snprintf(len_str, sizeof(len_str), "%zu", len);
  base = _noroi_append(base, end, len_str, (size_t)len_str_len);
  return _noroi_append_http_crlf(base, end);
}

// libuv server internals.

/**
 * @brief State owned by one accepted connection.
 *
 * `tcp` must remain the first field because libuv handle pointers are cast
 * back to `client_t`. After `noroi_handle` returns with the required
 * response, `buf` stores that allocation until the connection is closed.
 */
struct client_t {
  /** TCP handle; first so its address is also the enclosing object address.
   */
  uv_tcp_t tcp;
  /** Request used while asynchronously writing the response. */
  uv_write_t write_req;
  /** Response storage released when the client closes. */
  uv_buf_t buf;
};

/**
 * @brief Releases connection state after libuv finishes closing its handle.
 *
 * @param[in] handle TCP handle at the start of the allocated `client_t`.
 */
static void on_close(uv_handle_t* handle) {
  free(handle);
}

/**
 * @brief Releases response memory and schedules the handle to close if
 * needed.
 *
 * The response buffer pointer must already be initialized or `NULL`; this
 * function does not validate that precondition before passing it to `free`.
 *
 * @param[in,out] handle TCP handle belonging to the client being closed.
 */
static void close_client(uv_handle_t* handle) {
  struct client_t* client = (struct client_t*)handle;
  free(client->buf.base);
  if (!uv_is_closing(handle))
    uv_close(handle, on_close);
}

/**
 * @brief Allocates the inbound read buffer requested by libuv.
 *
 * The libuv handle argument is unused.
 * On allocation failure, `buf->base` is `NULL` and `buf->len` is zero.
 *
 * @param[in] suggested Capacity requested by libuv.
 * @param[out] buf Buffer initialized with allocated storage or an empty
 * value.
 */
static void alloc_cb(uv_handle_t*, size_t suggested, uv_buf_t* buf) {
  buf->base = (char*)malloc(suggested);
  buf->len = buf->base ? suggested : 0;
}

/**
 * @brief Reports a write error, if any, and closes the client connection.
 *
 * @param[in] req Completed libuv write request embedded in `client_t`.
 * @param[in] status Zero on success or a negative libuv error code.
 */
static void on_write(uv_write_t* req, int status) {
  if (status < 0)
    fprintf(stderr, "write: %s\n", uv_strerror(status));
  close_client((uv_handle_t*)req->handle);
}

/**
 * @brief Dispatches request bytes to the application or handles a read end.
 *
 * A positive read is passed synchronously to `noroi_handle` without
 * additional HTTP framing or accumulation. The callback's response buffer is
 * transferred to the client, reads are stopped, and the function attempts to
 * queue the response. The inbound buffer is freed before returning, which
 * bounds the lifetime of request slices. EOF and read errors close the
 * connection.
 *
 * @param[in,out] stream Client stream being read.
 * @param[in] nread Number of bytes read or a negative libuv status code.
 * @param[in] buf Inbound buffer allocated by `alloc_cb`.
 */
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  struct client_t* client = (struct client_t*)stream;

  if (nread > 0) {
    struct noroi_req_t req = {buf, (size_t)nread};
    struct noroi_res_t res;

    noroi_handle(&req, &res);
    client->buf = res.buf;
    uv_read_stop(stream);

    if (uv_write(&client->write_req, stream, &res.buf, 1, on_write))
      close_client((uv_handle_t*)stream);

  } else if (nread < 0) {
    if (nread != UV_EOF)
      fprintf(stderr, "read: %s\n", uv_strerror((int)nread));

    close_client((uv_handle_t*)stream);
  }

  free(buf->base);
}

/** Default libuv event loop used by the server and connection callbacks. */
static uv_loop_t* loop;

/**
 * @brief Accepts a client and starts asynchronous reads for it.
 *
 * A negative callback status is reported without allocating a client.
 * Otherwise, the callback allocates client state and attempts to initialize,
 * accept, and start reading from it. Return values from TCP initialization
 * and read startup are ignored; an accept failure closes the initialized
 * handle.
 *
 * @param[in,out] server_handle Listening TCP stream.
 * @param[in] status Zero when a connection is ready to accept, or a negative
 * libuv error code.
 */
static void on_connection(uv_stream_t* server_handle, int status) {
  if (status < 0) {
    fprintf(stderr, "connection: %s\n", uv_strerror(status));
    return;
  }

  struct client_t* client = (struct client_t*)malloc(sizeof(*client));
  if (!client) {
    fprintf(stderr, "Error alloc for client\n");
    return;
  }

  uv_tcp_init(loop, &client->tcp);
  if (uv_accept(server_handle, (uv_stream_t*)&client->tcp) == 0) {
    uv_read_start((uv_stream_t*)&client->tcp, alloc_cb, on_read);
  } else {
    close_client((uv_handle_t*)&client->tcp);
  }
}

// Public response API implementations; contracts live in noroi.h.

void noroi_res_set_ok_cstr(struct noroi_res_t* res,
                           const char* const type,
                           const size_t type_len,
                           const char* const content,
                           const size_t content_len) {
  char *start = res->buf.base, *end = start + res->buf.len;
  start = _noroi_append_http_status_ok(start, end);
  start = _noroi_append_http_content_type(start, end, type, type_len);
  start = _noroi_append_http_content_length(start, end, content_len);
  start = _noroi_append_http_connection_close(start, end);
  start = _noroi_append_http_crlf(start, end);
  start = _noroi_append(start, end, content, content_len);
  res->buf.len = (size_t)(start - res->buf.base);
}

void noroi_res_set_cstr(struct noroi_res_t* res,
                        const size_t status,
                        const char* type,
                        const size_t type_len,
                        const char* content,
                        const size_t content_len) {
  char *start = res->buf.base, *end = start + res->buf.len;
  start = _noroi_append_http_status(start, end, status);
  start = _noroi_append_http_content_type(start, end, type, type_len);
  start = _noroi_append_http_content_length(start, end, content_len);
  start = _noroi_append_http_connection_close(start, end);
  start = _noroi_append_http_crlf(start, end);
  start = _noroi_append(start, end, content, content_len);
  res->buf.len = (size_t)(start - res->buf.base);
}

void noroi_res_not_found(struct noroi_res_t* res) {
  static const char body[] = "Not Found\n";
  const size_t len = sizeof(body) - 1;
  char *start = res->buf.base, *end = start + res->buf.len;
  static const char status[] = "HTTP/1.0 404 Not Found\r\n";
  start = _noroi_append(start, end, status, sizeof(status) - 1);
  start = _noroi_append_http_content_length(start, end, len);
  start = _noroi_append_http_connection_close(start, end);
  start = _noroi_append_http_crlf(start, end);
  start = _noroi_append(start, end, body, len);
  res->buf.len = (size_t)(start - res->buf.base);
}

void noroi_res_redirect(struct noroi_res_t* res,
                        const char* const url,
                        const size_t url_len) {
  static const char status[] = "HTTP/1.0 302 Found\r\n";
  static const char location[] = "Location: ";
  char *start = res->buf.base, *end = start + res->buf.len;
  start = _noroi_append(start, end, status, sizeof(status) - 1);
  start = _noroi_append(start, end, location, sizeof(location) - 1);
  start = _noroi_append(start, end, url, url_len);
  start = _noroi_append_http_crlf(start, end);
  start = _noroi_append_http_connection_close(start, end);
  start = _noroi_append_http_crlf(start, end);
  res->buf.len = (size_t)(start - res->buf.base);
}

void noroi_res_method_not_allowed(struct noroi_res_t* res) {
  static const char body[] = "Method Not Allowed\n";
  const size_t len = sizeof(body) - 1;
  char *start = res->buf.base, *end = start + res->buf.len;
  static const char status[] = "HTTP/1.0 405 Method Not Allowed\r\n";
  start = _noroi_append(start, end, status, sizeof(status) - 1);
  start = _noroi_append_http_content_length(start, end, len);
  start = _noroi_append_http_connection_close(start, end);
  start = _noroi_append_http_crlf(start, end);
  start = _noroi_append(start, end, body, len);
  res->buf.len = (size_t)(start - res->buf.base);
}

// buf.len is assigned only on success, so a failed allocation leaves whatever
// length the caller's res already held; the null base is the failure marker.
[[nodiscard]] size_t noroi_res_bufalloc(struct noroi_res_t* res, size_t len) {
  res->buf.base = (char*)malloc(len);
  if (!res->buf.base)
    return 1;
  res->buf.len = len;
  return 0;
}

// vsnprintf's own truncation report is the only guard: an offset past buf.len
// makes `remaining` wrap, so the bound is trusted rather than checked.
size_t noroi_res_snfmt(struct noroi_res_t* res,
                       size_t offset,
                       const char* const fmt,
                       ...) {
  va_list args;
  va_start(args, fmt);
  size_t remaining = res->buf.len - offset;
  int written = vsnprintf(res->buf.base + offset, remaining, fmt, args);
  va_end(args);
  if (written < 0 || (size_t)written >= remaining) {
    fprintf(stderr, "Buffer overflow in noroi_res_snfmt\n");
    return 0;
  }
  return (size_t)written;
}

// The bounds test adds before comparing, so a wrapped offset + len can pass
// it.
void noroi_res_mcpy(struct noroi_res_t* res,
                    size_t offset,
                    const char* const s,
                    size_t len) {
  if ((size_t)offset + len > res->buf.len) {
    fprintf(stderr, "Buffer overflow in noroi_res_mcpy\n");
    return;
  }
  memcpy(res->buf.base + offset, s, len);
}

/** Listening TCP handle kept alive while the event loop runs. */
static uv_tcp_t server;

// TCP initialization, IPv4 address conversion, and bind results are not
// inspected. Only the listen result is handled explicitly here.
int noroi_run(void) {
  loop = uv_default_loop();

  struct sockaddr_in addr;

  uv_tcp_init(loop, &server);
  uv_ip4_addr(noroi_conf_ip, noroi_conf_port, &addr);
  uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

  int r = uv_listen((uv_stream_t*)&server, 128, on_connection);
  if (r) {
    fprintf(stderr, "listen: %s\n", uv_strerror(r));
    uv_close((uv_handle_t*)&server, NULL);
    uv_run(loop, UV_RUN_DEFAULT);
    return EXIT_FAILURE;
  }

  return uv_run(loop, UV_RUN_DEFAULT);
}

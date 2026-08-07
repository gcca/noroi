#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef noroi_conf_ip
#define noroi_conf_ip "0.0.0.0"
#endif

#ifndef noroi_conf_port
#define noroi_conf_port 8000
#endif

struct noroi_req_t {
  const uv_buf_t* buf;
  size_t nread;
};

struct noroi_res_t {
  uv_buf_t buf;
};

extern void noroi_handle(const struct noroi_req_t*, struct noroi_res_t*);

void noroi_req_parse_url(const struct noroi_req_t* req,
                         const char** method,
                         size_t* method_size,
                         const char** path,
                         size_t* path_size,
                         const char** query,
                         size_t* query_size) {
  if (!method_size || !path_size)
    return;
  if (method)
    *method = NULL;
  if (path)
    *path = NULL;
  if (query)
    *query = NULL;
  if (query_size)
    *query_size = 0;
  if (!req || !req->buf || !req->buf->base || !method || !path) {
    *method_size = 0;
    *path_size = 0;
    return;
  }

  const char* base = req->buf->base;
  const size_t n = req->nread > 2048 ? 2048 : req->nread;
  size_t i = 0;
  while (i < n && base[i] != ' ')
    i++;
  if (i == 0 || i >= n) {
    *method_size = 0;
    *path_size = 0;
    return;
  }
  const size_t method_len = i;
  const size_t path_start = i + 1;
  size_t j = path_start;
  while (j < n && base[j] != ' ' && base[j] != '?')
    j++;
  if (j >= n || j == path_start) {
    *method_size = 0;
    *path_size = 0;
    return;
  }
  const size_t path_len = j - path_start;

  size_t q_start = 0;
  size_t q_len = 0;
  if (j < n && base[j] == '?') {
    q_start = j + 1;
    size_t k = q_start;
    while (k < n && base[k] != ' ')
      k++;
    if (k >= n) {
      *method_size = 0;
      *path_size = 0;
      return;
    }
    q_len = k - q_start;
  } else if (j >= n || base[j] != ' ') {
    *method_size = 0;
    *path_size = 0;
    return;
  }

  *method = base;
  *path = base + path_start;
  *method_size = method_len;
  *path_size = path_len;
  if (query && query_size && q_start) {
    *query = base + q_start;
    *query_size = q_len;
  }
}

static inline char* _noroi_append(char* base,
                                  const char* end,
                                  const char* s,
                                  size_t len) {
  while (base < end && len > 0) {
    *base = *s;
    base++;
    s++;
    len--;
  }
  if (len > 0)
    fprintf(stderr, "Buffer overflow in _noroi_append\n");
  return base;
}

static inline char* _noroi_append_http_status(char* base, const char* end) {
  static const char status[] = "HTTP/1.0 200 OK\r\n";
  return _noroi_append(base, end, status, sizeof(status) - 1);
}

static inline char* _noroi_append_http_content_type(char* base,
                                                    const char* end) {
  static const char content_type[] = "Content-Type: text/plain\r\n";
  return _noroi_append(base, end, content_type, sizeof(content_type) - 1);
}

static inline char* _noroi_append_http_connection_close(char* base,
                                                        const char* end) {
  static const char connection_close[] = "Connection: close\r\n";
  return _noroi_append(base, end, connection_close,
                       sizeof(connection_close) - 1);
}

static inline char* _noroi_append_http_content_length(char* base,
                                                      const char* end,
                                                      size_t len) {
  static const char content_length[] = "Content-Length: ";
  base = _noroi_append(base, end, content_length, sizeof(content_length) - 1);
  char len_str[32];
  int len_str_len = snprintf(len_str, sizeof(len_str), "%zu", len);
  base = _noroi_append(base, end, len_str, (size_t)len_str_len);
  static const char crlf[] = "\r\n";
  return _noroi_append(base, end, crlf, sizeof(crlf) - 1);
}

static inline char* _noroi_append_http_crlf(char* base, const char* end) {
  static const char crlf[] = "\r\n";
  return _noroi_append(base, end, crlf, sizeof(crlf) - 1);
}

void noroi_res_set_content_cstr(struct noroi_res_t* res,
                                char* xbuf,
                                const size_t xbuf_size,
                                const char* s,
                                const size_t len) {
  char *start = xbuf, *end = xbuf + xbuf_size;
  xbuf = _noroi_append_http_status(xbuf, end);
  xbuf = _noroi_append_http_content_type(xbuf, end);
  xbuf = _noroi_append_http_content_length(xbuf, end, len);
  xbuf = _noroi_append_http_connection_close(xbuf, end);
  xbuf = _noroi_append_http_crlf(xbuf, end);
  xbuf = _noroi_append(xbuf, end, s, len);
  res->buf = uv_buf_init(start, (size_t)(xbuf - start));
}

void noroi_res_not_found(struct noroi_res_t* res,
                         char* xbuf,
                         const size_t xbuf_size) {
  static const char body[] = "Not Found\n";
  const size_t len = sizeof(body) - 1;
  char *start = xbuf, *end = xbuf + xbuf_size;
  static const char status[] = "HTTP/1.0 404 Not Found\r\n";
  xbuf = _noroi_append(xbuf, end, status, sizeof(status) - 1);
  xbuf = _noroi_append_http_content_type(xbuf, end);
  xbuf = _noroi_append_http_content_length(xbuf, end, len);
  xbuf = _noroi_append_http_connection_close(xbuf, end);
  xbuf = _noroi_append_http_crlf(xbuf, end);
  xbuf = _noroi_append(xbuf, end, body, len);
  res->buf = uv_buf_init(start, (size_t)(xbuf - start));
}

void noroi_res_redirect(struct noroi_res_t* res,
                        char* xbuf,
                        const size_t xbuf_size,
                        const char* url,
                        const size_t url_len) {
  char *start = xbuf, *end = xbuf + xbuf_size;
  static const char status[] = "HTTP/1.0 302 Found\r\n";
  static const char location[] = "Location: ";
  xbuf = _noroi_append(xbuf, end, status, sizeof(status) - 1);
  xbuf = _noroi_append(xbuf, end, location, sizeof(location) - 1);
  xbuf = _noroi_append(xbuf, end, url, url_len);
  xbuf = _noroi_append_http_crlf(xbuf, end);
  xbuf = _noroi_append_http_content_length(xbuf, end, 0);
  xbuf = _noroi_append_http_connection_close(xbuf, end);
  xbuf = _noroi_append_http_crlf(xbuf, end);
  res->buf = uv_buf_init(start, (size_t)(xbuf - start));
}

static uv_loop_t* loop;
static uv_tcp_t server;

typedef struct {
  uv_tcp_t tcp;
  uv_write_t write_req;
} client_t;

static void on_close(uv_handle_t* handle) {
  free(handle);
}

static void close_client(uv_handle_t* handle) {
  if (!uv_is_closing(handle))
    uv_close(handle, on_close);
}

static void alloc_cb(uv_handle_t*, size_t suggested, uv_buf_t* buf) {
  buf->base = (char*)malloc(suggested);
  buf->len = buf->base ? suggested : 0;
}

static void on_write(uv_write_t* req, int status) {
  if (status < 0)
    fprintf(stderr, "write: %s\n", uv_strerror(status));
  close_client((uv_handle_t*)req->handle);
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  client_t* client = (client_t*)stream;

  if (nread > 0) {
    struct noroi_req_t req = {buf, (size_t)nread};
    struct noroi_res_t res;
    noroi_handle(&req, &res);
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

static void on_connection(uv_stream_t* server_handle, int status) {
  if (status < 0) {
    fprintf(stderr, "connection: %s\n", uv_strerror(status));
    return;
  }

  client_t* client = (client_t*)malloc(sizeof(*client));
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

int noroi_run() {
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

#ifdef __cplusplus
}
#endif

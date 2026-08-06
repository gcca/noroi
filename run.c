#include "lwlog.h"
#include "noroi.h"

struct Route {
  const char* path;
  void (*handler)(const struct noroi_req_t*, struct noroi_res_t*);
};

void handler_welcome(const struct noroi_req_t*, struct noroi_res_t* res) {
  static const char body[] = "Welcome\n";
  static char xbuf[512];
  noroi_res_set_content_cstr(res, xbuf, sizeof(xbuf), body, sizeof(body) - 1);
}

void handler_healthcheck(const struct noroi_req_t*, struct noroi_res_t* res) {
  static const char body[] = "🍻\n";
  static char xbuf[512];
  noroi_res_set_content_cstr(res, xbuf, sizeof(xbuf), body, sizeof(body) - 1);
}

const struct Route routes[] = {
    {"/welcome", handler_welcome},
    {"/healthcheck", handler_healthcheck},
};

void route_dispatcher(const struct noroi_req_t* req, struct noroi_res_t* res) {
  const char *method, *path, *query;
  size_t method_size, path_size, query_size;

  noroi_req_parse_url(req, &method, &method_size, &path, &path_size, &query,
                      &query_size);

  lwlog_debug("Request sizes: method_size=%zu, path_size=%zu, query=%zu",
              method_size, path_size, query_size);
  lwlog_debug("Request method: %.*s, path: %.*s, query: %.*s", (int)method_size,
              method, (int)path_size, path, (int)query_size, query);

  for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
    if (!strncmp(path, routes[i].path, path_size)) {
      routes[i].handler(req, res);
      return;
    }
  }

  static char xbuf[512];
  noroi_res_not_found(res, xbuf, sizeof(xbuf));
}

void noroi_handle(const struct noroi_req_t* req, struct noroi_res_t* res) {
  lwlog_info("Handling request");
  route_dispatcher(req, res);
  lwlog_info("Response sent");
}

int main(void) {
  lwlog_info("Running on http://127.0.0.1:%d", noroi_conf_port);
  return noroi_run();
}

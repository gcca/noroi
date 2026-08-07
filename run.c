#include "lwlog.h"
#include "noroi.h"

struct Context {
  const struct noroi_req_t* req;
  struct noroi_res_t* res;
  const char* const method;
  const size_t method_size;
};

struct Route {
  const char* path;
  void (*handler)(struct Context c);
};

void handler_index(struct Context c) {
  static const char url[] = "/welcome";
  static char xbuf[128];
  noroi_res_redirect(c.res, xbuf, sizeof(xbuf), url, sizeof(url) - 1);
}

void handler_welcome(struct Context c) {
  static const char body[] = "Welcome\n";
  static char xbuf[512];
  noroi_res_set_content_cstr(c.res, xbuf, sizeof(xbuf), body, sizeof(body) - 1);
}

void handler_healthcheck(struct Context c) {
  static const char body[] = "🍻\n";
  static char xbuf[512];
  noroi_res_set_content_cstr(c.res, xbuf, sizeof(xbuf), body, sizeof(body) - 1);
}

const struct Route routes[] = {
    {"/", handler_index},
    {"/welcome", handler_welcome},
    {"/healthcheck", handler_healthcheck},
};

void route_map_dispatcher(const struct noroi_req_t* req,
                          struct noroi_res_t* res) {
  const char *method, *path, *query;
  size_t method_size, path_size, query_size;

  noroi_req_parse_url(req, &method, &method_size, &path, &path_size, &query,
                      &query_size);

  lwlog_debug("method_size=%zu, path_size=%zu, query=%zu", method_size,
              path_size, query_size);
  lwlog_debug("method: %.*s, path: %.*s, query: %.*s", (int)method_size, method,
              (int)path_size, path, (int)query_size, query);

  for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
    if (!strncmp(path, routes[i].path, path_size)) {
      routes[i].handler((struct Context){req, res, method, method_size});
      return;
    }
  }

  static char xbuf[512];
  noroi_res_not_found(res, xbuf, sizeof(xbuf));
}

void noroi_handle(const struct noroi_req_t* req, struct noroi_res_t* res) {
  lwlog_info("Handling request");
  route_map_dispatcher(req, res);
  lwlog_info("Response sent");
}

int main(void) {
  lwlog_info("Running on http://127.0.0.1:%d", noroi_conf_port);
  return noroi_run();
}

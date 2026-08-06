#include "lwlog.h"
#include "noroi.h"

void noroi_handle(const struct noroi_req_t* req, struct noroi_res_t* res) {
  lwlog_info("Handling request");

  const char *method, *path, *query;
  size_t method_size, path_size, query_size;

  noroi_req_parse_url(req, &method, &method_size, &path, &path_size, &query,
                      &query_size);
  lwlog_info("Request sizes: method_size=%zu, path_size=%zu, query=%zu",
             method_size, path_size, query_size);
  lwlog_info("Request method: %.*s, path: %.*s, query: %.*s", (int)method_size,
             method, (int)path_size, path, (int)query_size, query);

  static const char paths_welcome[] = "/welcome";
  static char xbuf[4092];

  if (!strncmp(path, paths_welcome, path_size)) {
    const char body[] = "🍻\n";
    noroi_res_set_content_cstr(res, xbuf, sizeof(xbuf), body, sizeof(body) - 1);
    return;
  }

  noroi_res_not_found(res, xbuf, sizeof(xbuf));

  lwlog_info("Response sent");
}

int main(void) {
  lwlog_info("Running on http://127.0.0.1:%d", noroi_conf_port);
  return noroi_run();
}

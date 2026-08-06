#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>

#include <sqlite3.h>

#include "lwlog.h"
#include "mustache.hpp"
#include "noroi.h"
#include "template.hpp"
#include "yyjson.h"

namespace mst = kainjow::mustache;

namespace {

using TextField = std::pair<std::string, std::string>;

mst::data MakeObject(std::initializer_list<TextField> fields) {
  mst::data object;
  for (const auto& [name, value] : fields)
    object.set(name, value);
  return object;
}

mst::data MakeWelcomeData() {
  mst::data data;
  data.set("page_title", "Noroi — Small server, serious velocity");
  data.set("meta_description",
           "A live Mustache-powered welcome page for the Noroi HTTP server.");
  data.set("brand_name", "Noroi");
  data.set("brand_tagline", "Tiny HTTP systems");
  data.set("header_cta", "View health");

  mst::data nav_items{mst::data::type::list};
  auto overview = MakeObject(
      {{"label", "Overview"}, {"href", "#overview"}, {"state", "is-current"}});
  overview.set("current", true);
  nav_items << overview
            << MakeObject({{"label", "Capabilities"},
                           {"href", "#capabilities"},
                           {"state", ""}})
            << MakeObject({{"label", "Workflow"},
                           {"href", "#workflow"},
                           {"state", ""}})
            << MakeObject({{"label", "Status"},
                           {"href", "/healthcheck"},
                           {"state", ""}});
  data.set("nav_items", nav_items);

  data.set("announcement",
           MakeObject({{"badge", "Fresh"},
                       {"message", "The welcome route now renders Mustache."},
                       {"href", "#capabilities"},
                       {"link_label", "See what changed"}}));
  data.set("eyebrow", "C++ at the edge");
  data.set("hero_title", "Ship less machinery.");
  data.set("hero_highlight", "Move more traffic.");
  data.set(
      "hero_description",
      "Noroi pairs a compact libuv core with expressive C++ templates, giving "
      "you a fast path from a request to a polished, data-driven response.");
  data.set("primary_cta", "Explore the system");
  data.set("secondary_cta", "How it works");

  mst::data team{mst::data::type::list};
  team << MakeObject({{"initials", "AK"}}) << MakeObject({{"initials", "LM"}})
       << MakeObject({{"initials", "RO"}}) << MakeObject({{"initials", "+8"}});
  data.set("team", team);
  data.set("trust_line", "Built for teams who prefer readable infrastructure.");

  data.set("dashboard_status", "All systems nominal");
  data.set("dashboard_eyebrow", "Live traffic");
  data.set("dashboard_title", "Welcome service");
  data.set("environment", "prod / lima-1");

  mst::data metrics{mst::data::type::list};
  metrics << MakeObject({{"label", "Requests"},
                         {"value", "28.4k"},
                         {"change", "+18.2%"}})
          << MakeObject({{"label", "p95 latency"},
                         {"value", "4.8ms"},
                         {"change", "−1.3ms"}})
          << MakeObject({{"label", "Availability"},
                         {"value", "99.99%"},
                         {"change", "30 days"}});
  data.set("metrics", metrics);

  mst::data activities{mst::data::type::list};
  activities << MakeObject({{"marker", "R"},
                            {"tone", ""},
                            {"title", "Template rendered"},
                            {"description", "GET /welcome · 200 OK"},
                            {"time", "now"}})
             << MakeObject({{"marker", "H"},
                            {"tone", "tone-cyan"},
                            {"title", "Health probe passed"},
                            {"description", "GET /healthcheck · 200 OK"},
                            {"time", "8s"}})
             << MakeObject({{"marker", "D"},
                            {"tone", "tone-amber"},
                            {"title", "Release promoted"},
                            {"description", "clang++ · optimized build"},
                            {"time", "2m"}});
  data.set("activities", activities);

  mst::data stats{mst::data::type::list};
  stats << MakeObject({{"value", "1"},
                       {"label", "Event loop"},
                       {"detail", "Focused and predictable"}})
        << MakeObject({{"value", "0"},
                       {"label", "Runtime frameworks"},
                       {"detail", "Only the pieces you need"}})
        << MakeObject({{"value", "5.0"},
                       {"label", "Mustache engine"},
                       {"detail", "Logic-less by design"}})
        << MakeObject({{"value", "C++23"},
                       {"label", "Toolchain"},
                       {"detail", "Compiled with clang++"}});
  data.set("stats", stats);

  data.set("features_eyebrow", "Capabilities");
  data.set("features_title", "A small surface area with room for real ideas.");
  data.set(
      "features_description",
      "Each layer stays visible: parse the request, shape the data, render "
      "the view, and send an exact response.");

  mst::data features{mst::data::type::list};
  auto templates = MakeObject(
      {{"layout", "card-wide"},
       {"icon", "{}"},
       {"kicker", "Presentation"},
       {"title", "Escaped, data-driven templates"},
       {"description",
        "Mustache variables, sections, and lists keep HTML expressive without "
        "moving business logic into the view."}});
  templates.set("is_new", true);
  features << templates
           << MakeObject({{"layout", "card-tall"},
                          {"icon", "↯"},
                          {"kicker", "Runtime"},
                          {"title", "A direct libuv path"},
                          {"description",
                           "Requests flow through a lean event loop with no "
                           "hidden middleware stack."}})
           << MakeObject({{"layout", ""},
                          {"icon", "<>"},
                          {"kicker", "Toolchain"},
                          {"title", "Modern C++ compilation"},
                          {"description",
                           "The application compiles and links with clang++, "
                           "ready for standard library types."}})
           << MakeObject(
                  {{"layout", "card-wide"},
                   {"icon", "◎"},
                   {"kicker", "Response"},
                   {"title", "Exact buffers, proper HTML"},
                   {"description",
                    "Rendered output determines response capacity and is "
                    "served with an explicit UTF-8 HTML content type."}});
  data.set("features", features);

  data.set("workflow_eyebrow", "Workflow");
  data.set("workflow_title", "From route to response in three legible moves.");
  data.set("workflow_description",
           "The welcome handler is intentionally straightforward, so the "
           "boundary between HTTP, application data, and HTML stays clear.");

  mst::data steps{mst::data::type::list};
  steps << MakeObject({{"number", "01"},
                       {"title", "Shape the context"},
                       {"description",
                        "Build objects and lists that describe navigation, "
                        "metrics, features, and live activity."}})
        << MakeObject({{"number", "02"},
                       {"title", "Render with Mustache"},
                       {"description",
                        "Apply the context to one reusable template while "
                        "escaping inserted values by default."}})
        << MakeObject({{"number", "03"},
                       {"title", "Send the exact result"},
                       {"description",
                        "Allocate from the rendered size, attach HTML headers, "
                        "and hand the buffer back to libuv."}});
  data.set("steps", steps);

  data.set("testimonial",
           MakeObject({{"initials", "NR"},
                       {"quote",
                        "The best server code is the kind you can hold in your "
                        "head—and still make the page feel considered."},
                       {"name", "Noroi Runtime"},
                       {"role", "Serving small, deliberate systems"}}));

  data.set("cta_eyebrow", "Ready on :8000");
  data.set("cta_title", "The route is live. Make it yours.");
  data.set("cta_description",
           "Replace the sample context with application data and keep the "
           "template as the presentation boundary.");
  data.set("cta_button", "Check service health");
  data.set("current_year", "2026");
  data.set("footer_note", "Rendered with Mustache and served by libuv.");

  mst::data footer_links{mst::data::type::list};
  footer_links << MakeObject({{"label", "Overview"}, {"href", "#overview"}})
               << MakeObject(
                      {{"label", "Capabilities"}, {"href", "#capabilities"}})
               << MakeObject({{"label", "Health"}, {"href", "/healthcheck"}});
  data.set("footer_links", footer_links);

  return data;
}

void SetTemplateError(struct noroi_res_t* res) {
  static const char body[] = "Failed to render welcome template\n";
  static const char content_type[] = "text/plain";
  if (noroi_res_bufalloc(res, 512)) {
    lwlog_err("Failed to allocate response buffer");
    return;
  }
  noroi_res_set_ok_cstr_static(res, content_type, body);
}

const char* SampleDataPath() {
  const char* const path = std::getenv("NOROI_SAMPLE_DATA");
  return path && *path ? path : "sample-data.sql";
}

const char* DatabasePath() {
  const char* const path = std::getenv("NOROI_DATABASE");
  return path && *path ? path : "noroi.db";
}

std::string ReadFile(const char* path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return {};
  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

bool CreateDatabase() {
  const char* const db_path = DatabasePath();
  sqlite3* db = nullptr;
  if (sqlite3_open(db_path, &db) != SQLITE_OK) {
    lwlog_err("Failed to create database %s: %s", db_path, sqlite3_errmsg(db));
    sqlite3_close(db);
    return false;
  }

  char* error = nullptr;
  const int created = sqlite3_exec(
      db,
      "CREATE TABLE IF NOT EXISTS auth_user ("
      "  username TEXT PRIMARY KEY,"
      "  password TEXT NOT NULL,"
      "  email TEXT NOT NULL UNIQUE,"
      "  is_active INTEGER NOT NULL DEFAULT 1,"
      "  created_at INTEGER NOT NULL"
      ");"
      "CREATE TABLE IF NOT EXISTS dash_app ("
      "  appname TEXT PRIMARY KEY,"
      "  description TEXT NOT NULL"
      ");"
      "CREATE TABLE IF NOT EXISTS dash_binding ("
      "  username TEXT NOT NULL REFERENCES auth_user(username) ON DELETE "
      "CASCADE,"
      "  appname TEXT NOT NULL REFERENCES dash_app(appname) ON DELETE CASCADE,"
      "  PRIMARY KEY (username, appname)"
      ");",
      nullptr, nullptr, &error);
  if (created != SQLITE_OK) {
    lwlog_err("Failed to create sample schema: %s", error);
    sqlite3_free(error);
    sqlite3_close(db);
    return false;
  }

  const char* const script_path = SampleDataPath();
  const std::string script = ReadFile(script_path);
  if (script.empty()) {
    lwlog_warning("Sample data script is missing or empty: %s", script_path);
    sqlite3_close(db);
    return true;
  }

  if (sqlite3_exec(db, script.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    lwlog_err("Failed to load sample data from %s: %s", script_path, error);
    sqlite3_free(error);
    sqlite3_close(db);
    return false;
  }

  sqlite3_close(db);
  lwlog_info("Created %s with sample data from %s", db_path, script_path);
  return true;
}

sqlite3* OpenDatabase() {
  const char* const path = DatabasePath();
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    lwlog_err("Failed to open database %s: %s", path, sqlite3_errmsg(db));
    sqlite3_close(db);
    return nullptr;
  }
  return db;
}

struct DatabaseGuard {
  sqlite3* const db;

  ~DatabaseGuard() { sqlite3_close(db); }
};

const char* ColumnText(sqlite3_stmt* stmt, int column) {
  const unsigned char* const text = sqlite3_column_text(stmt, column);
  return text ? reinterpret_cast<const char*>(text) : "";
}

template <class RowHandler>
bool ForEachRow(sqlite3* db, const char* query, RowHandler on_row) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
    lwlog_err("Failed to prepare query: %s", sqlite3_errmsg(db));
    return false;
  }

  int status;
  while ((status = sqlite3_step(stmt)) == SQLITE_ROW)
    on_row(stmt);
  if (status != SQLITE_DONE)
    lwlog_err("Failed to read rows: %s", sqlite3_errmsg(db));

  sqlite3_finalize(stmt);
  return status == SQLITE_DONE;
}

void AddListSizes(yyjson_mut_doc* doc,
                  yyjson_mut_val* array,
                  const char* list_key,
                  const char* count_key) {
  size_t index, max;
  yyjson_mut_val* entry;
  yyjson_mut_arr_foreach(array, index, max, entry) {
    yyjson_mut_obj_add_uint(
        doc, entry, count_key,
        yyjson_mut_arr_size(yyjson_mut_obj_get(entry, list_key)));
  }
}

bool ReadTotals(sqlite3* db, yyjson_mut_doc* doc, yyjson_mut_val* totals) {
  return ForEachRow(
      db,
      "SELECT (SELECT COUNT(*) FROM auth_user),"
      "       (SELECT COUNT(*) FROM auth_user WHERE is_active = 1),"
      "       (SELECT COUNT(*) FROM dash_app),"
      "       (SELECT COUNT(*) FROM dash_binding);",
      [doc, totals](sqlite3_stmt* stmt) {
        yyjson_mut_obj_add_sint(doc, totals, "users",
                                sqlite3_column_int64(stmt, 0));
        yyjson_mut_obj_add_sint(doc, totals, "active_users",
                                sqlite3_column_int64(stmt, 1));
        yyjson_mut_obj_add_sint(doc, totals, "apps",
                                sqlite3_column_int64(stmt, 2));
        yyjson_mut_obj_add_sint(doc, totals, "bindings",
                                sqlite3_column_int64(stmt, 3));
      });
}

bool ReadUsers(sqlite3* db, yyjson_mut_doc* doc, yyjson_mut_val* users) {
  std::string current;
  yyjson_mut_val* apps = nullptr;
  const bool ok = ForEachRow(
      db,
      "SELECT u.username, u.email, u.is_active, u.created_at,"
      "       a.appname, a.description"
      "  FROM auth_user AS u"
      "  LEFT JOIN dash_binding AS b ON b.username = u.username"
      "  LEFT JOIN dash_app AS a ON a.appname = b.appname"
      " ORDER BY u.username, a.appname;",
      [&](sqlite3_stmt* stmt) {
        const char* const username = ColumnText(stmt, 0);
        if (!apps || current != username) {
          current = username;
          yyjson_mut_val* const user = yyjson_mut_obj(doc);
          yyjson_mut_arr_append(users, user);
          yyjson_mut_obj_add_strcpy(doc, user, "username", username);
          yyjson_mut_obj_add_strcpy(doc, user, "email", ColumnText(stmt, 1));
          yyjson_mut_obj_add_bool(doc, user, "is_active",
                                  sqlite3_column_int(stmt, 2) != 0);
          yyjson_mut_obj_add_sint(doc, user, "created_at",
                                  sqlite3_column_int64(stmt, 3));
          apps = yyjson_mut_arr(doc);
          yyjson_mut_obj_add_val(doc, user, "apps", apps);
        }
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
          yyjson_mut_val* const app = yyjson_mut_obj(doc);
          yyjson_mut_arr_append(apps, app);
          yyjson_mut_obj_add_strcpy(doc, app, "appname", ColumnText(stmt, 4));
          yyjson_mut_obj_add_strcpy(doc, app, "description",
                                    ColumnText(stmt, 5));
        }
      });
  if (ok)
    AddListSizes(doc, users, "apps", "app_count");
  return ok;
}

bool ReadApps(sqlite3* db, yyjson_mut_doc* doc, yyjson_mut_val* apps) {
  std::string current;
  yyjson_mut_val* users = nullptr;
  const bool ok =
      ForEachRow(db,
                 "SELECT a.appname, a.description, u.username"
                 "  FROM dash_app AS a"
                 "  LEFT JOIN dash_binding AS b ON b.appname = a.appname"
                 "  LEFT JOIN auth_user AS u ON u.username = b.username"
                 " ORDER BY a.appname, u.username;",
                 [&](sqlite3_stmt* stmt) {
                   const char* const appname = ColumnText(stmt, 0);
                   if (!users || current != appname) {
                     current = appname;
                     yyjson_mut_val* const app = yyjson_mut_obj(doc);
                     yyjson_mut_arr_append(apps, app);
                     yyjson_mut_obj_add_strcpy(doc, app, "appname", appname);
                     yyjson_mut_obj_add_strcpy(doc, app, "description",
                                               ColumnText(stmt, 1));
                     users = yyjson_mut_arr(doc);
                     yyjson_mut_obj_add_val(doc, app, "users", users);
                   }
                   if (sqlite3_column_type(stmt, 2) != SQLITE_NULL)
                     yyjson_mut_arr_add_strcpy(doc, users, ColumnText(stmt, 2));
                 });
  if (ok)
    AddListSizes(doc, apps, "users", "user_count");
  return ok;
}

bool MakeDashboardJson(sqlite3* db, std::string& out) {
  yyjson_mut_doc* const doc = yyjson_mut_doc_new(nullptr);
  if (!doc) {
    lwlog_err("Failed to allocate JSON document");
    return false;
  }

  yyjson_mut_val* const root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_obj_add_str(doc, root, "service", "noroi");
  yyjson_mut_obj_add_str(doc, root, "endpoint", "/api/dashboard");

  yyjson_mut_val* const source = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, root, "source", source);
  yyjson_mut_obj_add_str(doc, source, "engine", "sqlite");
  yyjson_mut_obj_add_str(doc, source, "library_version", sqlite3_libversion());
  yyjson_mut_obj_add_strcpy(doc, source, "database", DatabasePath());
  yyjson_mut_obj_add_strcpy(doc, source, "script", SampleDataPath());

  yyjson_mut_val* const totals = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, root, "totals", totals);
  yyjson_mut_val* const users = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, root, "users", users);
  yyjson_mut_val* const apps = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, root, "apps", apps);

  bool ok = ReadTotals(db, doc, totals) && ReadUsers(db, doc, users) &&
            ReadApps(db, doc, apps);
  if (ok) {
    size_t length = 0;
    char* const json = yyjson_mut_write(doc, 0, &length);
    if (json) {
      out.assign(json, length);
      std::free(json);
    } else {
      lwlog_err("Failed to serialize JSON document");
      ok = false;
    }
  }

  yyjson_mut_doc_free(doc);
  return ok;
}

}  // namespace

struct Context {
  const struct noroi_req_t* const req;
  struct noroi_res_t* const res;
  const char* const method;
  const size_t method_size;
  const char* const path;
  const size_t path_size;
  const char* const query;
  const size_t query_size;
  const char* headers_begin;
  const char* headers_end;
  const char* body_begin;
  const char* body_end;
};

struct Route {
  const char* path;
  void (*handler)(struct Context c);
};

void handler_index(struct Context c) {
  if (noroi_res_bufalloc(c.res, 512)) {
    lwlog_err("Failed to allocate response buffer");
    return;
  }
  static const char url[] = "/welcome";
  noroi_res_redirect(c.res, url, sizeof(url) - 1);
}

void handler_welcome(struct Context c) {
  if (c.method_size != 3 || std::strncmp(c.method, "GET", 3) != 0) {
    if (noroi_res_bufalloc(c.res, 256)) {
      lwlog_err("Failed to allocate response buffer");
      return;
    }
    noroi_res_method_not_allowed(c.res);
    return;
  }

  mst::mustache welcome_template{noroi::templates::kWelcome};
  if (!welcome_template.is_valid()) {
    lwlog_err("Invalid welcome template: %s",
              welcome_template.error_message().c_str());
    SetTemplateError(c.res);
    return;
  }

  mst::data data = MakeWelcomeData();
  const std::string body = welcome_template.render(data);
  if (!welcome_template.is_valid()) {
    lwlog_err("Failed to render welcome template: %s",
              welcome_template.error_message().c_str());
    SetTemplateError(c.res);
    return;
  }

  static constexpr char kHeaders[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Content-Length: %zu\r\n"
      "Connection: close\r\n"
      "\r\n";
  const int measured_header_size =
      std::snprintf(nullptr, 0, kHeaders, body.size());
  if (measured_header_size <= 0) {
    lwlog_err("Failed to build response headers");
    SetTemplateError(c.res);
    return;
  }

  const size_t header_size = static_cast<size_t>(measured_header_size);
  if (noroi_res_bufalloc(c.res, header_size + body.size())) {
    lwlog_err("Failed to allocate response buffer");
    return;
  }

  const size_t written = noroi_res_snfmt(c.res, 0, kHeaders, body.size());
  if (written != header_size) {
    lwlog_err("Failed to build response headers");
    static const char error[] = "Failed to render welcome template\n";
    static const char content_type[] = "text/plain";
    noroi_res_set_ok_cstr_static(c.res, content_type, error);
    return;
  }

  noroi_res_mcpy(c.res, static_cast<ptrdiff_t>(header_size), body.data(),
                 body.size());
}

void handler_healthcheck(struct Context c) {
  if (noroi_res_bufalloc(c.res, 512)) {
    lwlog_err("Failed to allocate response buffer");
    return;
  }
  static const char body[] = "🍻\n";
  static const char content_type[] = "text/plain";
  noroi_res_set_ok_cstr_static(c.res, content_type, body);
}

void handler_misc(struct Context c) {
  static const char body[] = "Misc";
  static const char content_type[] = "text/plain";
  if (noroi_res_bufalloc(c.res, 128)) {
    fprintf(stderr, "Fail allocating\n");
    return;
  }
  noroi_res_set_cstr_static(c.res, 418, content_type, body);
}

void handler_dashboard(struct Context c) {
  if (c.method_size != 3 || std::strncmp(c.method, "GET", 3) != 0) {
    if (noroi_res_bufalloc(c.res, 256)) {
      lwlog_err("Failed to allocate response buffer");
      return;
    }
    noroi_res_method_not_allowed(c.res);
    return;
  }

  static constexpr char kOkHeaders[] =
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: %zu\r\n"
      "Connection: close\r\n"
      "\r\n";
  static constexpr char kErrorHeaders[] =
      "HTTP/1.0 500 Internal Server Error\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: %zu\r\n"
      "Connection: close\r\n"
      "\r\n";

  sqlite3* const db = OpenDatabase();
  const DatabaseGuard guard{db};
  std::string body;
  const char* headers = kOkHeaders;
  if (!db || !MakeDashboardJson(db, body)) {
    lwlog_err("Failed to build dashboard payload");
    body = "{\"error\":\"database unavailable\"}";
    headers = kErrorHeaders;
  }

  static const char error[] = "Failed to serialize dashboard\n";
  const int measured_header_size =
      std::snprintf(nullptr, 0, headers, body.size());
  if (measured_header_size <= 0) {
    lwlog_err("Failed to build response headers");
    if (noroi_res_bufalloc(c.res, 512)) {
      lwlog_err("Failed to allocate response buffer");
      return;
    }
    static const char content_type[] = "text/plain";
    noroi_res_set_ok_cstr_static(c.res, content_type, error);
    return;
  }

  const size_t header_size = static_cast<size_t>(measured_header_size);
  if (noroi_res_bufalloc(c.res, header_size + body.size())) {
    lwlog_err("Failed to allocate response buffer");
    return;
  }

  const size_t written = noroi_res_snfmt(c.res, 0, headers, body.size());
  if (written != header_size) {
    lwlog_err("Failed to build response headers");
    static const char content_type[] = "text/plain";
    noroi_res_set_ok_cstr_static(c.res, content_type, error);
    return;
  }

  noroi_res_mcpy(c.res, static_cast<ptrdiff_t>(header_size), body.data(),
                 body.size());
}

void handler_get(struct Context c) {
  if (noroi_res_bufalloc(c.res, 256)) {
    lwlog_err("Failed bufalloc at handler_get");
    return;
  }

  yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val* root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);

  const char *s = c.query, *e = s + c.query_size;
  while (s <= e) {
    const char *ns = s, *ne = s;
    while (*++ne != '=' && ne < e)
      ;
    const char *vs = ne + 1, *ve = ne;
    while (*++ve != '&' && ve < e)
      ;

    yyjson_mut_val* obj = yyjson_mut_arr_add_obj(doc, root);
    yyjson_mut_obj_add_strncpy(doc, obj, "name", ns, (ne - ns) & 0x7Full);
    yyjson_mut_obj_add_strncpy(doc, obj, "value", vs, (ve - vs) & 0x7Full);

    s = ve + 1;
  }

  size_t json_len = 0;
  char* json = yyjson_mut_write(doc, 0, &json_len);

  static const char content_type[] = "application/json";
  noroi_res_set_ok_cstr(c.res, content_type, sizeof(content_type) - 1, json,
                        json_len);

  free(json);
  yyjson_mut_doc_free(doc);
}

void handler_post(struct Context c) {
  if (noroi_res_bufalloc(c.res, 256)) {
    lwlog_err("Failed bufalloc at handler_post");
    return;
  }

  if (!std::strcmp("POST", c.method)) {
    noroi_res_method_not_allowed(c.res);
    return;
  }

  yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val* root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);

  const char *s = c.body_begin, *e = c.body_end;
  while (s <= e) {
    const char *ns = s, *ne = s;
    while (*++ne != '=' && ne < e)
      ;
    const char *vs = ne + 1, *ve = ne;
    while (*++ve != '&' && ve < e)
      ;

    yyjson_mut_val* obj = yyjson_mut_arr_add_obj(doc, root);
    yyjson_mut_obj_add_strncpy(doc, obj, "name", ns, (ne - ns) & 0x7Full);
    yyjson_mut_obj_add_strncpy(doc, obj, "value", vs, (ve - vs) & 0x7Full);

    s = ve + 1;
  }

  size_t json_len = 0;
  char* json = yyjson_mut_write(doc, 0, &json_len);

  static const char content_type[] = "application/json";
  noroi_res_set_ok_cstr(c.res, content_type, sizeof(content_type) - 1, json,
                        json_len);

  free(json);
  yyjson_mut_doc_free(doc);
}

const struct Route routes[] = {
    {"/", handler_index},
    {"/welcome", handler_welcome},
    {"/healthcheck", handler_healthcheck},
    {"/dashboard", handler_dashboard},
    {"/misc", handler_misc},
    {"/get", handler_get},
    {"/post", handler_post},
};

void route_map_dispatcher(const struct noroi_req_t* req,
                          struct noroi_res_t* res) {
  const char *method, *path, *query, *headers_begin, *headers_end, *body_begin,
      *body_end;
  size_t method_size, path_size, query_size;

  noroi_req_parse_url_mpqhb(req, &method, &method_size, &path, &path_size,
                            &query, &query_size, &headers_begin, &headers_end,
                            &body_begin, &body_end);

  lwlog_debug("method_size=%zu, path_size=%zu, query_size=%zu", method_size,
              path_size, query_size);
  lwlog_debug("method: '%.*s', path: '%.*s', query: '%.*s'", (int)method_size,
              method, (int)path_size, path, (int)query_size, query);
  lwlog_debug("headers: size=%zu, \n%.*s", (headers_end - headers_begin),
              (int)(headers_end - headers_begin), headers_begin);
  lwlog_debug("body: size=%zu %.*s\n", (body_end - body_begin),
              (int)((body_end - body_begin) & 0x7Full), body_begin);

  for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
    const size_t route_path_size = std::strlen(routes[i].path);
    if (path_size == route_path_size &&
        std::memcmp(path, routes[i].path, path_size) == 0) {
      routes[i].handler(Context{req, res, method, method_size, path, path_size,
                                query, query_size, headers_begin, headers_end,
                                body_begin, body_end});
      return;
    }
  }

  if (noroi_res_bufalloc(res, 512)) {
    lwlog_err("Failed to allocate response buffer");
    return;
  }
  noroi_res_not_found(res);
}

void noroi_handle(const struct noroi_req_t* req, struct noroi_res_t* res) {
  lwlog_info("Handling request");
  route_map_dispatcher(req, res);
  lwlog_info("Response sent");
}

int main(void) {
  if (!CreateDatabase())
    return EXIT_FAILURE;
  lwlog_info("Running on http://127.0.0.1:%d", noroi_conf_port);
  return noroi_run();
}

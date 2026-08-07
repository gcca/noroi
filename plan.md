# Plan: ticketeer templates → Mustache, then noroi core

## Goals

1. **Now (ticketeer):** Replace Drogon CSP templates with **Mustache** (same stack style as peachfuzz / bagend: [kainjow/mustache](https://github.com/kainjow/Mustache) header).
2. **Next (noroi):** Fix critical runtime characteristics so a later HTTP migration is possible.
3. **DB:** Prefer **sqlite3 C API** directly (already true — see finding below).

Out of scope for this plan: full Drogon → noroi port of handlers/routing.

---

## Finding: database layer (no migrate task)

**ticketeer already uses `sqlite3` directly**, not Drogon ORM.

- `#include <sqlite3.h>`, `sqlite3_open` / `prepare` / `bind` / `step` everywhere (auth, dashboard, requester, supervisor, …).
- CMake links `sqlite3`; no `drogon::orm`, `DbClient`, or mappers in app code.
- Drogon is used for HTTP, CSP views, cookies, multipart, static files, `HttpClient` — not for SQL.

**Action:** none for ORM. Keep sqlite3 C API as-is through the Mustache and noroi work.

---

## Reference: peachfuzz / bagend Mustache approach

| Piece | peachfuzz / bagend | ticketeer (C++23) |
|-------|--------------------|-------------------|
| Engine | `3rdparty/mustache.hpp` (kainjow) | Same header (vendor or copy) |
| Binding | C shim `mustacheshim.{hpp,cpp}` for Zig | Call `kainjow::mustache` **directly** from C++ (shim optional) |
| Data | `data_setstring` / `setbool` / nested `data` | `kainjow::mustache::data` map/list from handlers |
| Render | stream chunks via callback | `mustache.render(data)` → `std::string` → `HttpResponse` body |
| Templates | app-owned strings / files | `.mustache` files next to (or instead of) `.csp` |

CSP today embeds C++ (`<%c++ … %>`, `HttpViewData`, `htmlTranslate`). Mustache needs **logic in C++**, **markup in templates** (`{{escaped}}`, `{{#section}}`, partials if useful).

---

## Phase 1 — ticketeer: CSP → Mustache

Stay on **Drogon** for HTTP. Only change the view layer.

### Tasks

- [ ] **T1.1** Vendor kainjow Mustache  
  - Add `3rdparty/mustache.hpp` (same as peachfuzz/bagend).  
  - Wire CMake (include path; no special codegen like `drogon_create_views`).

- [ ] **T1.2** Small render helper  
  - e.g. `ticketeer::view::Render(name_or_path, data) → std::string`  
  - Load `.mustache` from disk (or embed later).  
  - Build `HttpResponse` with `text/html; charset=utf-8` (replace `newHttpViewResponse`).

- [ ] **T1.3** Convert templates (inventory)  
  Replace each `.csp` with `.mustache` and move C++ logic into the calling handler/common helpers.

  | Area | Templates |
  |------|-----------|
  | auth | `signin`, `app_signin` |
  | requester | `home`, `ticket_list`, `ticket_create`, `ticket_details`, `ticket_activity_message` |
  | supervisor | `home`, `ticket_list`, `ticket_create`, `ticket_details`, activity list/message/attachment, assigned_to select, due_date input, priority/status badges, config settings/status/priority |
  | technician | `home`, `ticket_list`, `ticket_create`, `ticket_details`, `ticket_activity_message` |

  Suggested order: **auth → one home → list → details/fragments → config**.

- [ ] **T1.4** Handler data plumbing  
  - Stop `drogon::HttpViewData` / `newHttpViewResponse`.  
  - Build `kainjow::mustache::data` (strings, bools, lists of maps).  
  - Move URL-encode / sort-link helpers out of templates into C++ (as in supervisor ticket list CSP).

- [ ] **T1.5** Drop CSP pipeline  
  - Remove `drogon_create_views(...)` from `CMakeLists.txt`.  
  - Delete `.csp` files once parity is checked.  
  - Document: no more re-cmake on template add (unless you choose embed-at-build).

- [ ] **T1.6** Smoke check  
  - Sign-in, role homes, ticket list/filters, create, details, htmx fragments, config pages.  
  - Escape check: user-controlled fields must use `{{…}}` not `{{{…}}}` unless intentional HTML.

**Done when:** app builds without CSP/codegen; all former views are Mustache; manual smoke of main flows OK.

---

## Phase 2 — noroi: critical runtime characteristics

Do this **after** Phase 1 (or in parallel if you want, but Phase 1 does not depend on noroi).

Ordered by dependency:

### Critical (gate for any real app)

- [ ] **N1 — Full HTTP request assembly**  
  - Read until headers complete (`\r\n\r\n`).  
  - Parse `Content-Length` (reject or ignore chunked for v1).  
  - Buffer body up to a max size; then call `noroi_handle`.  
  - *Today: one `uv_read`, then stop — multi-packet POST broken.*

- [ ] **N2 — Response buffer ownership**  
  - Clear lifetime: heap buffer freed in `on_write` (or server-owned builder on `client_t`).  
  - Document that stack/`static`-only buffers are not the API contract.  
  - *Today: demo relies on `static char xbuf[]`; stack is use-after-return.*

- [ ] **N3 — Request buffer lifetime**  
  - Assembled message owned for the whole `noroi_handle` call.  
  - Parse helpers may return views into that buffer only within handle.  
  - *Today: free request buf around write start; easy to get wrong with zero-copy.*

### Important helpers (minimal surface)

- [ ] **N4 — Response builders**  
  - Status code, `Content-Type`, body, redirect (keep), optional extra headers / `Set-Cookie`.  
  - HTML helper (not hard-coded `text/plain` only).

- [ ] **N5 — Request helpers**  
  - Method / path / query (exist).  
  - Get header, get cookie, query key, form field (`application/x-www-form-urlencoded`).

### Later (phase 2b — after N1–N5)

- [ ] **N6** Method-aware path routing with `{params}` (library or leave to app).  
- [ ] **N7** Multipart parse + file response (attachments).  
- [ ] **N8** Optional keep-alive / HTTP/1.1 (not required for first ticketeer spike).

**Done when:** can POST a form larger than one read chunk, set a cookie, return HTML from a heap buffer without UAF; tests cover assembly + ownership.

---

## Phase 3 — later (not started now)

- [ ] ticketeer vertical slice on noroi (healthcheck → sign-in → cookie → one Mustache page).  
- [ ] Router + middleware as plain C++ (no Drogon controllers).  
- [ ] Replace Drogon multipart / file / static / OAuth client as needed.  
- [ ] Remove Drogon from CMake/Docker.

---

## Dependency graph

```text
[sqlite3] ── already OK ─────────────────────────────► (no task)

Phase 1 (ticketeer Mustache)
  T1.1 vendor ──► T1.2 helper ──► T1.3/T1.4 convert ──► T1.5 drop CSP ──► T1.6 smoke

Phase 2 (noroi core)          [can start after or parallel to Phase 1]
  N1 assembly ──► N2 ownership ──► N3 req lifetime
       └──► N4 res helpers / N5 req helpers
              └──► N6..N8 later

Phase 3 migration slice ── needs Phase 1 (Mustache) + Phase 2 (N1–N5)
```

---

## Working agreement

| Project | Now | Later |
|---------|-----|--------|
| **ticketeer** | Mustache views; keep Drogon HTTP + sqlite3 | noroi HTTP when N1–N5 ready |
| **noroi** | Improve critical runtime (N1–N5) | Optional router/multipart |
| **DB** | sqlite3 C API only — **no ORM migration** | same |

---

## Immediate next step

Start **T1.1 + T1.2** in ticketeer (vendor Mustache + render helper), then convert **`signin`** as the first template end-to-end.

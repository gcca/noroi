import os

from locust import FastHttpUser, task, constant_throughput, events

HOST = os.getenv("NOROI_HOST", "http://127.0.0.1:8000")
USER_RPS = float(os.getenv("NOROI_USER_RPS", "4"))
MIN_USERS = int(os.getenv("NOROI_MIN_USERS", "700"))
MIN_RPS = float(os.getenv("NOROI_MIN_RPS", "1000"))
MAX_FAIL_RATIO = float(os.getenv("NOROI_MAX_FAIL_RATIO", "0.0"))

_peak_users = 0


class NoroiUser(FastHttpUser):
    host = HOST
    wait_time = constant_throughput(USER_RPS)

    @task(4)
    def misc(self):
        with self.client.get("/misc", catch_response=True) as res:
            if res.status_code == 418:
                res.success()
            else:
                res.failure(f"Expected 418, got {res.status_code}")

    @task(3)
    def healthcheck(self):
        self.client.get("/healthcheck")

    @task(2)
    def welcome(self):
        self.client.get("/welcome")

    @task(2)
    def dashboard(self):
        with self.client.get("/dashboard", catch_response=True) as r:
            if r.status_code != 200:
                r.failure(f"expected HTTP 200, got {r.status_code}")
                return
            content_type = r.headers.get("Content-Type")
            if content_type != "application/json; charset=utf-8":
                r.failure(f"expected a JSON content type, got {content_type!r}")
                return
            try:
                payload = r.json()
            except Exception as error:
                r.failure(f"expected a JSON body, got {error}")
                return
            totals = payload.get("totals") or {}
            users = payload.get("users") or []
            apps = payload.get("apps") or []
            bindings = sum(user.get("app_count", 0) for user in users)
            if not totals.get("users"):
                r.failure(f"expected a seeded database, got totals={totals!r}")
            elif len(users) != totals.get("users"):
                r.failure(f"expected {totals.get('users')} users, got {len(users)}")
            elif len(apps) != totals.get("apps"):
                r.failure(f"expected {totals.get('apps')} apps, got {len(apps)}")
            elif bindings != totals.get("bindings"):
                r.failure(f"expected {totals.get('bindings')} bindings, got {bindings}")
            else:
                r.success()

    @task(1)
    def index_redirect(self):
        with self.client.get("/", allow_redirects=False, catch_response=True) as r:
            if r.status_code != 302:
                r.failure(f"expected HTTP 302, got {r.status_code}")
            elif r.headers.get("Location") != "/welcome":
                r.failure(f"expected Location: /welcome, got {r.headers.get('Location')!r}")
            else:
                r.success()


@events.spawning_complete.add_listener
def _record_peak_users(user_count, **_kwargs):
    global _peak_users
    _peak_users = max(_peak_users, user_count)


@events.quitting.add_listener
def _assert_slo(environment, **_kwargs):
    stats = environment.runner.stats.total
    rps = stats.total_rps
    fail_ratio = stats.fail_ratio
    task_exceptions = sum(error["count"] for error in environment.runner.exceptions.values())
    enough_users = _peak_users >= MIN_USERS
    ok = (
        enough_users
        and rps >= MIN_RPS
        and fail_ratio <= MAX_FAIL_RATIO
        and task_exceptions == 0
    )
    verdict = "PASS" if ok else "FAIL"
    print(
        f"\n[SLO CHECK] {verdict}: users={_peak_users} (min {MIN_USERS}), "
        f"average={rps:.1f} req/s (min {MIN_RPS:.0f}), "
        f"fail_ratio {fail_ratio * 100:.2f}% (max {MAX_FAIL_RATIO * 100:.2f}%), "
        f"reqs={stats.num_requests}, fails={stats.num_failures}, "
        f"task_exceptions={task_exceptions}, "
        f"p95={stats.get_response_time_percentile(0.95)}ms"
    )
    for entry in sorted(environment.runner.stats.entries.values(), key=lambda e: e.name):
        print(
            f"[SLO CHECK]   {entry.method} {entry.name}: "
            f"reqs={entry.num_requests}, fails={entry.num_failures}, "
            f"p95={entry.get_response_time_percentile(0.95)}ms"
        )
    if not ok:
        environment.process_exit_code = 1

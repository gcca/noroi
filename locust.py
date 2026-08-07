import os

from locust import FastHttpUser, task, constant_throughput, events

HOST = os.getenv("NOROI_HOST", "http://127.0.0.1:8000")
USER_RPS = float(os.getenv("NOROI_USER_RPS", "5"))
MIN_RPS = float(os.getenv("NOROI_MIN_RPS", "100"))
MAX_FAIL_RATIO = float(os.getenv("NOROI_MAX_FAIL_RATIO", "0.0"))

class NoroiUser(FastHttpUser):
    host = HOST
    wait_time = constant_throughput(USER_RPS)

    @task(3)
    def healthcheck(self):
        self.client.get("/healthcheck")

    @task(2)
    def welcome(self):
        self.client.get("/welcome")

    @task(1)
    def index_redirect(self):
        with self.client.get("/", allow_redirects=False, catch_response=True) as r:
            if 300 <= r.status_code < 400:
                r.success()


@events.quitting.add_listener
def _assert_slo(environment, **_kwargs):
    stats = environment.runner.stats.total
    rps = stats.total_rps
    fail_ratio = stats.fail_ratio
    ok = rps >= MIN_RPS and fail_ratio <= MAX_FAIL_RATIO
    verdict = "PASS" if ok else "FAIL"
    print(
        f"\n[SLO CHECK] {verdict}: sustained {rps:.1f} req/s (bar {MIN_RPS:.0f}), "
        f"fail_ratio {fail_ratio * 100:.2f}% (max {MAX_FAIL_RATIO * 100:.2f}%), "
        f"reqs={stats.num_requests}, fails={stats.num_failures}, "
        f"p95={stats.get_response_time_percentile(0.95)}ms"
    )
    environment.process_exit_code = 0 if ok else 1

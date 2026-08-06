#!/usr/bin/env fish

set -l dir (dirname (status --current-filename))
cd $dir; or exit 1

set -q NOROI_HOST; or set -gx NOROI_HOST http://127.0.0.1:8000
set -q NOROI_USERS; or set -gx NOROI_USERS 800
set -q NOROI_MIN_USERS; or set -gx NOROI_MIN_USERS 700
set -q NOROI_MIN_RPS; or set -gx NOROI_MIN_RPS 1000
set -q NOROI_USER_RPS; or set -gx NOROI_USER_RPS 4
set -q NOROI_SPAWN_RATE; or set -gx NOROI_SPAWN_RATE 30
set -q NOROI_RUNTIME; or set -gx NOROI_RUNTIME 240s
set -l CSV build/noroi-bench
set -l OFFERED_RPS (math "$NOROI_USERS * $NOROI_USER_RPS")

if test $NOROI_USERS -lt $NOROI_MIN_USERS
    echo "!! configured users ($NOROI_USERS) are below the minimum ($NOROI_MIN_USERS)"
    exit 1
end
if test $OFFERED_RPS -lt $NOROI_MIN_RPS
    echo "!! offered load (~$OFFERED_RPS req/s) is below the minimum ($NOROI_MIN_RPS req/s)"
    exit 1
end

if not curl -sf --max-time 2 -o /dev/null "$NOROI_HOST/healthcheck"
    echo "!! $NOROI_HOST is not serving traffic — start the server first (./build/dist/run)"
    exit 1
end
if not curl -sf --max-time 5 -o /dev/null "$NOROI_HOST/dashboard"
    echo "!! $NOROI_HOST/dashboard is not answering 200 — start the server with a readable NOROI_SAMPLE_DATA"
    exit 1
end

echo "==> Endpoints: /healthcheck, /welcome, /dashboard (JSON), / (302 -> /welcome), /misc (418)"
echo "==> Load: $NOROI_USERS users, ~$OFFERED_RPS req/s offered for $NOROI_RUNTIME (minimum = $NOROI_MIN_USERS users and $NOROI_MIN_RPS req/s)"
locust -f locust.py --headless \
    -u $NOROI_USERS -r $NOROI_SPAWN_RATE -t $NOROI_RUNTIME \
    --host $NOROI_HOST \
    --reset-stats \
    --only-summary \
    --csv $CSV --html $CSV.html
set -l rc $status

echo
echo "==> Reports: "$CSV"_stats.csv  |  $CSV.html"
if test $rc -eq 0
    echo "==> RESULT: PASS — reached >= $NOROI_MIN_USERS users, averaged >= $NOROI_MIN_RPS req/s, and stayed within the failure limit"
else
    echo "==> RESULT: FAIL — see the [SLO CHECK] line above"
end
exit $rc

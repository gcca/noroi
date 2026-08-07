#!/usr/bin/env fish

set -l dir (dirname (status --current-filename))
cd $dir; or exit 1

set -q NOROI_HOST; or set -gx NOROI_HOST http://127.0.0.1:8000
set -q NOROI_MIN_RPS; or set -gx NOROI_MIN_RPS 100
set -q NOROI_USER_RPS; or set -gx NOROI_USER_RPS 5
set -l USERS 30
set -l RUNTIME 20s
set -l LOG build/noroi-bench.log
set -l CSV build/noroi-bench

echo "==> Building optimized server (ninja dist)"
ninja dist; or exit 1

echo "==> Starting server (stderr -> $LOG, per-request logging kept out of the report)"
./build/dist/run 2>$LOG &
set -g srv $last_pid
function _cleanup --on-signal INT --on-signal TERM
    kill $srv 2>/dev/null
end

set -l ready 0
for i in (seq 50)
    if curl -sf -o /dev/null $NOROI_HOST/healthcheck
        set ready 1
        break
    end
    sleep 0.1
end
if test $ready -eq 0
    echo "!! server never became ready — check $LOG"
    _cleanup
    exit 1
end

echo "==> Load: $USERS users, ~"(math "$USERS * $NOROI_USER_RPS")" req/s offered for $RUNTIME (bar = $NOROI_MIN_RPS req/s)"
locust -f locust.py --headless \
    -u $USERS -r $USERS -t $RUNTIME \
    --host $NOROI_HOST \
    --only-summary \
    --csv $CSV --html $CSV.html
set -l rc $status

_cleanup

echo
echo "==> Reports: "$CSV"_stats.csv  |  $CSV.html  |  server log: $LOG"
if test $rc -eq 0
    echo "==> RESULT: PASS — sustained >= $NOROI_MIN_RPS req/s with no failures"
else
    echo "==> RESULT: FAIL — see the [SLO CHECK] line above"
end
exit $rc

#! /usr/bin/env fish

http POST :8000/welcome
for x in /healthcheck /welcome /dashboard /not-found /misc / '/get?v1=abc&v2=123&v1=def'
    http :8000$x
end
http -f :8000/post v1=abc v2=123 v1=def

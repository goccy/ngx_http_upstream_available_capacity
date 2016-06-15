use Test::Nginx::Socket;

plan tests => repeat_each() * 4 * blocks();

run_tests();

__DATA__

=== TEST 1: default
--- http_config
    upstream backends {
        server 127.0.0.1:8001;
        server 127.0.0.1:8002;
        available_capacity;
    }
    server {
        listen 8001;
        location / {
            return 200 "8001";
        }
    }
    server {
       listen 8002;
       location / {
           return 200 "8002";
       }
    }
--- config
    location /available_capacity {
        proxy_pass http://backends;
    }
--- request eval
["GET /available_capacity", "GET /available_capacity"]

--- response_body eval
["8001", "8002"]

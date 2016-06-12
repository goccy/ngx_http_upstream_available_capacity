# ngx_http_upstream_available_capacity

## Status

WIP

### Background

#### 1. Architecture
- We assume architecture is nginx has multiple upstreams and each upsteam has many workers 

<img src="https://cloud.githubusercontent.com/assets/209884/15988209/17db95f4-3083-11e6-94c1-68316c60ce37.png" with="200px" height="200px"/>

#### 2. Request passing workflow

- new request comes nginx and nginx proxy request to a.example.com
- a.example.com's worker is processing request

<img src="https://cloud.githubusercontent.com/assets/209884/15988268/b46eddbc-3084-11e6-8fe1-f8547a9107c5.png" with="200px" height="200px"/>

- worker's processing is too long (ex. websockets or gaming session and so on)
- worker cannot process new request because it is occupied

<img src="https://cloud.githubusercontent.com/assets/209884/15988292/8c5e5158-3085-11e6-8fb4-dc1f7aec8dc6.png" with="200px" height="200px"/>

- Similary, other request comes nginx and proxy request to a.example.com
- a.example.com's server assign other worker for processing new request

<img src="https://cloud.githubusercontent.com/assets/209884/15988308/35dc8c04-3086-11e6-8481-78bda6fa6533.png" with="200px" height="200px"/>

- other request comes nginx and proxy request to a.example.com again
- a.example.com's server assign last worker for processing new request
- a.example.com's worker becomes full

<img src="https://cloud.githubusercontent.com/assets/209884/15988320/a48498e0-3086-11e6-9609-00f735a0525f.png" with="200px" height="200px"/>

- new request comes nginx and proxy request to a.example.com
- Howerver, a.example.com has no worker for processing request

<img src="https://cloud.githubusercontent.com/assets/209884/15988339/55080102-3087-11e6-8775-623b385cd4a4.png" with="200px" height="200px"/>

- If nginx knows a.example.com's left worker number, proxy to b.example.com

<img src="https://cloud.githubusercontent.com/assets/209884/15988341/5da5b4ee-3087-11e6-8471-4faebe627b9f.png" with="200px" height="200px"/>

### Approach

- teach available worker number to nginx and nginx proxy to upstream based it

## Directives

### available_capacity

| Syntax        | available_capacity |
| ------------- |:-------------:|
| Default      | - |
| Context      | upstream  |

## QuickStart

- 1. run two http servers ( http://127.0.0.1:8001 and http://127.0.0.1:8002 )
- 2. write the following code to nginx.conf

```
upstream backends {
    available_capacity;
}

server {
    listen 8000;

    location / {
        proxy_pass http://backends;
    }
}
```

- 3. add server address and workers to redis by CLI tool
```
$ script/nginx-upstream-list add --server 127.0.0.1:8001 --worker 3
$ script/nginx-upstream-list add --server 127.0.0.1:8002 --worker 3
```

- 4. run nginx
```
$ nginx -c /path/to/nginx.conf 
```

- 5. test request
```
$ curl http://127.0.0.1:8000 # response from 127.0.0.1:8001
```

- 6. remove workers from 127.0.0.1:8001
```
$ script/nginx-upstream-list update --server 127.0.0.1:8001 --worker 0
```

- 7. test request
```
$ curl http://127.0.0.1:8000 # response from 127.0.0.1:8002
```

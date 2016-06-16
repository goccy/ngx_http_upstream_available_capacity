#ifndef NGX_HTTP_UPSTREAM_AVAILABLE_CAPACITY_H
#define NGX_HTTP_UPSTREAM_AVAILABLE_CAPACITY_H

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_http_upstream_server_t *server;
    ngx_addr_t addr;
    int capacity;
} ngx_http_upstream_available_capacity_server_t;

typedef struct {
    ngx_http_upstream_srv_conf_t *conf;
} ngx_http_upstream_available_capacity_data_t;

typedef struct {
    ngx_str_t host;
    ngx_array_t *servers;
} ngx_http_upstream_available_capacity_srv_conf_t;

extern ngx_module_t ngx_http_upstream_available_capacity_module;
extern ngx_http_upstream_available_capacity_srv_conf_t *available_capacity_server_conf;

#endif

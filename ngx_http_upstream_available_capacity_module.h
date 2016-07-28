#ifndef NGX_HTTP_UPSTREAM_AVAILABLE_CAPACITY_H
#define NGX_HTTP_UPSTREAM_AVAILABLE_CAPACITY_H

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "picoredis.h"

// customized server configuration
typedef struct _ngx_http_upstream_available_capacity_server_t {
    ngx_http_upstream_server_t *server;
    ngx_addr_t addr;
    int capacity;
    struct _ngx_http_upstream_available_capacity_server_t *next;
} ngx_http_upstream_available_capacity_server_t;

// user defined data for sharing between requests
typedef struct {
    ngx_http_upstream_srv_conf_t *conf;
} ngx_http_upstream_available_capacity_data_t;

// main configuration for available_capacity
typedef struct {
    picoredis_t *redis_ctx;
    ngx_url_t redis_pass;
    size_t server_num;
    ngx_http_upstream_available_capacity_server_t *search_start_peer;
    ngx_http_upstream_available_capacity_server_t *server_list;
} ngx_http_upstream_available_capacity_srv_conf_t;

extern ngx_module_t ngx_http_upstream_available_capacity_module;
extern ngx_http_upstream_available_capacity_srv_conf_t *available_capacity_server_conf;

#endif

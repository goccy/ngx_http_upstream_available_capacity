/*
 * Copyright (C) 2016 Masaaki Goshima <goccy54@gmail.com>
 */

#include "ngx_http_upstream_available_capacity_module.h"

ngx_http_upstream_available_capacity_srv_conf_t *available_capacity_server_conf = NULL;

static char *ngx_http_upstream_available_capacity(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_upstream_available_capacity_create_conf(ngx_conf_t *cf);

static ngx_command_t ngx_http_upstream_available_capacity_commands[] = {
    { 
        ngx_string("available_capacity"),
        NGX_HTTP_UPS_CONF|NGX_CONF_NOARGS,
        ngx_http_upstream_available_capacity,
        0,
        0,
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_upstream_available_capacity_module_ctx = {
    NULL,                                             /* preconfiguration */
    NULL,                                             /* postconfiguration */
    
    NULL,                                             /* create main configuration */
    NULL,                                             /* init main configuration */

    ngx_http_upstream_available_capacity_create_conf, /* create server configuration */
    NULL,                                             /* merge server configuration */

    NULL,                                             /* create location configuration */
    NULL                                              /* merge location configuration */
};

ngx_module_t ngx_http_upstream_available_capacity_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_available_capacity_module_ctx, /* module context */
    ngx_http_upstream_available_capacity_commands, /* module directives */
    NGX_HTTP_MODULE,                  /* module type */
    NULL,                             /* init master */
    NULL,                             /* init module */
    NULL,                             /* init process */
    NULL,                             /* init thread */
    NULL,                             /* exit thread */
    NULL,                             /* exit process */
    NULL,                             /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_upstream_get_available_capacity_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_available_capacity_data_t *cap_data = data;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0, "get available capacity peer, try: %ui", pc->tries);
    pc->cached = 0;
    pc->connection = NULL;

    ngx_http_upstream_available_capacity_srv_conf_t *conf = ngx_http_conf_upstream_srv_conf(cap_data->conf, ngx_http_upstream_available_capacity_module);
    ngx_http_upstream_available_capacity_server_t *found_server = NULL;

    size_t i = 0;
    for (i = 0; i < conf->servers->nelts; ++i) {
        ngx_http_upstream_available_capacity_server_t *servers = conf->servers->elts;
        ngx_http_upstream_available_capacity_server_t *server  = &servers[i];
        if (server->capacity > 0) {
            found_server = server;
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0, "server_capacity = [%d]\n", server->capacity);
            --server->capacity;
            break;
        }
    }

    if (found_server) {
        pc->sockaddr = found_server->addr.sockaddr;
        pc->socklen  = found_server->addr.socklen;
        pc->name     = &found_server->server->name;
    } else {
        ngx_log_error_core(NGX_LOG_ERR, pc->log, 0, "cannot load balance");
        return NGX_ERROR;
    }

    return NGX_OK;
}

static ngx_int_t ngx_http_upstream_init_available_capacity_peer(ngx_http_request_t *r, ngx_http_upstream_srv_conf_t *us)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "init available capacity peer");
    ngx_http_upstream_available_capacity_data_t *data = ngx_palloc(r->pool, sizeof(ngx_http_upstream_available_capacity_data_t));
    data->conf = us;
    r->upstream->peer.data = data;
    r->upstream->peer.get  = ngx_http_upstream_get_available_capacity_peer;
    return NGX_OK;
}

static ngx_int_t ngx_http_upstream_init_available_capacity(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "init available_capacity");
    us->peer.init = ngx_http_upstream_init_available_capacity_peer;

    ngx_http_upstream_available_capacity_srv_conf_t *conf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_available_capacity_module);
    conf->servers = ngx_array_create(cf->pool, us->servers->nelts, sizeof(ngx_http_upstream_available_capacity_server_t));
	size_t i = 0;
    for (i = 0; i < us->servers->nelts; ++i) {
        ngx_http_upstream_server_t *servers = us->servers->elts;
        ngx_http_upstream_available_capacity_server_t *cap_server = ngx_array_push(conf->servers);
        cap_server->server   = &servers[i];
        cap_server->capacity = 1;

        ngx_str_t server_address = ngx_string(cap_server->server->name.data);
        ngx_addr_t addr;
        ngx_memzero(&addr, sizeof(ngx_addr_t));

        if (ngx_parse_addr_port(cf->pool, &addr, server_address.data, strlen((char *)server_address.data)) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "cannot parse url : %s", server_address.data);
            return NGX_DECLINED;
        }
        cap_server->addr = addr;
    }

    available_capacity_server_conf = conf;
    return NGX_OK;
}

static void *ngx_http_upstream_available_capacity_create_conf(ngx_conf_t *cf)
{
    ngx_http_upstream_available_capacity_srv_conf_t  *conf;
    conf = ngx_palloc(cf->pool, sizeof(ngx_http_upstream_available_capacity_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }
    return conf;
}
 
static char *ngx_http_upstream_available_capacity(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t *uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);
    if (uscf->peer.init_upstream) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "load balancing method redefined");
    }

    uscf->peer.init_upstream = ngx_http_upstream_init_available_capacity;
    return NGX_CONF_OK;
}

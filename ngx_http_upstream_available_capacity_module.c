/*
 * Copyright (C) 2016 Masaaki Goshima <goccy54@gmail.com>
 */

#include "ngx_http_upstream_available_capacity_module.h"
#include "ngx_inet_slab.h"

ngx_http_upstream_available_capacity_srv_conf_t *available_capacity_server_conf = NULL;

static char *ngx_http_upstream_available_capacity(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_upstream_available_capacity_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_upstream_available_capacity_create_conf(ngx_conf_t *cf);

static ngx_command_t ngx_http_upstream_available_capacity_commands[] = {
    {
        ngx_string("available_capacity"),
        NGX_HTTP_UPS_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
        ngx_http_upstream_available_capacity,
        NGX_HTTP_SRV_CONF_OFFSET,
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

#define NEXT_SERVER(conf, server) (server->next ? server->next : conf->server_list)

static ngx_int_t ngx_http_upstream_get_available_capacity_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_available_capacity_data_t *cap_data = data;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0, "get available capacity peer, try: %ui", pc->tries);
    pc->cached = 0;
    pc->connection = NULL;

    ngx_http_upstream_available_capacity_srv_conf_t *conf = ngx_http_conf_upstream_srv_conf(cap_data->conf, ngx_http_upstream_available_capacity_module);
    ngx_http_upstream_available_capacity_server_t *found_server = NULL;
    ngx_http_upstream_available_capacity_server_t *server = conf->search_start_peer;
    size_t i = 0;
    for (; i < conf->server_num; ++i) {
        if (i != 0) {
            server = NEXT_SERVER(conf, server);
        }
        if (server->capacity > 0) {
            found_server = server;
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0, "server_capacity = [%d]\n", server->capacity);
            --server->capacity;
            break;
        }
    }
    conf->search_start_peer = (server == conf->search_start_peer) ? NEXT_SERVER(conf, server) : server;

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

static ngx_int_t setup_default_servers(ngx_http_request_t *r, ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_available_capacity_srv_conf_t *conf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_available_capacity_module);
    if (conf->server_list) return NGX_OK;

    ngx_slab_pool_t *shpool = (ngx_slab_pool_t *)us->shm_zone->shm.addr;
    ngx_shmtx_lock(&shpool->mutex);

    ngx_http_upstream_available_capacity_server_t *prev_server_conf = NULL;
    size_t i = 0;
    for (i = 0; i < us->servers->nelts; ++i) {
        ngx_http_upstream_server_t *servers = us->servers->elts;

        ngx_http_upstream_available_capacity_server_t *server_conf = ngx_slab_calloc_locked(shpool, sizeof(ngx_http_upstream_available_capacity_server_t));
        server_conf->server   = &servers[i];
        server_conf->capacity = 1;

        ngx_str_t server_address = ngx_string(server_conf->server->name.data);
        ngx_url_t url;
        ngx_memzero(&url, sizeof(ngx_url_t));
        size_t server_address_size = strlen((char *)server_address.data);
        url.url.len  = server_address_size;
        url.url.data = ngx_slab_alloc_locked(shpool, server_address_size);
        url.default_port = 80;
        ngx_cpystrn(url.url.data, server_address.data, server_address_size + 1);

        if (ngx_parse_url_slab(shpool, &url) != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "cannot parse url : %s", server_address.data);
            return NGX_DECLINED;
        }
        server_conf->addr = *url.addrs;
        server_conf->next = NULL;

        if (prev_server_conf) {
            prev_server_conf->next = server_conf;
        } else {
            conf->server_list = server_conf;
        }
        prev_server_conf = server_conf;
    }

    conf->server_num        = us->servers->nelts;
    conf->search_start_peer = conf->server_list;

    ngx_shmtx_unlock(&shpool->mutex);
    return NGX_OK;
}

static ngx_int_t ngx_http_upstream_init_available_capacity_peer(ngx_http_request_t *r, ngx_http_upstream_srv_conf_t *us)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "init available capacity peer");
    ngx_http_upstream_available_capacity_data_t *data = ngx_palloc(r->pool, sizeof(ngx_http_upstream_available_capacity_data_t));
    data->conf = us;
    r->upstream->peer.data = data;
    r->upstream->peer.get  = ngx_http_upstream_get_available_capacity_peer;
    return setup_default_servers(r, us);
}

static ngx_int_t ngx_http_upstream_init_available_capacity(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "init available_capacity");

    // initialize us->peer.data
    if (ngx_http_upstream_init_round_robin(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    us->peer.init = ngx_http_upstream_init_available_capacity_peer;
    ngx_http_upstream_available_capacity_srv_conf_t *conf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_available_capacity_module);
    available_capacity_server_conf = conf;
    return NGX_OK;
}

static void *ngx_http_upstream_available_capacity_create_conf(ngx_conf_t *cf)
{
    return ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_available_capacity_srv_conf_t));
}

static ngx_int_t ngx_http_upstream_available_capacity_support_redis(ngx_conf_t *cf, ngx_str_t *redis_pass, ngx_http_upstream_available_capacity_srv_conf_t *conf)
{
    ngx_url_t url;
    ngx_memzero(&url, sizeof(ngx_url_t));
    url.url = *redis_pass;
    url.default_port = 80;
    if (ngx_parse_url(cf->pool, &url) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "cannot parse url : %V", &url.url);
        return NGX_ERROR;
    }
    conf->redis_pass = url;
    conf->redis_ctx  = picoredis_connect_with_address((char *)url.host.data);
    return NGX_OK;
}

static char *ngx_http_upstream_available_capacity(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t *uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);
    if (uscf->peer.init_upstream) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "load balancing method redefined");
    }
    if (cf->args->nelts == 2) {
        ngx_str_t *args = cf->args->elts;
        if (ngx_http_upstream_available_capacity_support_redis(cf, &args[1], conf) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    uscf->peer.init_upstream = ngx_http_upstream_init_available_capacity;
    return NGX_CONF_OK;
}

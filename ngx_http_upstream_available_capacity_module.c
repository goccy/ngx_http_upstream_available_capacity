/*
 * Copyright (C) 2016 Masaaki Goshima <goccy54@gmail.com>
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static char *ngx_http_upstream_available_capacity(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

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
    NULL,                              /* preconfiguration */
    NULL,                              /* postconfiguration */
    
    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    NULL,                              /* create location configuration */
    NULL                               /* merge location configuration */
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
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0, "get available capacity peer, try: %ui", pc->tries);
    pc->cached = 0;
    pc->connection = NULL;

	ngx_url_t u;
    ngx_memzero(&u, sizeof(ngx_url_t));
	ngx_str_t url = ngx_string("127.0.0.1");
    u.url = url;
    u.default_port = 8002;

	ngx_pool_t *pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, pc->log);

    if (ngx_parse_url(pool, &u) != NGX_OK) {
        if (u.err) {
			fprintf(stderr, "error : %s\n", u.err);
        }
		ngx_destroy_pool(pool);
        return NGX_ERROR;
    }

    pc->sockaddr = u.addrs->sockaddr;
    pc->socklen = u.addrs->socklen;
    pc->name = &u.url;

	ngx_destroy_pool(pool);
	
    return NGX_OK;
}

static ngx_int_t ngx_http_upstream_init_available_capacity_peer(ngx_http_request_t *r, ngx_http_upstream_srv_conf_t *us)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "init available capacity peer");
    r->upstream->peer.get = ngx_http_upstream_get_available_capacity_peer;
    return NGX_OK;
}

static ngx_int_t ngx_http_upstream_init_available_capacity(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "init available_capacity");
    us->peer.init = ngx_http_upstream_init_available_capacity_peer;
    return NGX_OK;
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

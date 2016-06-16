#include "ngx_http_upstream_available_capacity_module.h"
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static char *ngx_http_upstream_capacity_configure(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t ngx_http_upstream_capacity_configure_commands[] = {
    {
        ngx_string("upstream_capacity_configure"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_upstream_capacity_configure,
        0,
        0,
        NULL
    },

    ngx_null_command
};

static ngx_http_module_t ngx_http_upstream_capacity_configure_module_ctx = {
    NULL,                              /* preconfiguration */
    NULL,                              /* postconfiguration */

    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    NULL,                              /* create location configuration */
    NULL                               /* merge location configuration */
};


ngx_module_t ngx_http_upstream_capacity_configure_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_capacity_configure_module_ctx, /* module context */
    ngx_http_upstream_capacity_configure_commands,    /* module directives */
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

typedef struct {
    ngx_str_t arg_name;
    void (*callabck)(ngx_http_request_t *r);
} ngx_http_upstream_capacity_configure_data_t;

static ngx_http_upstream_capacity_configure_data_t ngx_http_upstream_capacity_configure_table[] = {
    { ngx_string("arg_upstream")
}

static ngx_int_t ngx_http_upstream_capacity_configure_handler(ngx_http_request_t *r)
{
    fprintf(stderr, "called capacity_configure_handler\n");
    ngx_int_t                       rc;
    ngx_chain_t                     out;
    ngx_buf_t                      *b;

    ngx_http_upstream_available_capacity_srv_conf_t *conf = available_capacity_server_conf;

    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }
    
    r->headers_out.content_type_len = sizeof("text/plain") - 1;
    ngx_str_set(&r->headers_out.content_type, "text/plain");
    r->headers_out.content_type_lowcase = NULL;

    if (r->method == NGX_HTTP_HEAD) {
        r->headers_out.status = NGX_HTTP_OK;

        rc = ngx_http_send_header(r);

        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
        }
    }
    
/*
    rc = ngx_dynamic_upstream_build_op(r, &op);
    if (rc != NGX_OK) {
        if (op.status == NGX_HTTP_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        return op.status;
    }
*/  

    const char *dummy_response = "dummy response text";
    b = ngx_create_temp_buf(r->pool, strlen(dummy_response));
    b->pos = (u_char *)dummy_response;
    b->last = (u_char *)dummy_response + strlen(dummy_response);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = b->last - b->pos;

    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);

}

static char *ngx_http_upstream_capacity_configure(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_upstream_capacity_configure_handler;

    return NGX_CONF_OK;
}

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

typedef enum {
    METHOD_NONE,
    METHOD_INIT,
    METHOD_INC
} METHOD_TYPE;

typedef struct {
    METHOD_TYPE method_type;
    ngx_str_t server;
    int capacity;
    char response_buf[128];
} ngx_http_upstream_capacity_configure_param_t;

typedef struct {
    ngx_str_t arg_name;
    ngx_int_t (*callback)(ngx_http_request_t *r, ngx_http_variable_value_t *value, ngx_http_upstream_capacity_configure_param_t *param);
} ngx_http_upstream_capacity_configure_data_t;

// ex) http://127.0.0.1/upstream?init=192.168.1.1:8080&cap=10
static ngx_int_t init_server_capacity_handler(ngx_http_request_t *r, ngx_http_variable_value_t *value, ngx_http_upstream_capacity_configure_param_t *param)
{
    if (param->method_type != METHOD_NONE) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "multiple method are not allowed. %s:%d", __FUNCTION__, __LINE__);
        return NGX_ERROR;
    }
    param->method_type = METHOD_INIT;
    param->server.data = value->data;
    param->server.len  = value->len;
    return NGX_OK;
}

// ex) http://127.0.0.1/upstream?inc=192.168.1.1:8080
static ngx_int_t increment_server_capacity_handler(ngx_http_request_t *r, ngx_http_variable_value_t *value, ngx_http_upstream_capacity_configure_param_t *param)
{
    if (param->method_type != METHOD_NONE) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "multiple method are not allowed. %s:%d", __FUNCTION__, __LINE__);
        return NGX_ERROR;
    }
    param->method_type = METHOD_INC;
    param->server.data = value->data;
    param->server.len  = value->len;
    return NGX_OK;
}

// ex) http://127.0.0.1/upstream?init=192.168.1.1:8080&cap=10
static ngx_int_t set_capacity_handler(ngx_http_request_t *r, ngx_http_variable_value_t *value, ngx_http_upstream_capacity_configure_param_t *param)
{
    if (param->capacity != 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "capacity already defined %s:%d", __FUNCTION__, __LINE__);
        return NGX_ERROR;
    }
    param->capacity = ngx_atoi(value->data, value->len);
    return NGX_OK;
}

static ngx_http_upstream_capacity_configure_data_t ngx_http_upstream_capacity_configure_table[] = {
    { ngx_string("arg_init"), init_server_capacity_handler   },
    { ngx_string("arg_inc"),  increment_server_capacity_handler },

    { ngx_string("arg_cap"),  set_capacity_handler },
};

static ngx_http_upstream_available_capacity_server_t *find_server_from_config(ngx_http_request_t *r, ngx_str_t *target_server)
{
    ngx_http_upstream_available_capacity_server_t *ret = NULL;
    
    ngx_addr_t addr;
    ngx_memzero(&addr, sizeof(ngx_addr_t));
    
    if (ngx_parse_addr_port(r->pool, &addr, target_server->data, target_server->len) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "cannot parse url : %s", target_server->data);
        return NULL;
    }
    
    ngx_http_upstream_available_capacity_srv_conf_t *conf = available_capacity_server_conf;
    size_t i = 0;
    for (i = 0; i < conf->servers->nelts; ++i) {
        ngx_http_upstream_available_capacity_server_t *servers = conf->servers->elts;
        ngx_http_upstream_available_capacity_server_t *server  = &servers[i];
        if (ngx_strncmp(server->server->name.data, target_server->data, server->server->name.len) == 0) {
            ret = server;
            break;
        }
    }
    
    return ret;
}

static ngx_buf_t *create_response_buf(ngx_http_request_t *r, const char *response_text)
{
    size_t text_size = strlen(response_text) + 1;
    ngx_buf_t *buf   = ngx_create_temp_buf(r->pool, text_size);
    if (!buf) {
        return NULL;
    }

    ngx_cpystrn(buf->pos, (u_char *)response_text, text_size);
    buf->last = buf->pos + text_size;
    
    buf->last_buf = (r == r->main) ? 1 : 0;
    buf->last_in_chain = 1;
    r->headers_out.content_length_n = text_size;

    return buf;
}

static ngx_int_t exec_init_capacity(ngx_http_request_t *r, ngx_http_upstream_capacity_configure_param_t *param)
{
    ngx_http_upstream_available_capacity_server_t *server = find_server_from_config(r, &param->server);
    if (server) {
        server->capacity = param->capacity;
        ngx_snprintf((u_char *)param->response_buf, sizeof(param->response_buf), "init %s capacity to %d\n", server->server->name.data, server->capacity);
        return NGX_OK;
    } else {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "cannot find server from config");
        return NGX_ERROR;
    }
}

static ngx_int_t exec_increment_capacity(ngx_http_request_t *r, ngx_http_upstream_capacity_configure_param_t *param)
{
    ngx_http_upstream_available_capacity_server_t *server = find_server_from_config(r, &param->server);
    if (server) {
        server->capacity++;
        ngx_snprintf((u_char *)param->response_buf, sizeof(param->response_buf), "inc %s capacity to %d\n", server->server->name.data, server->capacity);
        return NGX_OK;
    } else {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "cannot find server from config");
        return NGX_ERROR;
    }
}

static ngx_int_t exec_configuration(ngx_http_request_t *r, ngx_http_upstream_capacity_configure_param_t *param)
{
    ngx_int_t rc = NGX_NONE;
    switch (param->method_type) {
    case METHOD_INIT:
        rc = exec_init_capacity(r, param);
        break;
    case METHOD_INC:
        rc = exec_increment_capacity(r, param);
        break;
    default:
        break;
    }
    return rc;
}

static ngx_int_t parse_nginx_query_string(ngx_http_request_t *r, ngx_http_upstream_capacity_configure_param_t *param)
{
    size_t table_size = sizeof(ngx_http_upstream_capacity_configure_table) / sizeof(ngx_http_upstream_capacity_configure_data_t);
    size_t i = 0;
    for (i = 0; i < table_size; ++i) {
        ngx_http_upstream_capacity_configure_data_t data = ngx_http_upstream_capacity_configure_table[i];
        ngx_str_t arg_name = data.arg_name;
        u_char *base = ngx_pnalloc(r->pool, arg_name.len);
        if (base == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "failed to allocate memory from r->pool %s:%d", __FUNCTION__, __LINE__);
            return NGX_ERROR;
        }
        ngx_uint_t key = ngx_hash_strlow(base, arg_name.data, arg_name.len);
        ngx_http_variable_value_t *value = ngx_http_get_variable(r, &arg_name, key);
        if (!value->not_found) {
            ngx_int_t rc = data.callback(r, value, param);
            if (rc != NGX_OK) {
                return NGX_ERROR;
            }
        }
    }
    return NGX_OK;
}

static ngx_int_t ngx_http_upstream_capacity_configure_handler(ngx_http_request_t *r)
{
    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    ngx_int_t rc = ngx_http_discard_request_body(r);

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

    ngx_http_upstream_capacity_configure_param_t param;
    ngx_memzero(&param, sizeof(ngx_http_upstream_capacity_configure_param_t));
    param.method_type = METHOD_NONE;
    
    if (parse_nginx_query_string(r, &param) != NGX_OK) {
        return NGX_ERROR;
    }

    if (exec_configuration(r, &param) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_chain_t out;
    out.buf  = create_response_buf(r, param.response_buf);
    out.next = NULL;

    r->headers_out.status = NGX_HTTP_OK;

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

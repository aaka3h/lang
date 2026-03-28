/*
   langweb.c — Embedded web server for Lang
   
   Adds import "web" to Lang with:
     route(path, handler_fn)   — register a route
     serve(port)               — start server (blocking)
     html(str)                 — return HTML response
     json_response(str)        — return JSON response
     redirect(url)             — redirect response
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "value.h"
#include "interp.h"
#include "parser.h"

#define MAX_ROUTES 64
#define BUFSIZE    8192

typedef struct {
    char   path[256];
    Value  handler;   /* VAL_FUNCTION or VAL_NATIVE */
} Route;

static Route  g_routes[MAX_ROUTES];
static int    g_route_count = 0;
static Env   *g_web_env = NULL;

/* ── Request context (passed to handler) ── */
typedef struct {
    char method[8];
    char path[256];
    char query[512];
    char body[4096];
} Request;

/* ── Response type ── */
#define RESP_HTML     0
#define RESP_JSON     1
#define RESP_REDIRECT 2

typedef struct {
    int  type;
    char body[8192];
} Response;

static Response g_last_response;

/* ── Lang built-ins for web module ── */
static Value web_html(Value *a, int n) {
    g_last_response.type = RESP_HTML;
    if (n >= 1 && a[0].type == VAL_STRING)
        strncpy(g_last_response.body, a[0].as.s, sizeof(g_last_response.body)-1);
    else
        g_last_response.body[0] = '\0';
    return val_string(g_last_response.body);
}

static Value web_json_response(Value *a, int n) {
    g_last_response.type = RESP_JSON;
    if (n >= 1 && a[0].type == VAL_STRING)
        strncpy(g_last_response.body, a[0].as.s, sizeof(g_last_response.body)-1);
    else
        g_last_response.body[0] = '\0';
    return val_string(g_last_response.body);
}

static Value web_redirect(Value *a, int n) {
    g_last_response.type = RESP_REDIRECT;
    if (n >= 1 && a[0].type == VAL_STRING)
        strncpy(g_last_response.body, a[0].as.s, sizeof(g_last_response.body)-1);
    return val_null();
}

static Value web_route(Value *a, int n) {
    if (n < 2 || a[0].type != VAL_STRING) return val_null();
    if (g_route_count >= MAX_ROUTES) return val_null();
    strncpy(g_routes[g_route_count].path, a[0].as.s, 255);
    g_routes[g_route_count].handler = a[1];
    g_route_count++;
    return val_null();
}

/* ── HTTP helpers ── */
static void parse_request(const char *buf, Request *req) {
    memset(req, 0, sizeof(*req));
    sscanf(buf, "%7s %255s", req->method, req->path);
    /* split path and query */
    char *q = strchr(req->path, '?');
    if (q) { strncpy(req->query, q+1, 511); *q = '\0'; }
}

static void send_response(int sock, int type, const char *body) {
    char header[512];
    const char *ct = (type == RESP_JSON) ? "application/json" : "text/html";
    if (type == RESP_REDIRECT) {
        snprintf(header, sizeof(header),
            "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n", body);
        send(sock, header, strlen(header), 0);
        return;
    }
    int blen = strlen(body);
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n", ct, blen);
    send(sock, header, strlen(header), 0);
    send(sock, body, blen, 0);
}

static void send_404(int sock) {
    const char *body = "<h1>404 Not Found</h1>";
    char header[256];
    snprintf(header, sizeof(header),
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n\r\n", strlen(body));
    send(sock, header, strlen(header), 0);
    send(sock, body, strlen(body), 0);
}

/* Call a Lang function handler */
static void call_handler(Value handler, Request *req) {
    /* Reset response */
    g_last_response.type = RESP_HTML;
    g_last_response.body[0] = '\0';

    if (handler.type == VAL_NATIVE) {
        handler.as.native(NULL, 0);
    } else if (handler.type == VAL_FUNCTION) {
        extern Interpreter g_interp;
        Value args[1];
        int argc = 0;
        Env *call_env = env_new(handler.as.fn.closure);
        Node *body = handler.as.fn.def->as.fn_def.body;
        interp_eval(&g_interp, body, call_env);
        env_free(call_env);
        (void)args; (void)argc;
    }
}


/* ── Static file serving ── */
static char g_static_path[256] = "";
static char g_static_url[256]  = "";

static Value web_static(Value *a, int n) {
    if (n < 2 || a[0].type != VAL_STRING || a[1].type != VAL_STRING) return val_null();
    strncpy(g_static_url,  a[0].as.s, 255);
    strncpy(g_static_path, a[1].as.s, 255);
    return val_null();
}

static const char *mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css")  == 0) return "text/css";
    if (strcmp(ext, ".js")   == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png")  == 0) return "image/png";
    if (strcmp(ext, ".jpg")  == 0) return "image/jpeg";
    if (strcmp(ext, ".svg")  == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico")  == 0) return "image/x-icon";
    return "application/octet-stream";
}

static int try_serve_static(int sock, const char *url_path) {
    if (!g_static_url[0] || !g_static_path[0]) return 0;
    if (strncmp(url_path, g_static_url, strlen(g_static_url)) != 0) return 0;

    /* Build file path */
    char filepath[512];
    const char *rel = url_path + strlen(g_static_url);
    snprintf(filepath, 511, "%s/%s", g_static_path, rel);

    /* Security: no path traversal */
    if (strstr(filepath, "..")) return 0;

    FILE *f = fopen(filepath, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f); rewind(f);
    char *buf = malloc(sz);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return 0; }
    fclose(f);

    char header[256];
    snprintf(header, 255,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
        mime_type(filepath), sz);
    send(sock, header, strlen(header), 0);
    send(sock, buf, sz, 0);
    free(buf);
    return 1;
}

static Value web_serve(Value *a, int n) {
    int port = 8080;
    if (n >= 1 && a[0].type == VAL_INT) port = (int)a[0].as.i;
    if (n >= 1 && a[0].type == VAL_FLOAT) port = (int)a[0].as.f;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return val_null(); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return val_null();
    }
    listen(server_fd, 10);

    printf("Lang web server running on http://localhost:%d\n", port);
    printf("Press Ctrl+C to stop.\n\n");
    fflush(stdout);

    signal(SIGPIPE, SIG_IGN);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t clen = sizeof(client_addr);
        int client = accept(server_fd, (struct sockaddr*)&client_addr, &clen);
        if (client < 0) continue;

        char buf[BUFSIZE]; memset(buf, 0, BUFSIZE);
        recv(client, buf, BUFSIZE-1, 0);

        Request req;
        parse_request(buf, &req);
        printf("%s %s\n", req.method, req.path);
        fflush(stdout);

        /* Try static files first */
        if (try_serve_static(client, req.path)) { close(client); continue; }

        /* Find matching route */
        int found = 0;
        for (int i = 0; i < g_route_count; i++) {
            if (strcmp(g_routes[i].path, req.path) == 0) {
                call_handler(g_routes[i].handler, &req);
                send_response(client, g_last_response.type, g_last_response.body);
                found = 1;
                break;
            }
        }
        if (!found) send_404(client);

        close(client);
    }

    close(server_fd);
    return val_null();
}

void stdlib_load_web(Env *env) {
    env_set(env, "route",         val_native(web_route));
    env_set(env, "serve",         val_native(web_serve));
    env_set(env, "html",          val_native(web_html));
    env_set(env, "json_response", val_native(web_json_response));
    env_set(env, "redirect",      val_native(web_redirect));
    env_set(env, "route_static",   val_native(web_static));
}

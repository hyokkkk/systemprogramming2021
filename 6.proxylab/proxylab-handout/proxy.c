#include <stdio.h>
#include <stdbool.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


typedef struct {
    char method[4];
    char uri[MAXLINE];
    char port[100];
    char path[MAXLINE];
    char version[10];
} Req;

typedef struct {
    char host[7];
    char hostname[MAXLINE];
    char connection[20];         //always be "close"
    char proxy_connection[30];   //always be "close"
} Req_hdr;

void sequential_proxy(int fd, Req* request, Req_hdr* req_hdr);
void parse_request(Req* req, Req_hdr* req_hdr);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

int main(int argc, char** argv){
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;

    Req req;
    Req_hdr req_hdr;

    /* init request elements */
    memset(req.method, 0, 4);
    memset(req.uri, 0, MAXLINE);
    memset(req.port, 0, 100);
    memset(req.path, 0, MAXLINE);
    memset(req.version, 0, 10);

    /* init header elements */
    strcpy(req_hdr.host, "Host: ");
    strcpy(req_hdr.connection, "Connection: close");
    strcpy(req_hdr.proxy_connection, "Proxy_Connection: close");

    if (argc != 2){
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        return 1;
    }

    listenfd = Open_listenfd(argv[1]);
    while(1){
        clientlen = sizeof(clientaddr);
        printf("[d]accept 준비중\n");

        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        sequential_proxy(connfd, &req, &req_hdr);
        Close(connfd);
    }
    return 0;
}

void sequential_proxy(int fd, Req* req, Req_hdr* req_hdr){
    printf("\n**[d]sequential에 들어옴\n");

    char buf[MAXLINE];
    char request_buf[MAXLINE];
    char response_buf[MAXLINE];

    rio_t rio;
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    // GET http://www.cmu.edu/hub/index.html HTTP/1.1
    printf("%s\n", buf);

    // parse HTTP request from browser
    sscanf(buf, "%s %s %s", req->method, req->uri, req->version);
    printf("[d]method: %s\n", req->method);
    printf("[d]uri: %s\n", req->uri);
    printf("[d]version: %s\n", req->version);

    // parse the request
    if (strcasecmp(req->method, "GET")){
        printf("Only able to use GET\n");
        exit(1);
    }
    // 서버에게 request line이랑 headers 보내야 함.
    parse_request(req, req_hdr);
    int requestfd = Open_clientfd(req_hdr->hostname, req->port);

    // build request buffer
    strcat(request_buf, req->method);
    strcat(request_buf, " ");
    strcat(request_buf, req->path);
    strcat(request_buf, " ");
    strcat(request_buf, req->version);
    strcat(request_buf, req_hdr->host);
    strcat(request_buf, req_hdr->hostname);
    strcat(request_buf, "\r\n");
    strcat(request_buf, user_agent_hdr);
    strcat(request_buf, "\r\n");
    strcat(request_buf, req_hdr->connection);
    strcat(request_buf, "\r\n");
    strcat(request_buf, req_hdr->proxy_connection);
    strcat(request_buf, "\r\n");
    strcat(request_buf, "\r\n");

    printf("%s", request_buf);

    // send request
    Rio_writen(requestfd, request_buf, strlen(request_buf));

    // receive response
    ssize_t n = 0;
    Rio_readinitb(&rio, requestfd);
    while((n = Rio_readnb(&rio, response_buf, MAXLINE)) > 0){
        Rio_writen(fd, response_buf, (size_t)n);
    }
    Close(requestfd);
}

//
// Parse request from browser and setting all elements in `req` and `req_hdr`
//
void parse_request(Req* req, Req_hdr* req_hdr){
    printf("\n**[d] parse_request에 들어옴\n");

    // Parse uri
    char* path_bgn, *hostname_bgn, *port_bgn;
    char* http= strstr(req->uri, "http://");

    if (!http){
        printf("Request not begin with \"http://\")\n");
        exit(1);
    }

    // 1) extract path
    hostname_bgn = http + 7; // http:// 이후에 /로 시작하는 거 있느냐.
    if ((path_bgn = strstr(hostname_bgn, "/"))){ strcpy(req->path, path_bgn); }
    else{ strcpy(req->path, "/"); } // no path -> make default path as /

    // 2) extract port for open_clientfd()
    // 3) extract hostname for req_hdr
    if ((port_bgn = strstr(hostname_bgn, ":"))){
        strncpy(req->port, port_bgn+1, path_bgn-port_bgn-1); // exclude :
        strncpy(req_hdr->hostname, hostname_bgn, port_bgn-hostname_bgn);
    }else {
        strcpy(req->port, "80");        // default port: 80
        strncpy(req_hdr->hostname, hostname_bgn, path_bgn-hostname_bgn);
    }
    // 4) set version
    strcpy(req->version, "HTTP/1.0\r\n");

    printf("[debug] hostname: %s\n", req_hdr->hostname);
    printf("[debug] path: %s\n", req->path);
    printf("[debug] port: %s\n", req->port);
    printf("[debug] version: %s\n", req->version);

    /*
    char req_buf[MAXLINE];
    memset(req_buf, 0, MAXLINE);
    // GET /path HTTP/1.0 으로 만듦.
    strcat(req_buf, req->method);
    strcat(req_buf, " ");
    strcat(req_buf, req->path);
    strcat(req_buf, " ");
    strcat(req_buf, "HTTP/1.0\r\n");

    printf("[d] REQ: %s", req_buf);
    */
}





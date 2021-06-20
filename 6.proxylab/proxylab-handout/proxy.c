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
    char connection[25];         //always be "close"
    char proxy_connection[30];   //always be "close"
} Req_hdr;

typedef struct _Cache{
    char data[MAX_OBJECT_SIZE];
    char* key_uri;      // key for finding cache
    struct _Cache* next;
    size_t LRUcnt;
} Cache;

static Cache* root;

void* sequential_proxy(void*);
void parse_uri(Req*, Req_hdr*);
void parse_header(rio_t*, Req_hdr*);

size_t total_cache_size = 0;
Cache* search_cache(char*);
bool insert_cache(Cache*, int);
void print_cache();
void evict_cache();

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

int main(int argc, char** argv){
    int listenfd, connfd;
    pthread_t tid;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;


    if (argc != 2){
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        return 1;
    }
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    // Init cache dummy block
    root = (Cache*)malloc(sizeof(Cache));
    memset(root->data, 0, MAX_OBJECT_SIZE);
    root->key_uri = NULL;
    root->next = NULL;
    root->LRUcnt = 0;

    listenfd = Open_listenfd(argv[1]);
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        //sequential_proxy(connfd, &req, &req_hdr);
        Pthread_create(&tid, NULL, sequential_proxy, &connfd);
    }
    return 0;
}

void* sequential_proxy(void* vargp){
    int fd = *((int*)vargp);
    Pthread_detach(pthread_self());

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
    memset(req_hdr.hostname, 0, sizeof(req_hdr.hostname));
    strcpy(req_hdr.connection, "Connection: close");
    strcpy(req_hdr.proxy_connection, "Proxy_Connection: close");

    /* Declare buffers */
    char buf[MAXLINE];
    char request_buf[MAXLINE];
    char response_buf[MAXLINE];

    // 이거 안 해줬어서 첫번째 testcase만 통과했었음
    memset(buf, 0, MAXLINE);
    memset(request_buf, 0, MAXLINE);
    memset(response_buf, 0, MAXLINE);

    // GET http://www.cmu.edu/hub/index.html HTTP/1.1 를 받는다.
    rio_t rio;
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);

    // parse HTTP request from browser
    sscanf(buf, "%s %s %s", req.method, req.uri, req.version);

    /* debugging
    printf("Request headers:\n");
    printf("%s\n", buf);
    printf("[d]method: %s\n", req.method);
    printf("[d]uri: %s\n", req.uri);
    printf("[d]version: %s\n", req.version);
    */

    // check the method is "GET"
    if (strcasecmp(req.method, "GET")){
        printf("Only able to use GET\n");
        exit(1);
    }
    // 이때 cache에 있는지 체크한다. uri만 가지고 할 수 있음.
    Cache* node;
    if ((node = search_cache(req.uri))){
        /*
        캐쉬에 있으면 make request, send request, receive response 할 필요 없이
        그냥 캐쉬에 있는 거 response_buf에 넣어주면 된다.
        */
        //printf("Cache hit!\n");
        Rio_writen(fd, node->data, strlen(node->data));
        // LRUcnt도 update해줌
    }
    // 캐쉬에 없으면 insert cache, parsing해서 서버에 다 보내야 함.
    else{
        //printf("Cache miss!\n");

        // make request (req line and req header)
        parse_uri(&req, &req_hdr);
        int requestfd = Open_clientfd(req_hdr.hostname, req.port);

        // build request buffer
        strcat(request_buf, req.method);
        strcat(request_buf, " ");
        strcat(request_buf, req.path);
        strcat(request_buf, " ");
        strcat(request_buf, req.version);
        strcat(request_buf, req_hdr.host);          // "Host: "
        strcat(request_buf, req_hdr.hostname);
        strcat(request_buf, "\r\n");
        strcat(request_buf, user_agent_hdr);
        strcat(request_buf, "\r\n");
        strcat(request_buf, req_hdr.connection);
        strcat(request_buf, "\r\n");
        strcat(request_buf, req_hdr.proxy_connection);
        strcat(request_buf, "\r\n");
        strcat(request_buf, "\r\n");

        // send request
        Rio_writen(requestfd, request_buf, strlen(request_buf));

        // receive response
        ssize_t n = 0;
        ssize_t obj_size = 0;
        Rio_readinitb(&rio, requestfd);

        // new node만들고
        Cache* newNode = (Cache*)malloc(sizeof(Cache));
        char* cache_buf = (char*)calloc(1, MAX_OBJECT_SIZE);
        bool tooBig = false;

        while((n = Rio_readnb(&rio, response_buf, MAXLINE)) > 0){
            Rio_writen(fd, response_buf, (size_t)n);

            // data의 size가 MAX_OBJECT_SIZE 102400 보다 작은지 확인
            obj_size += n;
            if (obj_size > MAX_OBJECT_SIZE) { 
                tooBig = true;
                continue; }
            strncat(cache_buf, response_buf, n);
        }

        /* node의 element setting */
        newNode->key_uri = (char*)malloc(sizeof(req.uri));
        if (!tooBig) strcpy(newNode->data, cache_buf);
        strcpy(newNode->key_uri, req.uri);
        newNode->next = NULL;

        /*
        printf("data size: %d\n", (int)obj_size);
        printf("key uri: %s\n", newNode->key_uri);
        printf("data: %s\n", newNode->data);
        */


        // mutex lock 해야
        while (!insert_cache(newNode, obj_size)){
            evict_cache();
        }
        // mutex unlock 해야
        //print_cache();

        // lru update 해야
        Close(requestfd);
    }
    Close(fd);      // 이거때문에 response가 중간에 끊겼었음.

    return NULL;
}
void update_LRU(Cache* node){
}
void evict_cache(){
    Cache* prev = root, *curr = root->next;
    if (!curr) { printf("empty cache! cannot evict\n"); exit(1); }

    // decide eviction target
    while (curr){
        if (curr->LRUcnt == 0){
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    // do eviction
    total_cache_size -= sizeof(curr->data);
    prev->next = curr->next;
    free(curr);
}

void print_cache(){
    //printf("print_cache에 들어옴\n");
    Cache* curr = root;
    //printf("root uri: %s\n", curr->key_uri);

    curr = curr->next;
    while(curr){
        printf("cache: %s\n", (curr->key_uri));
        curr = curr->next;
    }
}

// 그냥 맨 앞에다 넣는 게 편하다.
bool insert_cache(Cache* node, int obj_size){
    //printf("insert_cache에 들어옴\n");
    if (total_cache_size + obj_size > MAX_CACHE_SIZE){
        return false;
    }
    node->next = root->next;
    root->next = node;
    total_cache_size += obj_size;
    return true;
}

// 우리 과제에서는 그냥 port 80만 들어올 것 같으니.....
Cache* search_cache(char* uri){
    Cache *curr = root;
    curr = curr->next;
    while(curr){
        if (!curr->key_uri){ printf("Valid node, null key_uri\n"); exit(1); }
        if (!strcmp(curr->key_uri, uri)){ return curr; }
        curr = curr->next;
    }
    return NULL;
}

//
// Parse request from browser and setting all elements in `req` and `req_hdr`
//
void parse_uri(Req* req, Req_hdr* req_hdr){

    // Parse uri
    char* path_bgn, *hostname_bgn, *port_bgn;
    char* http= strstr(req->uri, "http://");

    if (!http){
        printf("Request not begin with \"http://\")\n");
        exit(1);
    }

    /* 1) extract path */
    hostname_bgn = http + 7;
    // http:// 이후에 /로 시작하는 거 있느냐.
    if ((path_bgn = strstr(hostname_bgn, "/"))){ strcpy(req->path, path_bgn); }
    // no path -> make default path as /
    else{ strcpy(req->path, "/"); }

    /* 2) extract port for open_clientfd() */
    /* 3) extract hostname for req_hdr */
    if ((port_bgn = strstr(hostname_bgn, ":"))){
        strncpy(req->port, port_bgn+1, path_bgn-port_bgn-1); // remove `:`
        strncpy(req_hdr->hostname, hostname_bgn, port_bgn-hostname_bgn);
    }else {
        strcpy(req->port, "80");        // default port: 80
        strncpy(req_hdr->hostname, hostname_bgn, path_bgn-hostname_bgn);
    }
    /* 4) set version */
    strcpy(req->version, "HTTP/1.0\r\n");
    /*
    printf("[debug] hostname: %s\n", req_hdr->hostname);
    printf("[debug] path: %s\n", req->path);
    printf("[debug] port: %s\n", req->port);
    printf("[debug] version: %s\n", req->version);
    */
}


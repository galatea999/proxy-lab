/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) // argument count, argument vector(array)
{
  int listenfd, connfd; // 기존에 만들었던 웹서버와는 달리 먼저 선언
  char hostname[MAXLINE], port[MAXLINE]; //
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // Ipv4와 Ipv6 둘 다 담을 수 있는 구조체

  /* Check command line args */
  if (argc != 2) // when we type ./tiny 8000, argc=2, argv[0] = "./tiny", argv[1] = "8000"
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); 
  //helper function that do socket + bind + listen at a time
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0); //Reverse DNS(getaddrinfo) : return host name, port 
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit / read write 역할을 해야 함
    Close(connfd); // line:netp:tiny:close
  }
}

/* Doit todo
* 1. Read HTTP request
* 2. Request Parsing : pick method, uri, version (ex. GET index.html, HTTP/1.1)
* 3. searching file using uri
* 4. send Response
* But, why do we get method although we only deal with GET?
* :: because we'll alert error when they send another method
*/

void doit (int fd) {
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  rio_t rio; //Robust I/O type. Buffer for parsing
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE); // sizeof(buf) == MAXLINE
  //now GET, /index.html, HTTP/1.1 \r\n is in buf (이거 직접 확인해 볼 수 없을까 디버깅 해보자 이따)
  sscanf(buf, "%s %s %s", method, uri, version); //scan and def string in buffer
  if (strcmp(method, "GET") != 0) {
    /* You should not use != when you compare string.
    * Why? Because String is pointer in C. But '!=' is for Address comparing.
    * when we want to compare value, we should use strcmp(string compare)
    */
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio); 

  char filename[MAXLINE], cgiargs[MAXLINE];
  
  

  int is_static = parse_uri(uri, filename, cgiargs);
 
  struct stat sbuf; // predefined struct by POSIX standard
  if (stat(filename, &sbuf) < 0) {
  // 함수 호출 + 결과 체크 동시에 됨
  // stat() : sbuf 안에 파일 크기 포함한 각종 정보가 들어옴
  // sbuf.st_size : file size
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
} 
// 이미 결과가 변수에 들어 있는 상태


  if (is_static == 1)
  serve_static(fd, filename, sbuf.st_size);

  else 
  serve_dynamic(fd, filename, sbuf.st_size);
  
  
}

void read_requesthdrs(rio_t *rp) { //socket stream draining
    char buf[MAXLINE]; 
    Rio_readlineb(rp, buf, MAXLINE); //do - while pattern
    while (strcmp(buf, "\r\n")!= 0) {
      Rio_readlineb(rp, buf, MAXLINE);
    }
  }


  /* Serve_static To-do
  * 1. Send Header (Rio_writen)
  * 2. Open File
  * 3. memory mapping by mmap
  * 4. Send to socket (Rio_writen)
  * 5. return memory by munmap
  */

void serve_static(int fd, char *filename, int filesize){


}

void get_filetype(char *filename, char *filetype){
  
}

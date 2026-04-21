#include <stdio.h>
#include "csapp.h"


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void doit(int fd); //전방선언 (Forward Declaration)
int parse_uri(char *uri, char *filename, char *cgiargs, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = //서버한테 요청 보낼 때 header에 넣어야 하는 값.
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) // argument count, argument vector(array)
{
  int listenfd, connfd; 
  char hostname[MAXLINE], port[MAXLINE]; //
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // Ipv4와 Ipv6 둘 다 담을 수 있는 구조체

  /* Check command line args */
  if (argc != 2) // when we type ./tiny 8000, argc=2, argv[0] = "./tiny", argv[1] = "8000"
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1); //main에선 괜찮음
  } // !!! 테스트 해보기

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

void doit (int fd) {
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
 
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
    clienterror(fd, method, "501", "Not Implemented", "Sorry, in this server I didn't implement this method");
    return;
  }
  
  //
  int res = parse_uri(uri, hostname, port, path);
  if (res<0) {
    clienterror(fd, uri, "400", "Bad Request", "Inavalid URI");
    return;
  }

  //Request를 재구성해서 서버로 전달
  int serverfd = open_clientfd(hostname, port);
  char request[MAXLINE];
    sprintf(request, "%s %s HTTP/1.0\r\n", method, path);
    Rio_writen(serverfd, request, strlen(request)); 
    sprintf(request, "Host: %s:%s\r\n", hostname, port);
    Rio_writen(serverfd, request, strlen(request)); 
    sprintf(request, "%s", user_agent_hdr);
    Rio_writen(serverfd, request, strlen(request)); 
    sprintf(request, "Connection: close\r\n");
    Rio_writen(serverfd, request, strlen(request));
    sprintf(request, "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, request, strlen(request)); 
    Rio_readlineb(&rio, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) { //끝까지
      if (strstr(buf, "Host:")){ /* 스킵 */} 
      else if (strstr(buf, "User-Agent:")){ /* 스킵 */}
      else if (strstr(buf, "Connection:")){ /* 스킵 */}
      else if (strstr(buf, "Proxy-Connection:")){ /* 스킵 */}
      else {
        Rio_writen(serverfd, buf, strlen(buf));
      }
      Rio_readlineb(&rio, buf, MAXLINE);
      }
    sprintf(request, "\r\n");
    Rio_writen(serverfd, request, strlen(request));  

  //서버에서 응답을 받음
  rio_t server_rio;
  Rio_readinitb(&server_rio, serverfd);

  size_t n;
  while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
    Rio_writen(fd, buf, n);
  }

  close(serverfd);

}

  int parse_uri(char *uri, char *hostname, char *port, char *path){ 

    char *ptr = strstr(uri, "://"); // str을 찾을 땐 큰 따옴표
    if (ptr == NULL) return -1;
    ptr += 3; //현재 ptr은 "localhost:15213/index.html"

    char *host_end = strchr(ptr, ':'); //string character : find specific char and return pointer of the location
    if (host_end == NULL) return -1;
    *host_end = '\0'; // localhost\015213/index.html
    strcpy(hostname, ptr);

    ptr = host_end + 1; // \0은 null 문자이고, 시스템이 한 글자 취급함
    char *port_end = strchr(ptr, '/'); 
    if (port_end == NULL) return -1;
    *port_end = '\0'; // 15213\0index.html
    strcpy(port, ptr);

    ptr = port_end + 1;
    strcpy(path, "/"); //strcpy는 signature가 char *(str)이기에 "" 써줘야함
    strcat(path, ptr);

    return 0;
    
  }

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE];
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); 
  sprintf(buf, "Content-type:text/html\r\n");
  Rio_writen(fd, buf, strlen(buf)); 
  sprintf(buf, "\r\n");
  Rio_writen(fd, buf, strlen(buf)); 
  sprintf(buf, "<html><b>원인 : %s</b><p>%s: %s</p></html>\r\n", cause, shortmsg, longmsg);
  Rio_writen(fd, buf, strlen(buf)); 


}
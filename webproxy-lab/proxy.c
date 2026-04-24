#include <stdio.h>
#include "csapp.h"
#include <time.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_ENTRIES (MAX_CACHE_SIZE / MAX_OBJECT_SIZE) //최악의 경우(들어온 것들이 모두 MAX_OBJECT_SIZE일때) 몇개를 저장할 수 있는가. 진짜 정확하게 하려면 동적할당 해야하는데 그러면 복잡해지므로 이렇게 가신대



void doit(int fd); //전방선언 (Forward Declaration)
int parse_uri(char *uri, char *filename, char *cgiargs, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void * thread_func(void *arg);
int cache_find(char *url, char *data, int *size);
void cache_store(char *url, char *data, int size);



typedef struct {
  char url[MAXLINE]; //key
  char data[MAX_OBJECT_SIZE]; // value (서버응답)
  int size; // 응답 크기
  time_t last_used; // LRU용 타임 스탬프
} cache_entry;


cache_entry cache[MAX_ENTRIES]; 
int cache_count = 0;
pthread_rwlock_t cache_lock;


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
  pthread_t tid;

  /* Check command line args */
  if (argc != 2) // when we type ./tiny 8000, argc=2, argv[0] = "./tiny", argv[1] = "8000"
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1); //main에선 괜찮음
  } // !!! 테스트 해보기

  listenfd = Open_listenfd(argv[1]); 
  pthread_rwlock_init(&cache_lock, NULL);

  //helper function that do socket + bind + listen at a time
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0); //Reverse DNS(getaddrinfo) : return host name, port 
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    pthread_create(&tid, NULL, thread_func, (void*)connfd);
  }
}

void * thread_func(void *arg) { //pthread_create에서 arg로 void*를 요구하기 때문에 여기서도 void로 받아주고 int로 바꿔줌
  pthread_detach(pthread_self());
  int fd = (int) arg;
  doit(fd);
  Close(fd);
  return NULL;


}

void doit (int fd) {
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE], port[MAXLINE], path[MAXLINE], cache_key[MAXLINE];
 
  rio_t rio; //Robust I/O type. Buffer for parsing
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE); // sizeof(buf) == MAXLINE
  //now GET, /index.html, HTTP/1.1 \r\n is in buf (이거 직접 확인해 볼 수 없을까 디버깅 해보자 이따)
  sscanf(buf, "%s %s %s", method, uri, version); //scan and def string in buffer
  strcpy(cache_key, uri);
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

  
/* cache 여부 확인
 * 1. cache find로 존재 여부 확인
 *    있을 시 -> 그대로 print
 *    없을 시 -> cache_store 후 서버로 보내기..가 가능한가? 서버에서 응답을 받아 와서 보내야하는ㄱ ㅓ아닌가?
 *
*/

  char cache_data[MAX_OBJECT_SIZE];
  int cache_size = 0;
  
  if(cache_find(cache_key, cache_data, &cache_size)) { //signature가 int *이니, int인 cache_size의 주솟값을 보내야해서 &
    Rio_writen(fd, cache_data, cache_size); 
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


  // 서버로 응답 보냄
  size_t n;
  while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
    Rio_writen(fd, buf, n);
    if (cache_size +n <= MAX_OBJECT_SIZE) {
      //caching 과정, 이어 붙임
    memcpy(cache_data+cache_size, buf, n); //offset 패턴 주의! 
    cache_size += n; 

    }
    
}
cache_store(cache_key, cache_data, cache_size);
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

int cache_find(char *url, char *data, int *size) {
  pthread_rwlock_rdlock(&cache_lock);
  for (int i = 0; i<cache_count; i++) {

  if (strcmp(url, cache[i].url) == 0) //아, 어차피 cache url을 수정할게 아니기 때문에 인자로 안 받았구나
  {
    memcpy(data, cache[i].data, cache[i].size); //data에 바이너리도 있을 수 있기에 strcpy를 사용하지 않음
    *size = cache[i].size;
    cache[i].last_used = time(NULL); //현재 시간을 초단위로 반환
    
    pthread_rwlock_unlock(&cache_lock);

    return 1; // C에서 사이즈가 중요해지는 이유 : 메모리를 다루기 때문에. C의 배열(포인터)는 시작점만 알지 자기들이 어디서 끝나는지 모름. 
  }
}
    pthread_rwlock_unlock(&cache_lock);
    return 0;
  
  

}

void cache_store(char *url, char *data, int size) {
  pthread_rwlock_wrlock(&cache_lock);
  
  // 1. 캐시 꽉 찼으면 → LRU 교체 (last_used 제일 오래된 거 삭제)
  if (cache_count ==MAX_ENTRIES) {  
    int lru_idx = 0; //
    for (int i=1; i<cache_count; i++) {
      if (cache[i].last_used < cache[lru_idx].last_used) 
      {
        lru_idx = i;
      }
    

    }
    memcpy(cache[lru_idx].data, data, size); //이미지, 동영상 같은 것들은 \0이 중간에 있을 수 있기에 strcpy 못 씀. str은 \0을 기준으로 끊으니까
    strcpy(cache[lru_idx].url, url);
    cache[lru_idx].size = size;
    cache[lru_idx].last_used = time(NULL);
  } else {
  // 2. 안 찼으면 → 그냥 추가
    memcpy(cache[cache_count].data, data, size); //이미지, 동영상 같은 것들은 \0이 중간에 있을 수 있기에 strcpy 못 씀. str은 \0을 기준으로 끊으니까
    strcpy(cache[cache_count].url, url);
    cache[cache_count].size = size;
    cache[cache_count].last_used = time(NULL);
    cache_count++;

  }
  pthread_rwlock_unlock(&cache_lock);

}

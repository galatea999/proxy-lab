#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    // 1. make socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //fd(번호표)만 받은 상태이며, 어디에도 연결 안됨. IP도 포트도 없음

    // 2. setting address : Where I accept (my IP)
    struct sockaddr_in addr;
    addr.sin_family = AF_INET; //socket에서 어떤 통신방식을 쓸지 결정한거고, addr는 실제 주소값을 담음. socket은 껍데기 생성, bind에선 실제 Ip 넣기
    addr.sin_port = htons(8080); // 바이트 순서 변환. 내 컴퓨터는 리틀 엔디안 -> 네트워크는 빅 엔디안
    addr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0 : I will accept every IP

    // 3. bind : bind socket to 
    bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)); 
    //    Common type for sockaddr_in(Ipv4, 16byte), sockaddr(Ipv6 28byte) / size로 IPv4와 IPv6 판단

    //4. listen
    listen(sockfd, 10); //I choose 10 because It's tiny server
    while(1) {
    //5. accept
        int connfd = accept(sockfd, NULL, NULL); // connfd라는 통로를 열어 줌. 현재 NULL인 곳은 IP와 Port. 이건 작은 echo server라 null 처리
    //6. echo
        char buf[1024]; //temporal 1kb space . 네트워크에서 날아온 걸 임시로 받아놓는 장소. 다시 write로 돌려주기 위해.
        int n = read(connfd, buf, sizeof(buf)); // n은 읽은 바이트 수. sizeof(buf)는 최대 상한선을 정해주는 것. 
    // 예를 들어 , connfd가 10000byte라면 buffer overflow이니 C스럽게 방지해주는 것
        write(connfd, buf, n); //buffer를 n byte까지만 출력. 만약 sizeof(buf)로 하면 나머지 1024바이트가 다 감. 

    // 7. close
        close(connfd);

    }
    close(sockfd);






}
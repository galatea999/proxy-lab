/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    sprintf(content, "QUERY_STRING=%s\r\n<p>", buf); 
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf); //C의 str 함수들은 \0을 기준으로 움직임.
    strcpy(arg2, p + 1); // p+1은 s 주소지만, strcpy는 \0 만날 때까지 쭉 복사함
    n1 = atoi(strchr(arg1, '=') + 1); //ascii to integer 의미가 직관적이지는 않음
    n2 = atoi(strchr(arg2, '=') + 1); //해당 pointer부터 \0까지 숫자로 바꾸는 함수임
  }

  //C에서 str을 다루는 것들은 \0을 기준으로 작동함

  /* Make the response body */
  sprintf(content + strlen(content), "Welcome to add.com: ");
  sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>",
          n1, n2, n1 + n2);
  sprintf(content + strlen(content), "Thanks for visiting!\r\n");

  /* Generate the HTTP response */
  printf("Content-type: text/html\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("\r\n");
  printf("%s", content);
  fflush(stdout); //버퍼를 강제로 비우기. 지금 당장 강제로 내보내!

  exit(0);
}
/* $end adder */

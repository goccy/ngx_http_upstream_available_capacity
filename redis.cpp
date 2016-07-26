#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <netinet/tcp.h>

int main(void)
{
    int sd;
    struct sockaddr_in addr;
 
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    int flag = fcntl(sd, F_GETFL, 0);
    if (flag < 0) perror("fcntl(GET) error");
    if (fcntl(sd, F_SETFL, flag) < 0) {
        return -1;
    }
    int on = 1;
    if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) != 0) {
        fprintf(stderr, "TCP_NODELAY失敗.%d \n", errno);
    }
    fprintf(stderr, "sock = [%d]\n", sd);
 
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(6379);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    fprintf(stderr, "connect\n");

    int connect_result = connect(sd, (struct sockaddr *)&addr, sizeof(addr));
    if (connect_result < 0) {
        perror("connect");
        close(sd);
        return -1;
    }

    signal(SIGPIPE , SIG_IGN);
    fprintf(stderr, "connect successfully\n");
    //const char *payload = "*3\r\n$3\r\nSET\r\n$5\r\nmekey\r\n$7\r\nmyVALUE\r\n";
    const char *payload = "*2\r\n$3\r\nGET\r\n$5\r\nmekey\r\n";
    int send_result = send(sd, payload, strlen(payload), 0);
    fprintf(stderr, "send_result = [%d]\n", send_result);
    if (send_result < 0) {
        perror("send");
        return -1;
    }
    fprintf(stderr, "send successfully\n");

    char buf[BUFSIZ] = {0};
    int recv_result = recv(sd, buf, BUFSIZ, 0);
    if (recv_result < 0) {
        perror("recv");
        return -1;
    }

    fprintf(stderr, "recv successfully = [%s]\n", buf);
 
    close(sd);
    return 0;
}

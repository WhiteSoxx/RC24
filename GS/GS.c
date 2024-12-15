#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int fd, errcode;
ssize_t n;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr;
char buffer[1024];
int gamestate = 0;


int main(int argc, char *argv[])
{
    char GSPort[] = "50054";
    int verbose = 0;

    for(int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
        if(!strcmp(argv[i], "-p"))
        {
            printf("p found\n");
            strcpy(GSPort, argv[i+1]);
            i++;
        }
        else if(!strcmp(argv[i], "-v"))
        {
            printf("v found\n");
            verbose = 1;
        }
    }

    printf("GSPort: %s\n", GSPort);
    printf("verbose: %d\n", verbose);
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd == -1) (1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(NULL, GSPort, &hints, &res);
    if (errcode != 0) {
        exit(1);
    }

    n = bind(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1) exit(1);

    while (1) {
        addrlen = sizeof(addr);
        n = recvfrom(fd, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
        if (n == -1) exit(1);

        write(1, "received: ", 10);
        write(1, buffer, n);

        n = sendto(fd, buffer, n, 0, (struct sockaddr *)&addr, addrlen);
        if (n == -1) exit(1);
    }

    freeaddrinfo(res);
    close(fd);


    return 0;
}

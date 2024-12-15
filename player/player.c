#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define GN 54


int fd, errcode;
ssize_t n;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr;
char buffer[1024];
int gamestate = 0;

int main(int argc, char *argv[])
{
    int i;
    char GSIP[] = "localhost";
    char GSPort[] = "50054";

    for(i = 0; i < argc; i++)
    {
        if(!strcmp(argv[i], "-n"))
        {
            strcpy(GSIP, argv[i+1]);
            i++;
        }
        else if(!strcmp(argv[i], "-p"))
        {
            strcpy(GSPort, argv[i+1]);
            i++;
        }
    }

    printf("GSIP: %s\n", GSIP);
    printf("GSPort: %s\n", GSPort);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errcode = getaddrinfo("localhost", GSPort, &hints, &res);
    if (errcode != 0) exit(1);

    char PLID[6] = "";
    char max_playtime[3] = "";
    char *key[4];

    char cmd_buffer[1024] = "";
    
    while(fgets(cmd_buffer, sizeof(cmd_buffer), stdin) != NULL)
    {
        cmd_buffer[strcspn(cmd_buffer, "\n")] = 0;
        char *token = strtok(cmd_buffer, " ");
        char* command[1024];
        command[0] = token;

        i = 1;
        token = strtok(NULL, " ");
        while(token != NULL)
        {
            command[i] = token;
            i++;
            token = strtok(NULL, " ");
        }

        if(command[0] == NULL)
        {
            printf("command[0] is NULL\n");
            continue;
        }
        else if(!strcmp(command[0], "start"))
        {
            strcpy(PLID, command[1]);
            strcpy(max_playtime, command[2]);

            char msg_buffer[15] = "";
            strcat(msg_buffer, "SNG ");
            strcat(msg_buffer, PLID);
            strcat(msg_buffer, " ");
            strcat(msg_buffer, max_playtime);
            strcat(msg_buffer, "\n");
            n = sendto(fd, msg_buffer, 15, 0, res->ai_addr, res->ai_addrlen);
                if (n == -1) exit(1);


            addrlen = sizeof(addr);
            n = recvfrom(fd, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
            if (n == -1) exit(1);

            write(1, "echo: ", 6);
            write(1, buffer, n);


            printf("start!\n");
            continue;
        } //FIXME outros comandos só depois do jogo ter começado?
        else if(!strcmp(command[0], "try"))
        {
            key[0] = command[1];
            key[1] = command[2];
            key[2] = command[3];
            key[3] = command[4];

            //FIXME cores só podem ser algumas específicas, verificar aqui? switch case

            printf("try!\n");
            continue;
        }
        else if(!strcmp(command[0], "show_trials") || !strcmp(command[0], "st"))
        {
            printf("show trials!\n");
            continue;
        }
        else if(!strcmp(command[0], "scoreboard") || !strcmp(command[0], "sb"))
        {
            printf("scoreboard!\n");
            continue;
        }
        else if(!strcmp(command[0], "quit"))
        {
            printf("quit!\n");
            continue;
        }
        else if(!strcmp(command[0], "exit"))
        {
            freeaddrinfo(res);
            close(fd);
            printf("exit!\n");
            break;
        }
        else if(!strcmp(command[0], "debug"))
        {
            strcpy(PLID, command[1]);
            strcpy(max_playtime, command[2]);
            key[0] = command[3];
            key[1] = command[4];
            key[2] = command[5];
            key[3] = command[6];

            printf("debug!\n");
            continue;
        }

    }
    return 0;
}

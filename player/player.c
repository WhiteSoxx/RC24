#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GN 54

int main(int argc, char *argv[])
{
    int i;
    char *GSIP = "localhost";
    int GSPort = 58000+GN;

    for(i = 0; i < argc; i++)
    {
        if(!strcmp(argv[i], "-n"))
        {
            GSIP = argv[i+1];
            i++;
        }
        else if(!strcmp(argv[i], "-p"))
        {
            GSPort = atoi(argv[i+1]);
            i++;
        }
    }

    printf("GSIP: %s\n", GSIP);
    printf("GSPort: %d\n", GSPort);

    int PLID = -1;
    int max_playtime = -1;
    char *key[4];

    char buffer[1024];
    while(fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        buffer[strcspn(buffer, "\n")] = 0;
        char *token = strtok(buffer, " ");
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
            PLID = atoi(command[1]);
            max_playtime = atoi(command[2]);
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
            printf("exit!\n");
            break;
        }
        else if(!strcmp(command[0], "debug"))
        {
            PLID = atoi(command[1]);
            max_playtime = atoi(command[2]);
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

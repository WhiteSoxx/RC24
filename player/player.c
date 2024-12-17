#include "../common.h"

int fd, errcode;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr;
char buffer[MAX_BUFFER_SIZE];
int gamestate = 0;


void handle_tcp_request(const char *GSIP, const char *GSPort, const char *request, const char *filename) {
    int tcp_fd;
    struct addrinfo thints, *tres;
    char tbuffer[MAX_BUFFER_SIZE];

    memset(&thints, 0, sizeof(thints));
    thints.ai_family = AF_INET;
    thints.ai_socktype = SOCK_STREAM;

    if ((errcode = getaddrinfo(GSIP, GSPort, &thints, &tres)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errcode));
        return;
    }

    tcp_fd = socket(tres->ai_family, tres->ai_socktype, tres->ai_protocol);
    if (tcp_fd == -1) {
        perror("TCP socket failed");
        freeaddrinfo(tres);
        return;
    }

    if (connect(tcp_fd, tres->ai_addr, tres->ai_addrlen) == -1) {
        perror("Connection to server failed");
        close(tcp_fd);
        freeaddrinfo(tres);
        return;
    }

    if (send(tcp_fd, request, strlen(request), 0) == -1) {
        perror("Failed to send TCP request");
        close(tcp_fd);
        freeaddrinfo(tres);
        return;
    }

    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open local file for writing");
        close(tcp_fd);
        freeaddrinfo(tres);
        return;
    }

    int n;
    while ((n = recv(tcp_fd, tbuffer, sizeof(tbuffer) - 1, 0)) > 0) {
        tbuffer[n] = '\0';
        fputs(tbuffer, file); // Write to local file
        printf("%s", tbuffer); // Print to console
    }

    if (n < 0) {
        perror("Failed to receive file data");
    }

    fclose(file);
    close(tcp_fd);
    freeaddrinfo(tres);
    printf("\nFile '%s' saved successfully.\n", filename);
}

int main(int argc, char *argv[]) {
    char GSIP[] = DEFAULT_IP;
    char GSPort[] = DEFAULT_PORT;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-n") == 0) {
            if (i+1 < argc) {
                strcpy(GSIP, argv[++i]);
            } else {
                fprintf(stderr, "Error: Missing argument for -n\n");
                exit(1);
            }
        } else if(strcmp(argv[i], "-p") == 0) {
            if (i+1 < argc) {
                strcpy(GSPort, argv[++i]);
            } else {
                fprintf(stderr, "Error: Missing argument for -p\n");
                exit(1);
            }
        }
    }

    printf("Using GSIP: %s and GSPort: %s\n", GSIP, GSPort);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errcode = getaddrinfo(GSIP, GSPort, &hints, &res);
    if (errcode != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errcode));
        exit(1);
    }

    while(fgets(buffer, sizeof(buffer), stdin) != NULL) {

        buffer[strcspn(buffer, "\n")] = 0;
        char *command = strtok(buffer, " ");

        if (!command) {
            printf("Please enter a command.\n");
            continue;
        }

        if(strcmp(command, "start") == 0) {

            char *PLID = strtok(NULL, " ");
            char *time = strtok(NULL, " ");
            if (!PLID || !time) {
                printf("Invalid start command. Usage: start PLID time\n");
                continue;
            }
            snprintf(buffer, MAX_BUFFER_SIZE, "SNG %s %s\n", PLID, time);
            printf("Sending: %s", buffer);
            sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);

        } else if(strcmp(command, "try") == 0) {

            char *C1 = strtok(NULL, " ");
            char *C2 = strtok(NULL, " ");
            char *C3 = strtok(NULL, " ");
            char *C4 = strtok(NULL, " ");

            if (!C1 || !C2 || !C3 || !C4) {
                printf("Invalid try command. Usage: try C1 C2 C3 C4\n");
                continue;
            }

            snprintf(buffer, MAX_BUFFER_SIZE, "TRY %s %s %s %s\n", C1, C2, C3, C4);
            printf("Sending: %s", buffer);
            sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);

        } else if(strcmp(command, "quit") == 0) {

            snprintf(buffer, MAX_BUFFER_SIZE, "QUT\n");
            printf("Sending: %s", buffer);
            sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);

        } else if(strcmp(command, "exit") == 0) {

            freeaddrinfo(res);
            close(fd);
            printf("Exiting player client\n");
            exit(0);

        } else if (strcmp(command, "show_trials") == 0) {
            
            handle_tcp_request(GSIP, GSPort, "STR\n", "trials.txt");

        } else if (strcmp(command, "scoreboard") == 0) {
            
            handle_tcp_request(GSIP, GSPort, "SSB\n", "scoreboard.txt");

        } else {
            printf("Unknown command\n");
        }
    }

    return 0;
}

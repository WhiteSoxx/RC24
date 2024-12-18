#include "../common.h"

int fd, errcode;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr;
char buffer[MAX_BUFFER_SIZE];

int playerID;
int gamestate = 0;
int trial_number = 1; 

void handle_tcp_request(const char *GSIP, const char *GSPort, const char *request, const char *filename);
char* receive_udp_response();

int main(int argc, char *argv[]) {
    char GSIP[] = DEFAULT_IP;
    char GSPort[] = DEFAULT_PORT;

    // Parse command-line arguments for IP and Port
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

    // Create a UDP socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Set up the address information
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

            if (!validate_plid(PLID)){
                printf("[!] Invalid start command, PLID must be a 6-digit number.\n");
                continue;
            }
            
            snprintf(buffer, MAX_BUFFER_SIZE, "SNG %s %s\n", PLID, time);
            printf("(->) Sending: %s", buffer);
            sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
            char *response = receive_udp_response();
            printf("(<-) Server Response: %s\n", response);
            free(response);

        } else if(strcmp(command, "try") == 0) {

            char *C1 = strtok(NULL, " ");
            char *C2 = strtok(NULL, " ");
            char *C3 = strtok(NULL, " ");
            char *C4 = strtok(NULL, " ");

            if (!C1 || !C2 || !C3 || !C4) {
                printf("Invalid try command. Usage: try C1 C2 C3 C4\n");
                continue;
            }

            snprintf(buffer, MAX_BUFFER_SIZE, "TRY %s %s %s %s %d\n", C1, C2, C3, C4, trial_number++);
            printf("(->) Sending: %s", buffer);
            sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
            char* response = receive_udp_response();
            printf("(<-) Server Response: %s\n", response);
            free(response);

        } else if(strcmp(command, "quit") == 0) {
            char *PLID = strtok(NULL, " ");
            if (!PLID) {
                printf("Invalid quit command. Usage: quit PLID\n");
                continue;
            }

            snprintf(buffer, MAX_BUFFER_SIZE, "QUT %s\n", PLID);
            printf("Sending: %s", buffer);
            sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
            receive_udp_response();

        } else if(strcmp(command, "exit") == 0) {
            freeaddrinfo(res);
            close(fd);
            printf("Exiting player client\n");
            exit(0);

        } else {
            printf("Unknown command\n");
        }
    }

    return 0;
}

char* receive_udp_response() {
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char *response = malloc(MAX_BUFFER_SIZE);
    if (!response) {
        perror("Failed to allocate memory for response");
        exit(1);
    }
    
    int n = recvfrom(fd, response, MAX_BUFFER_SIZE - 1, 0, (struct sockaddr *)&server_addr, &addr_len);
    if (n == -1) {
        perror("recvfrom failed");
        free(response);
        return NULL;
    }

    response[n] = '\0';
    return response;
}

void handle_tcp_request(const char *GSIP, const char *GSPort, const char *request, const char *filename) {
    int tcp_fd;
    struct addrinfo thints, *tres;
    char tbuffer[MAX_BUFFER_SIZE];

    memset(&thints, 0, sizeof(thints));
    thints.ai_family = AF_INET;
    thints.ai_socktype = SOCK_STREAM;
    thints.ai_flags = AI_PASSIVE;

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
        fputs(tbuffer, file); 
        printf("%s", tbuffer);
    }

    if (n < 0) {
        perror("Failed to receive file data");
    }

    fclose(file);
    close(tcp_fd);
    freeaddrinfo(tres);
    printf("\nFile '%s' saved successfully.\n", filename);
}


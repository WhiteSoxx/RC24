#include "../common.h"

int fd, errcode;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr;
char buffer[MAX_BUFFER_SIZE];

char currentPLID[7] = "";
int gamestate = 0;
int trial_number = 1;

void handle_tcp_request(const char *GSIP, const char *GSPort, const char *request, const char *filename);
char* receive_udp_response();

void handle_start_command(const char *PLID, const char *time) {
    if (!validate_plid(PLID) || !validate_play_time(time)) {
        printf("[!] Invalid start command, PLID must be a 6-digit number and time cannot exceed 600 seconds.\n");
        return;
    }

    // Copy PLID to currentPLID
    strncpy(currentPLID, PLID, sizeof(currentPLID) - 1);
    currentPLID[sizeof(currentPLID) - 1] = '\0';

    printf("First PLID: %s\n", currentPLID);
    printf("time: %s\n", time);

    snprintf(buffer, MAX_BUFFER_SIZE, "SNG %s %s\n", currentPLID, time);
    printf("(->) Sending: %s", buffer);

    // Send the start request
    sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);

    char *response = receive_udp_response();
    if (!response) {
        printf("[!] No response from server.\n");
        return;
    }

    printf("(<-) Server Response: %s\n", response);

    if (strcmp(response, "RSG OK\n") == 0) {
        printf("[+] Game started successfully. You can now play!\n");
    } else if (strcmp(response, "RSG NOK\n") == 0) {
        printf("[!] A game is already associated with this PLID.\n");
    } else if (strcmp(response, "RSG ERR\n") == 0) {
        printf("[!] Error starting the game. Please check the input and try again.\n");
    }

    free(response);
}

void handle_try_command(const char *C1, const char *C2, const char *C3, const char *C4) {
    if (strlen(currentPLID) == 0) {
        printf("[!] No active game. Please start a game first.\n");
        return;
    }

    printf("PLID: %s\n", currentPLID);
    snprintf(buffer, MAX_BUFFER_SIZE, "TRY %s %s %s %s %s %d\n", currentPLID, C1, C2, C3, C4, trial_number);
    printf("(->) Sending: %s", buffer);

    sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
    char* response = receive_udp_response();
    if (!response) {
        printf("[!] No response from server.\n");
        return;
    }

    printf("(<-) Server Response: %s\n", response);

    if (strncmp(response, "RTR OK", 6) == 0){
        int trial, nB,nW;
        sscanf(response, "RTR OK %d %d %d", &trial, &nB, &nW);
        printf("[+] Trial %d: %d blacks, %d whites\n", trial, nB, nW);
        trial_number++;
    }
    
    free(response);
}

void handle_debug_command(const char *PLID, const char *time, const char *C1, const char *C2, const char *C3, const char *C4) {
    if (!validate_plid(PLID) || !time || !C1 || !C2 || !C3 || !C4) {
        printf("Invalid debug command. Usage: debug PLID time C1 C2 C3 C4\n");
        return;
    }

    snprintf(buffer, MAX_BUFFER_SIZE, "DBG %s %s %s %s %s %s\n", PLID, time, C1, C2, C3, C4);
    printf("(->) Sending: %s", buffer);
    sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
    char* response = receive_udp_response();
    if (!response) {
        printf("[!] No response from server.\n");
        return;
    }

    printf("(<-) Server Response: %s\n", response);
    free(response);
}

void handle_quit_command(const char *PLID) {
    if (!PLID || !validate_plid(PLID)) {
        printf("Invalid quit command. Usage: quit PLID\n");
        return;
    }

    snprintf(buffer, MAX_BUFFER_SIZE, "QUT %s\n", PLID);
    printf("(->) Sending: %s", buffer);
    sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
    char *response = receive_udp_response();
    if (response) {
        printf("(<-) Server Response: %s\n", response);
        free(response);
    } else {
        printf("[!] No response from server.\n");
    }
}

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

    char input_line[MAX_BUFFER_SIZE];
    while (fgets(input_line, sizeof(input_line), stdin) != NULL) {
        input_line[strcspn(input_line, "\n")] = 0; // Remove newline

        if (strlen(input_line) == 0) {
            printf("Please enter a command.\n");
            continue;
        }

        // Extract the command (the first word)
        char cmd[16];
        int matched = sscanf(input_line, "%15s", cmd);
        if (matched != 1) {
            printf("Please enter a command.\n");
            continue;
        }

        if (strcmp(cmd, "start") == 0) {
            char PLID[7];
            char time_str[16];
            int count = sscanf(input_line, "%*s %6s %15s", PLID, time_str); 
            // %*s ignores the first token (the command)
            if (count == 2) {
                handle_start_command(PLID, time_str);
            } else {
                printf("Invalid start command. Usage: start <PLID> <time>\n");
            }

        } else if (strcmp(cmd, "try") == 0) {
            char C1[2], C2[2], C3[2], C4[2];
            int count = sscanf(input_line, "%*s %1s %1s %1s %1s", C1, C2, C3, C4);
            if (count == 4) {
                handle_try_command(C1, C2, C3, C4);
            } else {
                printf("Invalid try command. Usage: try C1 C2 C3 C4\n");
            }

        } else if (strcmp(cmd, "debug") == 0) {
            char PLID[7], time_str[16], C1[2], C2[2], C3[2], C4[2];
            int count = sscanf(input_line, "%*s %6s %15s %1s %1s %1s %1s", PLID, time_str, C1, C2, C3, C4);
            if (count == 6) {
                handle_debug_command(PLID, time_str, C1, C2, C3, C4);
            } else {
                printf("Invalid debug command. Usage: debug PLID time C1 C2 C3 C4\n");
            }

        } else if (strcmp(cmd, "quit") == 0) {
            char PLID[7];
            int count = sscanf(input_line, "%*s %6s", PLID);
            if (count == 1) {
                handle_quit_command(PLID);
            } else {
                printf("Invalid quit command. Usage: quit PLID\n");
            }

        } else if (strcmp(cmd, "exit") == 0) {
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

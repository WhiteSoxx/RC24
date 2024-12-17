#include "../common.h"

int udp_fd, tcp_fd, errcode;
socklen_t addrlen;
char buffer[MAX_BUFFER_SIZE];
int verbose = 0;

void handle_tcp_connection(int client_fd);
void handle_udp_commands();

int main(int argc, char *argv[]) {
    char GSPort[] = DEFAULT_PORT;
    int yes = 1;

    // FIXME: Possible segmentation fault.
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            strcpy(GSPort, argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        }
    }

    printf("Starting Game Server on port: %s\n", GSPort);

    // ================== SET UP UDP SOCKET ==================
    struct addrinfo hints_udp, *res_udp;
    memset(&hints_udp, 0, sizeof(hints_udp));
    hints_udp.ai_family = AF_INET;
    hints_udp.ai_socktype = SOCK_DGRAM;
    hints_udp.ai_flags = AI_PASSIVE;

    if ((errcode = getaddrinfo(NULL, GSPort, &hints_udp, &res_udp)) != 0) {
        fprintf(stderr, "getaddrinfo (UDP) error: %s\n", gai_strerror(errcode));
        exit(1);
    }

    udp_fd = socket(res_udp->ai_family, res_udp->ai_socktype, res_udp->ai_protocol);
    if (udp_fd == -1) {
        perror("UDP socket creation failed");
        freeaddrinfo(res_udp);
        exit(1);
    }

    if (bind(udp_fd, res_udp->ai_addr, res_udp->ai_addrlen) == -1) {
        perror("UDP bind failed");
        close(udp_fd);
        freeaddrinfo(res_udp);
        exit(1);
    }

    freeaddrinfo(res_udp);
    printf("UDP server bound to port %s\n", GSPort);

    // ================== SET UP TCP SOCKET ==================
    struct addrinfo hints_tcp, *res_tcp;
    memset(&hints_tcp, 0, sizeof(hints_tcp));
    hints_tcp.ai_family = AF_INET;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_flags = AI_PASSIVE;

    if ((errcode = getaddrinfo(NULL, GSPort, &hints_tcp, &res_tcp)) != 0) {
        fprintf(stderr, "getaddrinfo (TCP) error: %s\n", gai_strerror(errcode));
        exit(1);
    }

    tcp_fd = socket(res_tcp->ai_family, res_tcp->ai_socktype, res_tcp->ai_protocol);
    if (tcp_fd == -1) {
        perror("TCP socket creation failed");
        freeaddrinfo(res_tcp);
        exit(1);
    }

    if (bind(tcp_fd, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) {
        perror("TCP bind failed");
        close(tcp_fd);
        freeaddrinfo(res_tcp);
        exit(1);
    }

    if (listen(tcp_fd, 5) == -1) {
        perror("listen failed");
        close(tcp_fd);
        freeaddrinfo(res_tcp);
        exit(1);
    }

    freeaddrinfo(res_tcp);
    printf("TCP server listening on port %s\n", GSPort);

    fd_set read_fds;
    int max_fd = (udp_fd > tcp_fd) ? udp_fd : tcp_fd;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(udp_fd, &read_fds);
        FD_SET(tcp_fd, &read_fds);

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            continue;
        }

        if (FD_ISSET(udp_fd, &read_fds)) {
            handle_udp_commands();
        }

        if (FD_ISSET(tcp_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(tcp_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd == -1) {
                perror("Accept failed");
                continue;
            }

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                close(client_fd);
                continue;
            } else if (pid == 0) {
                close(tcp_fd);
                handle_tcp_connection(client_fd);
                close(client_fd);
                exit(0);
            } else {
                close(client_fd);
            }
        }
    }

    return 0;
}

void handle_udp_commands() {
    struct sockaddr_in addr;
    addrlen = sizeof(addr);
    int n = recvfrom(udp_fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addrlen);

    if (n == -1) {
        perror("recvfrom failed");
        return;
    }

    buffer[n] = '\0';

    if (verbose) {
        printf("UDP Received from %s:%d: %s", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), buffer);
    }

    char *command = strtok(buffer, " ");
    if (command == NULL) return;

    if (strcmp(command, "SNG") == 0) {
        char *PLID = strtok(NULL, " ");
        char *time = strtok(NULL, " ");
        printf("Start command received with PLID: %s, time: %s\n", PLID, time);
    } 
    else if (strcmp(command, "TRY") == 0) {
        printf("TRY command received\n");
    } 

    //FIXME
    else if (strcmp(command, "QUT") == 0) {
        printf("QUIT command received\n");
    } 
    else if (strcmp(command, "DBG") == 0) {
        printf("DEBUG command received\n");
    } 
    else {
        printf("Unknown UDP command\n");
    }
}

void handle_tcp_connection(int client_fd) {
    char buffer[MAX_BUFFER_SIZE];
    int n = recv(client_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) {
        if (n < 0) perror("recv failed");
        return;
    }

    buffer[n] = '\0';
    printf("TCP request: %s\n", buffer);

    if (strncmp(buffer, "STR", 3) == 0) {
        printf("Execute show trials protocol\n");
        FILE *file = fopen("trials.txt", "r");
        if (!file) {
            perror("Failed to open trials file");
            send(client_fd, "ERROR\n", 6, 0);
            return;
        }

        while (fgets(buffer, MAX_BUFFER_SIZE, file)) {
            send(client_fd, buffer, strlen(buffer), 0);
        }

        fclose(file);
    } 
    else if (strncmp(buffer, "SSB", 3) == 0) {
        printf("Execute Scoreboard protocol\n");
        FILE *file = fopen("scoreboard.txt", "r");
        if (!file) {
            perror("Failed to open scoreboard file");
            send(client_fd, "ERROR\n", 6, 0);
            return;
        }

        while (fgets(buffer, MAX_BUFFER_SIZE, file)) {
            send(client_fd, buffer, strlen(buffer), 0);
        }

        fclose(file);
    } 
    else {
        printf("Unknown TCP request\n");
        send(client_fd, "ERROR\n", 6, 0);
    }
}

/**
 * Grupo: 54
 * Nomes:
 *      Gabriel Bispo (1105918)
 *      Guilherme Filipe (1106326)
 * 
 * Turno: RC11L03
 *
 * @brief Implementation of the player application.
 *
 * This file defines the logic for communicating with the GS using UDP and TCP connections.
 * The Player supports the following commands:
 * - start <PLID> <time>
 * - try <C1> <C2> <C3> <C4>
 * - debug <PLID> <time> <C1> <C2> <C3> <C4>
 * - quit
 * - show_trials or st
 * - scoreboard or sb
 * - exit
 *
 * Additionally, if the player receives a SIGINT (Ctrl+C), it will process it
 * like an exit command (performing the quit if a game is ongoing, then exiting).
 * Also, if the player doesn't receive a response from the server in 10 seconds,
 * it stops waiting and returns control to the user.
*/

#include "../common.h"
#include <signal.h>
#include <errno.h>

int fd, errcode;
struct addrinfo hints, *res;
char buffer[MAX_BUFFER_SIZE];

char currentPLID[7] = "";
int gamestate = 0;
int trial_number = 1;
int exit_requested = 0;

/**
 * @brief Resets the player's game state.
 *
 * Resets player variables to their original state, except PLID is not cleared 
 * because we might reuse it. The intention is to reset trial_number and gamestate.
 */
void reset_player(){
    trial_number = 1;
    gamestate = 0;
}

/**
 * @brief Handles signals (e.g., Ctrl+C).
 *
 * If the player presses Ctrl+C, we handle it like the "exit" command:
 * If a game is ongoing, we send quit first, then exit.
 */
void handle_signal(int sig) {
    exit_requested = 1;
}

/**
 * @brief Receives a UDP response from the server with a timeout of 10 seconds.
 *
 * @return char* The response from the server, or NULL if no response was received within 10s.
 */
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
        // Either timed out or error
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // Timeout
            free(response);
            return NULL;
        }
        perror("recvfrom failed");
        free(response);
        return NULL;
    }

    response[n] = '\0';
    return response;
}

/**
 * @brief Handles the "start" command.
 *
 * Sends the "SNG" message to the server to request the start of a new game 
 * for the given PLID and time limit.
 * 
 * @param PLID The 6-digit Player ID.
 * @param time The proposed time for the game.
 */
void handle_start_command(const char *PLID, const char *time) {
    if (!validate_plid(PLID)) {
        printf("[!] Invalid start command, PLID must be a 6-digit number.\n");
        return;
    }

    if (gamestate == 1) {
        printf("[!] You must terminate the current game before starting a new one.\n");
        return;
    }

    int time_int = atoi(time);

    strcpy(currentPLID, PLID);

    snprintf(buffer, MAX_BUFFER_SIZE, "SNG %s %03d\n", currentPLID, time_int);
    printf("(->) Sending: %s", buffer);

    sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);

    char *response = receive_udp_response();
    if (!response) {
        printf("[!] No response from server (timeout or error).\n");
        return;
    }

    printf("(<-) Server Response: %s\n", response);

    if (strcmp(response, "RSG OK\n") == 0) {
        trial_number = 1;
        gamestate = 1;
        printf("[+] Game started successfully. You can now play! You have %d seconds!\n", time_int);

    } else if (strcmp(response, "RSG NOK\n") == 0) {
        printf("[!] A game is already associated with this PLID.\n");
    } else if (strcmp(response, "RSG ERR\n") == 0) {
        printf("[!] Error starting the game. Please check the input and try again.\n");
        reset_player();
    }

    free(response);
}

/**
 * @brief Handles the "try" command.
 *
 * Sends the "TRY" command to the Game Server with the player's 4-color guess.
 * 
 * @param C1 Color 1 of the guess.
 * @param C2 Color 2 of the guess.
 * @param C3 Color 3 of the guess.
 * @param C4 Color 4 of the guess.
 */
void handle_try_command(const char *C1, const char *C2, const char *C3, const char *C4) {
    if (gamestate == 0) {
        printf("[!] No active game. Please start a game first.\n");
        return;
    }

    printf("PLID: %s\n", currentPLID);
    snprintf(buffer, MAX_BUFFER_SIZE, "TRY %s %s %s %s %s %d\n", currentPLID, C1, C2, C3, C4, trial_number);
    printf("(->) Sending: %s", buffer);

    sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
    char* response = receive_udp_response();
    if (!response) {
        printf("[!] No response from server (timeout or error).\n");
        return;
    }

    printf("(<-) Server Response: %s\n", response);

    if (strncmp(response, "RTR OK", 6) == 0){
        int trial, nB, nW;
        sscanf(response, "RTR OK %d %d %d", &trial, &nB, &nW);
        printf("[+] TRIAL %d: nB = %d, nW = %d\n", trial, nB, nW);

        if(nB == 4){
            printf("[+] WELL DONE! You guessed the key in %d trials!\n", trial_number);
            reset_player();
        }

        trial_number++;

    } else if (strncmp(response, "RTR NOK", 7) == 0) {
        printf("[!] No ongoing game for this player. You need to start a game.\n");

    } else if (strncmp(response, "RTR ERR", 7) == 0) {
        printf("[!] Error: Syntax incorrect, PLID invalid or color sequence is not valid.\n");

    } else if (strncmp(response, "RTR DUP", 7) == 0) {
        printf("[!] Duplicate attempt. Try a different combination.\n");

    } else if (strncmp(response, "RTR INV", 7) == 0) {
        printf("[!] Invalid trial logic. Please try again.\n");

    } else if (strncmp(response, "RTR ENT", 7) == 0) {
        char c1, c2, c3, c4;
        sscanf(response, "RTR ENT %c %c %c %c", &c1, &c2, &c3, &c4);
        printf("[!] Game over! The secret key was: %c %c %c %c\n", c1, c2, c3, c4);
        reset_player();

    } else if (strncmp(response, "RTR ETM", 7) == 0) {
        char c1, c2, c3, c4;
        sscanf(response, "RTR ETM %c %c %c %c", &c1, &c2, &c3, &c4);
        printf("[!] Time is up! The secret key was: %c %c %c %c\n", c1, c2, c3, c4);
        reset_player();

    } else {
        printf("[!] Unrecognized response from the server.\n");
    }

    free(response);
}

/**
 * @brief Handles the "debug" command.
 *
 * Sends the "DBG" command to the server to start a new debug game.
 */
void handle_debug_command(const char *PLID, const char *time, const char *C1, const char *C2, const char *C3, const char *C4) {
    if (!validate_plid(PLID) || !time || !C1 || !C2 || !C3 || !C4) {
        printf("Invalid debug command. Usage: debug PLID time C1 C2 C3 C4\n");
        return;
    }

    if (gamestate == 1) {
        printf("[!] You must terminate the game before starting a new one.\n");
        return;
    }

    strcpy(currentPLID, PLID);

    snprintf(buffer, MAX_BUFFER_SIZE, "DBG %s %s %s %s %s %s\n", PLID, time, C1, C2, C3, C4);
    sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);

    char* response = receive_udp_response();
    if (!response) {
        printf("[!] No response from server (timeout or error).\n");
        return;
    }

    if (strcmp(response, "RDB OK\n") == 0) {
        int time_int = atoi(time);
        trial_number = 1;
        gamestate = 1;
        printf("[+] Debug Game started successfully. You have %d seconds!\n", time_int);

    } else if(strcmp(response, "RDB NOK\n") == 0){
        printf("[!] This player ID already has a game running!\n");

    } else if (strcmp(response, "RDB ERR\n") == 0) {
        printf("[!] Error: PLID, color sequence or time not valid!\n");
        reset_player();
    }
    printf("(<-) Server Response: %s\n", response);
    free(response);
}

/**
 * @brief Handles the "quit" command.
 *
 * Sends the "QUT" command to the Game Server to terminate an active game.
 */
void handle_quit_command() {
    snprintf(buffer, MAX_BUFFER_SIZE, "QUT %s\n", currentPLID);
    printf("(->) Sending: %s", buffer);
    sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
    char *response = receive_udp_response();
    
    if (!response) {
        printf("[!] No response from server (timeout or error).\n");
        reset_player();
        return;
    }

    printf("(<-) Server Response: %s\n", response);

    if (strncmp(response,"RQT OK", 6)==0) {
        char c1, c2, c3, c4;
        sscanf(response, "RQT OK %c %c %c %c", &c1, &c2, &c3, &c4);
        printf("[+] Quit game successfully! The secret key was: %c %c %c %c.\n", c1, c2, c3, c4);
        reset_player();

    } else if(strncmp(response, "RQT NOK", 7) == 0){
        reset_player();
        printf("[!] There is no game associated with this player.\n");

    } else if(strncmp(response, "RQT ERR", 7) == 0){
        printf("[!] Error processing the request. Try again.\n");

    } else {
        printf("[!] Error: Unable to do this operation.\n");
    }

    free(response);
}

/**
 * @brief Handles the "show_trials" command.
 *
 * Sends the "STR PLID" command via TCP. Receives the file and prints it.
 */
void handle_show_trials_command(const char *GSIP, const char *GSPort) {
    int tcp_fd;
    struct addrinfo thints, *tres;
    char tbuffer[MAX_BUFFER_SIZE];
    char status[4], fname[64];
    long fsize = 0;

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

    freeaddrinfo(tres);

    snprintf(tbuffer, MAX_BUFFER_SIZE, "STR %s\n", currentPLID);
    if (send(tcp_fd, tbuffer, strlen(tbuffer), 0) == -1) {
        perror("Failed to send STR command");
        close(tcp_fd);
        return;
    }

    int total_read = 0;
    memset(tbuffer, 0, sizeof(tbuffer));
    while (1) {
        int r = recv(tcp_fd, tbuffer + total_read, 1, 0);
        if (r <= 0) {
            perror("recv failed while reading header");
            close(tcp_fd);
            return;
        }
        if (tbuffer[total_read] == '\n') {
            tbuffer[total_read] = '\0'; 
            break;
        }
        total_read++;
        if (total_read >= (int)sizeof(tbuffer)-1) {
            close(tcp_fd);
            return;
        }
    }

    if (sscanf(tbuffer, "RST %3s %63s %ld ", status, fname, &fsize) < 2) {
        printf("[!] Error: show trials not available at this stage.\n");
        close(tcp_fd);
        return;
    }

    if (strcmp(status, "NOK") == 0) {
        printf("[!] No game available or error occurred for PLID %s.\n", currentPLID);
        close(tcp_fd);
        return;
    }

    if (strcmp(status, "ACT") != 0 && strcmp(status, "FIN") != 0) {
        printf("[!] Unrecognized status: %s\n", status);
        close(tcp_fd);
        return;
    }

    FILE *file = fopen(fname, "wb");
    if (!file) {
        perror("Failed to open local file for writing");
        close(tcp_fd);
        return;
    }

    long bytes_received = 0;
    while (bytes_received < fsize) {
        int to_read = (fsize - bytes_received) < MAX_BUFFER_SIZE ? (int)(fsize - bytes_received) : MAX_BUFFER_SIZE;
        int n = recv(tcp_fd, tbuffer, to_read, 0);
        if (n <= 0) {
            perror("Failed to receive file data");
            fclose(file);
            close(tcp_fd);
            return;
        }
        fwrite(tbuffer, 1, n, file);
        bytes_received += n;
    }

    fclose(file);
    close(tcp_fd);

    printf("[+] Received file '%s' (%ld bytes) from server.\n", fname, fsize);

    if (strcmp(status, "FIN") == 0) {
        printf("[+] This game is finished. You can start a new game.\n");
        reset_player();
    } else {
        printf("[+] Ongoing game summary received. You may continue playing.\n");
    }

    // Print the file content to the player
    file = fopen(fname, "r");
    if (!file) {
        perror("Failed to open received trials file for reading");
        return;
    }
    printf("=======================================================\n");
    while (fgets(tbuffer, sizeof(tbuffer), file)) {
        printf("%s", tbuffer);
    }
    fclose(file);
    printf("=======================================================\n");

    
}

/**
 * @brief Handles the "scoreboard" command.
 *
 * Sends the "SSB" command over TCP and receives the scoreboard file.
 */
void handle_scoreboard_command(const char *GSIP, const char *GSPort) {
    int tcp_fd;
    struct addrinfo thints, *tres;
    char tbuffer[MAX_BUFFER_SIZE];

    char status[6]; 
    char fname[64];
    long fsize = 0;

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

    freeaddrinfo(tres);

    snprintf(tbuffer, MAX_BUFFER_SIZE, "SSB\n");
    if (send(tcp_fd, tbuffer, strlen(tbuffer), 0) == -1) {
        perror("Failed to send SSB command");
        close(tcp_fd);
        return;
    }

    int total_read = 0;
    memset(tbuffer, 0, sizeof(tbuffer));

    while (1) {
        int r = recv(tcp_fd, tbuffer + total_read, 1, 0);
        if (r <= 0) {
            perror("recv failed while reading scoreboard header");
            close(tcp_fd);
            return;
        }
        if (tbuffer[total_read] == '\n') {
            tbuffer[total_read] = '\0';
            break;
        }
        total_read++;
        if (total_read >= (int)sizeof(tbuffer)-1) {
            close(tcp_fd);
            return;
        }
    }

    int fields = sscanf(tbuffer, "RSS %5s %63s %ld", status, fname, &fsize);
    if (strcmp(status, "EMPTY") == 0) {
        printf("[!] The scoreboard is empty. No winners yet.\n");
        close(tcp_fd);
        return;
    } else if (strcmp(status, "OK") == 0) {
        if (fields < 3 || ) {
            printf("[!] Invalid response format from server.\n");
            close(tcp_fd);
            return;
        }

        FILE *file = fopen(fname, "wb");
        if (!file) {
            perror("Failed to open local file for writing");
            close(tcp_fd);
            return;
        }

        long bytes_received = 0;
        while (bytes_received < fsize) {
            int to_read = (fsize - bytes_received) < MAX_BUFFER_SIZE ? (int)(fsize - bytes_received) : MAX_BUFFER_SIZE;
            int n = recv(tcp_fd, tbuffer, to_read, 0);
            int n = fread(tbuffer, 1, sizeof(tbuffer), file);
            if (n <= 0) {
                perror("Failed to receive scoreboard file data");
                fclose(file);
                close(tcp_fd);
                return;
            }
            fwrite(tbuffer, 1, n, file);
            bytes_received += n;
        }

        fclose(file);
        close(tcp_fd);

        printf("[+] Received scoreboard file '%s' (%ld bytes) from server.\n", fname, fsize);
        printf("[+] File '%s' saved successfully.\n", fname);

        FILE *read_file = fopen(fname, "r");
        if (!read_file) {
            perror("Failed to open scoreboard file for reading");
            return;
        }

        printf("===== Top 10 Scores =====\n");
        while (fgets(tbuffer, sizeof(tbuffer), read_file)) {
            printf("%s", tbuffer);
        }
        printf("\n=========================\n");
        fclose(read_file);

    } else {
        printf("[!] Error: %s\n", status);
        close(tcp_fd);
    }
}

/**
 * @brief Main function of the Player application.
 *
 * Initializes the player's UDP connection to the Game Server (GS) and
 * processes commands. On Ctrl+C, it acts like "exit".
 */
int main(int argc, char *argv[]) {
    char GSIP[256] = DEFAULT_IP;
    char GSPort[16] = DEFAULT_PORT;

    // Handle Ctrl+C
    signal(SIGINT, handle_signal);

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

    // Set a 10-second timeout for UDP responses
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt failed");
        exit(1);
    }

    // Set up the UDP address information
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errcode = getaddrinfo(GSIP, GSPort, &hints, &res);
    if (errcode != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errcode));
        exit(1);
    }

    char input_line[MAX_BUFFER_SIZE];
    while (1) {
        if (exit_requested) {
            // Ctrl+C pressed
            // Act like "exit" command
            if (gamestate == 1) {
                handle_quit_command();
            }
            freeaddrinfo(res);
            close(fd);
            printf("\nExiting player client.\n");
            exit(0);
        }

        printf("> ");
        fflush(stdout);
        if (!fgets(input_line, sizeof(input_line), stdin)) {
            // EOF or error
            if (feof(stdin)) {
                // If input closed, act like exit
                if (gamestate == 1) {
                    handle_quit_command();
                }
                freeaddrinfo(res);
                close(fd);
                printf("Exiting player client.\n");
                exit(0);
            }
            continue;
        }

        input_line[strcspn(input_line, "\n")] = 0; // Remove newline

        if (strlen(input_line) == 0) {
            printf("Please enter a command.\n");
            continue;
        }

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
            handle_quit_command();

        } else if (strcmp(cmd, "exit") == 0) {
            handle_quit_command();
            freeaddrinfo(res);
            close(fd);
            printf("Exiting player client.\n");
            exit(0);

        } else if (strcmp(cmd, "show_trials") == 0 || strcmp(cmd, "st") == 0) {
            handle_show_trials_command(GSIP, GSPort);

        } else if (strcmp(cmd, "scoreboard") == 0 || strcmp(cmd, "sb") == 0) {
            handle_scoreboard_command(GSIP, GSPort);

        } else {
            printf("Unknown command\n");
        }
    }

    // Should never reach here
    freeaddrinfo(res);
    close(fd);
    return 0;
}

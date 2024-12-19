#include "../common.h"
#include "GS.h"
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

PlayerGame *player_games = NULL; // Head of the player linked list

int udp_fd, tcp_fd, errcode;
socklen_t addrlen;
char buffer[MAX_BUFFER_SIZE];
int verbose = 0;

PlayerGame *get_game(const char *PLID) {
    PlayerGame *current = player_games;
    while (current != NULL) {
        if (strcmp(current->PLID, PLID) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Add or create a player game
PlayerGame *find_or_create_game(const char *PLID) {
    PlayerGame *game = get_game(PLID);
    if (game) return game;

    PlayerGame *new_game = (PlayerGame *)malloc(sizeof(PlayerGame));
    if (!new_game) {
        perror("Memory allocation failed");
        return NULL;
    }

    strcpy(new_game->PLID, PLID);
    memset(new_game->secret_key, 0, sizeof(new_game->secret_key));
    new_game->remaining_time = 0;
    new_game->current_trial = 1;
    new_game->expected_trial = 1;
    new_game->next = player_games;
    new_game->elapsed_time = 0;
    new_game->start_time = time(NULL);
    new_game->last_update_time = time(NULL);
    player_games = new_game;

    return new_game;
}

void end_game_file(const char *PLID, const char *status, time_t start_time) {
    char filename[64];
    snprintf(filename, sizeof(filename), "GAMES/%s.txt", PLID);

    FILE *file = fopen(filename, "a");
    if (!file) {
        perror("Failed to open game file for final update");
        return;
    }

    time_t now = time(NULL);
    int game_duration = (int)(now - start_time);
    struct tm *t = localtime(&now);

    char end_date[11];
    char end_time[9];
    strftime(end_date, sizeof(end_date), "%Y-%m-%d", t);
    strftime(end_time, sizeof(end_time), "%H:%M:%S", t);


    fprintf(file, "%s %s %d\n", end_date, end_time, game_duration);
    fclose(file);


    // Create the GAMES/PLID directory if it doesn't exist
    char player_dir[64];
    snprintf(player_dir, sizeof(player_dir), "GAMES/%s", PLID);

    struct stat st = {0};
    if (stat(player_dir, &st) == -1) {
        if (mkdir(player_dir, 0777) == -1) {
            perror("Failed to create player directory");
            return;
        }
        printf("[*] Created player directory: %s\n", player_dir);
    }

    // Rename the file to the format: YYYYMMDD_HHMMSS_(code).txt
    char new_filename[128];
    char end_datetime[32];
    strftime(end_datetime, sizeof(end_datetime), "%Y%m%d %H%M%S", t);
    snprintf(new_filename, sizeof(new_filename), "GAMES/%s/%s_%s.txt", PLID, end_datetime, status);

    if (rename(filename, new_filename) == -1) {
        perror("Failed to move and rename game file");
        return;
    }

    printf("[*] Game file moved to: %s\n", new_filename);
}

void remove_game(const char *PLID, const char *status) {
    PlayerGame *current = player_games;
    PlayerGame *previous = NULL;

    while (current != NULL) {
        if (strcmp(current->PLID, PLID) == 0) {
            if (previous == NULL) {
                player_games = current->next; 
            } else {
                previous->next = current->next; 
            }

            end_game_file(PLID,status,current->start_time);
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

void create_game_file(const char *PLID, const char *time_str, const char *secret_key, const char *mode){
    char filename[64];
    snprintf(filename, sizeof(filename), "GAMES/%s.txt", PLID);

    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Failed to create game file for player");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    fprintf(file, "%s %s %s %s %s %ld\n", PLID, mode, secret_key, time_str, timestamp, (long)now);
    fclose(file);
}

void update_game_file(const char *PLID, const char *guess, int time_elapsed, int nB, int nW){
    char filename[64];
    snprintf(filename, sizeof(filename), "GAMES/%s.txt", PLID);

    FILE *file = fopen(filename, "a");
    if (!file) {
        perror("Failed to open game file for player");
        return;
    }

    fprintf(file, "T: %s %d %d %d\n", guess, nB, nW, time_elapsed);
    fclose(file);
}


/**
 * @brief Processes the "START" command from a player.
 * 
 * This function processes a player's request to start a new game.
 * It validates the player's input and creates a new game if one does not already exist for the player.
 * If the player already has an active game, it responds with **RSG NOK**.
 * If validation fails (e.g., invalid PLID or time), it responds with **RSG ERR**.
 * If successful, a new game is created, and the server responds with **RSG OK**.
 * 
 * @param addr The address of the player sending the command, used to send back the response.
 */
void process_start_command(struct sockaddr_in *addr) {
    char PLID[7], time_str[16];
    int sscount = sscanf(buffer, "SNG %6s %15s", PLID, time_str);
    if (sscount != 2 || !validate_plid(PLID) || !validate_play_time(time_str)) {
        sendto(udp_fd, "RSG ERR\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    PlayerGame *game = get_game(PLID);
    if (game) {
        sendto(udp_fd, "RSG NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
    } else {
        game = find_or_create_game(PLID);
        game->remaining_time = atoi(time_str);
        generate_secret_key(game->secret_key);
        create_game_file(PLID, time_str, game->secret_key,"P");
        sendto(udp_fd, "RSG OK\n", 7, 0, (struct sockaddr *)addr, addrlen);
    }
}

/**
 * @brief Processes the "TRY" command from a player.
 */
void process_try_command(struct sockaddr_in *addr) {
    char PLID[7], C1[2], C2[2], C3[2], C4[2];
    int nT;

    int scanned = sscanf(buffer, "TRY %6s %1s %1s %1s %1s %d", PLID, C1, C2, C3, C4, &nT);
    if (scanned != 6) {
        sendto(udp_fd, "RTR ERR\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }
    
    if (!validate_plid(PLID) || !validate_color_sequence(C1, C2, C3, C4)) {
        sendto(udp_fd, "RTR ERR\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    PlayerGame *game = get_game(PLID);
    if (!game) {
        sendto(udp_fd, "RTR NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    printf("[DEBUGGING]: The secret key is: %s\n", game->secret_key);

    if (game->current_trial > MAX_TRIALS) {
        char formatted_key[8];
        format_secret_key(formatted_key, game->secret_key);
        snprintf(buffer, MAX_BUFFER_SIZE, "RTR ENT %s\n", formatted_key);
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
        remove_game(PLID,FAIL);
        return;
    }

    // Handles remaining time
    time_t current_time = time(NULL);
    int time_elapsed = (int)(current_time - game->last_update_time);
    game->elapsed_time += time_elapsed;
    game->remaining_time -= time_elapsed;
    game->last_update_time = current_time;


    if (game->remaining_time <= 0) {
        char formatted_key[8];
        format_secret_key(formatted_key, game->secret_key);
        snprintf(buffer, MAX_BUFFER_SIZE, "RTR ETM %s\n", formatted_key);
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
        remove_game(PLID,TIMEOUT);
        return;
    }

    char guess[COLOR_SEQUENCE_LEN + 1] = {C1[0], C2[0], C3[0], C4[0], '\0'};


    if (game->current_trial > 1 && strcmp(guess, game->last_guess) == 0) {
        snprintf(buffer, MAX_BUFFER_SIZE, "RTR DUP\n");
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    int nB, nW;

    calculate_nB_nW(guess, game->secret_key, &nB, &nW);
    
    if (game->current_trial != nT || (strcmp(guess, game->last_guess) != 0 && nT == game->expected_trial - 1)) {
        snprintf(buffer, MAX_BUFFER_SIZE, "RTR INV\n");
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    if (game->current_trial >= MAX_TRIALS && nB != 4) {
        snprintf(buffer, MAX_BUFFER_SIZE, "RTR ENT %s\n", game->secret_key);
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
        remove_game(PLID, FAIL);
    
    } else {
        strncpy(game->last_guess, guess, COLOR_SEQUENCE_LEN);
        update_game_file(PLID, guess, game->elapsed_time, nB, nW);
        snprintf(buffer, MAX_BUFFER_SIZE, "RTR OK %d %d %d\n", game->current_trial, nB, nW);
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);

        if (nB == 4){
            remove_game(PLID, WIN);
            return;
        }

        if(!(strcmp(guess, game->last_guess) == 0 && nT == game->expected_trial - 1)){
            game->current_trial++;
            game->expected_trial = game->current_trial;
        }
    }
}

/**
 * @brief Handles the "DBG" (debug) command from a player.
 */
void process_debug_command(struct sockaddr_in *addr) {
    char PLID[7], time_str[16], C1[2], C2[2], C3[2], C4[2];

    int scanned = sscanf(buffer, "DBG %6s %15s %1s %1s %1s %1s", PLID, time_str, C1, C2, C3, C4);
    if (scanned != 6) {
        sendto(udp_fd, "RDB ERR\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    if (!validate_plid(PLID) || !validate_play_time(time_str) || !validate_color_sequence(C1, C2, C3, C4)) {
        sendto(udp_fd, "RDB ERR\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    PlayerGame *game = get_game(PLID);
    if (game) {
        sendto(udp_fd, "RDB NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
    } else {
        game = find_or_create_game(PLID);
        game->remaining_time = atoi(time_str);
        snprintf(game->secret_key, COLOR_SEQUENCE_LEN + 1, "%s%s%s%s", C1, C2, C3, C4);
        create_game_file(PLID, time_str, game->secret_key,"D");
        sendto(udp_fd, "RDB OK\n", 7, 0, (struct sockaddr *)addr, addrlen);
    }
}

void process_quit_command(struct sockaddr_in *addr) {
    char PLID[7];
    sscanf(buffer, "QUT %6s", PLID);

    /*
    if (!validate_plid(PLID)) {
        sendto(udp_fd, "RQT ERR\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return; 
    }
    */

    PlayerGame *game = get_game(PLID);
    if (!game) {
        sendto(udp_fd, "RQT NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
    } else if (game) {
        char formatted_key[8];
        format_secret_key(formatted_key, game->secret_key);
        snprintf(buffer, MAX_BUFFER_SIZE, "RQT OK %s\n", formatted_key);
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
        remove_game(PLID, QUIT);
    } else {
        sendto(udp_fd, "RQT ERR\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }
}

void process_show_trials_command(int client_fd) {
    char PLID[7];
    int scanned = sscanf(buffer, "STR %6s", PLID);
    if (scanned != 1 || !validate_plid(PLID)) {
        send(client_fd, "RST NOK\n", 8, 0);
        return;
    }

    PlayerGame *game = get_game(PLID);

    if (game) {
        char filename[64];
        snprintf(filename, sizeof(filename), "trials_%s.txt", PLID);
        FILE *file = fopen(filename, "w");
        
        if (!file) {
            perror("Failed to create active game file");
            send(client_fd, "RST NOK\n", 8, 0);
            return;
        }

        fprintf(file, "Active game for player %s\n", PLID);
        fprintf(file, "Remaining Time: %d seconds\n", game->remaining_time);
        fclose(file);

        send_file_to_client(client_fd, "ACT", filename);
    /*} else {
        // Check for the most recent finished game
        char latest_game_file[64];
        if (FindLastGame(PLID, latest_game_file)) {
            send_file_to_client(client_fd, "FIN", latest_game_file);
        } else {
            send(client_fd, "RST NOK\n", 8, 0);
        }*/
    }
}

void send_file_to_client(int client_fd, const char *status, const char *filepath) {
    
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Failed to open file to send");
        send(client_fd, "RST NOK\n", 8, 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    char header[MAX_BUFFER_SIZE];
    snprintf(header, sizeof(header), "RST %s %s %ld\n", status, filepath, filesize);
    send(client_fd, header, strlen(header), 0);

    char buffer[MAX_BUFFER_SIZE];
    size_t nread;
    while ((nread = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_fd, buffer, nread, 0);
    }

    fclose(file);
}

void handle_udp_commands() {
    struct sockaddr_in addr;
    addrlen = sizeof(addr);

    memset(buffer, 0, MAX_BUFFER_SIZE);

    int n = recvfrom(udp_fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
    if (n == -1) {
        perror("recvfrom failed");
        return;
    }

    if (verbose) {
        printf("UDP Received from %s:%d: %s\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), buffer);
    }

    buffer[n] = '\0';

    char command[4];
    int cmd_scanned = sscanf(buffer, "%3s", command);
    if (cmd_scanned != 1) return;

    if (strcmp(command, "SNG") == 0) {
        process_start_command(&addr);
    } else if (strcmp(command, "TRY") == 0) {
        process_try_command(&addr);
    } else if (strcmp(command, "DBG") == 0) {
        process_debug_command(&addr);
    } else if (strcmp(command,"QUT") == 0) {
        process_quit_command(&addr);
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

void generate_secret_key(char *secret_key) {
    const char colors[] = {'R', 'G', 'B', 'Y', 'O', 'P'};
    srand(time(NULL));
    for (int i = 0; i < COLOR_SEQUENCE_LEN; i++) {
        secret_key[i] = colors[rand() % 6];
    }
    secret_key[COLOR_SEQUENCE_LEN] = '\0';
}

/**
 * @brief Calculates the number of black (nB) and white (nW) pegs.
 */
void calculate_nB_nW(const char *guess, const char *secret_key, int *nB, int *nW) {
    *nB = *nW = 0;

    char temp_guess[COLOR_SEQUENCE_LEN + 1];
    char temp_secret_key[COLOR_SEQUENCE_LEN + 1];

    strncpy(temp_guess, guess, COLOR_SEQUENCE_LEN);
    strncpy(temp_secret_key, secret_key, COLOR_SEQUENCE_LEN);
    
    temp_guess[COLOR_SEQUENCE_LEN] = '\0';
    temp_secret_key[COLOR_SEQUENCE_LEN] = '\0';

    for (int i = 0; i < COLOR_SEQUENCE_LEN; i++) {
        if (temp_guess[i] == temp_secret_key[i]) {
            (*nB)++;
            temp_guess[i] = temp_secret_key[i] = '*';
        }
    }

    for (int i = 0; i < COLOR_SEQUENCE_LEN; i++) {
        if (temp_guess[i] != '*') {
            for (int j = 0; j < COLOR_SEQUENCE_LEN; j++) {
                if (temp_guess[i] == temp_secret_key[j] && temp_secret_key[j] != '*') {
                    (*nW)++;
                    temp_secret_key[j] = '*';
                    break;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    char GSPort[] = DEFAULT_PORT;
    signal(SIGINT, cleanup_and_exit);

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

    int yes = 1;
    if (setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt failed");
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

    if (setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt failed");
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

void format_secret_key(char *formatted_key, const char *secret_key) {
    snprintf(formatted_key, 8, "%c %c %c %c", 
             secret_key[0], secret_key[1], secret_key[2], secret_key[3]);
}

void cleanup_and_exit(int signum) {
    printf("\nShutting down server gracefully...\n");

    // Close UDP and TCP file descriptors
    if (udp_fd > 0) close(udp_fd);
    if (tcp_fd > 0) close(tcp_fd);

    // Free the linked list of PlayerGame
    PlayerGame *current = player_games;
    while (current != NULL) {
        PlayerGame *next = current->next;
        free(current);
        current = next;
    }

    printf("Resources cleaned up successfully. Exiting.\n");
    exit(0);
}

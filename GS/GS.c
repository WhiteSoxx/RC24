#include "../common.h"
#include "GS.h"

PlayerGame *player_games = NULL; // Head of the player linked list

int udp_fd, tcp_fd, errcode;
socklen_t addrlen;
char buffer[MAX_BUFFER_SIZE];
int verbose = 0;

typedef struct {
    int SSS;            // Score (number of attempts)
    char PLID[7];
    char secret_key[5];
    int total_plays;
} ScoreEntry;

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

PlayerGame *find_or_create_game(const char *PLID, const char *time_str, const char *mode) {
    PlayerGame *game = get_game(PLID);
    if (game) return game;

    PlayerGame *new_game = (PlayerGame *)malloc(sizeof(PlayerGame));
    if (!new_game) {
        perror("Memory allocation failed");
        return NULL;
    }
    
    strcpy(new_game->PLID, PLID);
    strcpy(new_game->mode, mode);
    memset(new_game->secret_key, 0, sizeof(new_game->secret_key));
    
    new_game->remaining_time = 0;
    new_game->current_trial = 1;
    new_game->expected_trial = 1;
    new_game->next = player_games;
    new_game->total_duration = atoi(time_str);
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

    char new_filename[128];
    char end_datetime[32];
    strftime(end_datetime, sizeof(end_datetime), "%Y%m%d_%H%M%S", t);
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

            end_game_file(PLID, status, current->start_time);
            

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

// Checks and updates the time.
// If time up:
//  - If TRY: send RTR ETM ... and remove game
//  - Otherwise: just remove game, no response
// Returns: -1 time up, 0 no ongoing game, 1 ongoing and updated
int check_and_update_game_time(const char *PLID, struct sockaddr_in *addr, int client_fd, int is_udp, const char *command_type) {
    PlayerGame *game = get_game(PLID);
    if (!game) {
        return 0; 
    }

    time_t current_time = time(NULL);
    int time_elapsed = (int)(current_time - game->last_update_time);
    game->elapsed_time += time_elapsed;
    game->remaining_time -= time_elapsed;
    game->last_update_time = current_time;

    if (game->remaining_time <= 0) {
        char formatted_key[8];
        format_secret_key(formatted_key, game->secret_key);
        if (strcmp(command_type, "TRY") == 0) {
            // SEND RTR ETM
            if (is_udp && addr != NULL) {
                snprintf(buffer, MAX_BUFFER_SIZE, "RTR ETM %s\n", formatted_key);
                sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
            }
        }
        remove_game(PLID, TIMEOUT);
        return -1; // time up
    }

    return 1; // ongoing
}

void process_start_command(struct sockaddr_in *addr) {
    char PLID[7], time_str[16];
    int sscount = sscanf(buffer, "SNG %6s %15s", PLID, time_str);
    if (sscount != 2 || !validate_plid(PLID) || !validate_play_time(time_str)) {
        sendto(udp_fd, "RSG ERR\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    int time_status = check_and_update_game_time(PLID, addr, -1, 1, "SNG");
    if (time_status == -1) {
        // Time up. Just create new game anyway as per code given (seems odd, but following existing logic)
        PlayerGame *game;
        game = find_or_create_game(PLID, time_str, "PLAY");
        game->remaining_time = atoi(time_str);
        generate_secret_key(game->secret_key);
        //create_game_file(PLID, time_str, game->secret_key,"PLAY");
        sendto(udp_fd, "RSG OK\n", 7, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    PlayerGame *game = get_game(PLID);
    if (game) {
        sendto(udp_fd, "RSG NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
    } else {
        game = find_or_create_game(PLID, time_str, "PLAY");
        game->remaining_time = atoi(time_str);
        generate_secret_key(game->secret_key);
        create_game_file(PLID, time_str, game->secret_key,"PLAY");
        sendto(udp_fd, "RSG OK\n", 7, 0, (struct sockaddr *)addr, addrlen);
    }
}

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

    int time_status = check_and_update_game_time(PLID, addr, -1, 1, "TRY");
    if (time_status == -1) {
        return; // time up response already sent if needed
    }

    PlayerGame *game = get_game(PLID);
    if (!game) {
        sendto(udp_fd, "RTR NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    if (game->current_trial > MAX_TRIALS) {
        char formatted_key[8];
        format_secret_key(formatted_key, game->secret_key);
        snprintf(buffer, MAX_BUFFER_SIZE, "RTR ENT %s\n", formatted_key);
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
        remove_game(PLID,FAIL);
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
            create_score_file(game);
            remove_game(PLID, WIN);
            return;
        }

        if(!(strcmp(guess, game->last_guess) == 0 && nT == game->expected_trial - 1)){
            game->current_trial++;
            game->expected_trial = game->current_trial;
        }
    }
}

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

    int time_status = check_and_update_game_time(PLID, addr, -1, 1, "DBG");
    if (time_status == -1) {
        // time up, no response
        return;
    }

    PlayerGame *game = get_game(PLID);
    if (game) {
        sendto(udp_fd, "RDB NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
    } else {
        game = find_or_create_game(PLID, time_str, "DEBUG");
        game->remaining_time = atoi(time_str);
        snprintf(game->secret_key, COLOR_SEQUENCE_LEN + 1, "%s%s%s%s", C1, C2, C3, C4);
        create_game_file(PLID, time_str, game->secret_key,"DEBUG");
        sendto(udp_fd, "RDB OK\n", 7, 0, (struct sockaddr *)addr, addrlen);
    }
}

void process_quit_command(struct sockaddr_in *addr) {
    char PLID[7];
    sscanf(buffer, "QUT %6s", PLID);

    int time_status = check_and_update_game_time(PLID, addr, -1, 1, "QUT");
    if (time_status == -1) {
        // time up, no response
        sendto(udp_fd, "RQT NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
        return;
    }

    PlayerGame *game = get_game(PLID);
    if (!game) {
        sendto(udp_fd, "RQT NOK\n", 8, 0, (struct sockaddr *)addr, addrlen);
    } else {
        char formatted_key[8];
        format_secret_key(formatted_key, game->secret_key);
        snprintf(buffer, MAX_BUFFER_SIZE, "RQT OK %s\n", formatted_key);
        sendto(udp_fd, buffer, strlen(buffer), 0, (struct sockaddr *)addr, addrlen);
        remove_game(PLID, QUIT);
    }
}

int extract_trials_from_game_file(const char *source_filename, const char *output_filename, PlayerGame *game) {
    FILE *source_file = fopen(source_filename, "r");
    FILE *output_file = fopen(output_filename, "w");

    if (!source_file || !output_file) {
        perror("Failed to open files for trial extraction");
        if (source_file) fclose(source_file);
        if (output_file) fclose(output_file);
        return 0;
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), source_file)) {
        if (strncmp(line, "T: ", 3) == 0) {
            char C1, C2, C3, C4;
            int nB, nW, tElapsed;
            if (sscanf(line, "T: %c%c%c%c %d %d %d", &C1, &C2, &C3, &C4, &nB, &nW, &tElapsed) == 7) {
                fprintf(output_file, "%c %c %c %c %d %d\n", C1, C2, C3, C4, nB, nW);
            }
        }
    }

    if (game) {
        fprintf(output_file, "Remaining Time: %d seconds\n", game->remaining_time);
    }

    fclose(source_file);
    fclose(output_file);
    return 1;
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

    char *fname = strrchr(filepath, '/');
    fname = fname ? fname + 1 : (char *)filepath;

    char header[MAX_BUFFER_SIZE];
    snprintf(header, sizeof(header), "RST %s %s %ld\n", status, fname, filesize);
    send(client_fd, header, strlen(header), 0);

    char file_buffer[MAX_BUFFER_SIZE];
    size_t nread;
    while ((nread = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        send(client_fd, file_buffer, nread, 0);
    }

    fclose(file);
}

int compare_scores(const void *a, const void *b) {
    const ScoreEntry *sa = (const ScoreEntry*)a;
    const ScoreEntry *sb = (const ScoreEntry*)b;
    return sb->SSS - sa->SSS; // ascending order by attempts
}

void process_scoreboard_command(int client_fd) {
    struct dirent **filelist;
    int n = scandir("SCORES", &filelist, NULL, alphasort);
    if (n < 0) {
        perror("scandir SCORES");
        send(client_fd, "RSS EMPTY\n", 10, 0);
        return;
    }

    ScoreEntry scores[1000]; 
    int score_count = 0;

    for (int i = 0; i < n; i++) {
        if (filelist[i]->d_name[0] == '.') {
            free(filelist[i]);
            continue;
        }

        char fullpath[512]; // Increased size to avoid truncation warning
        snprintf(fullpath, sizeof(fullpath), "SCORES/%s", filelist[i]->d_name);

        FILE *f = fopen(fullpath, "r");
        if (!f) {
            perror("fopen score file");
            free(filelist[i]);
            continue;
        }

        char line[MAX_BUFFER_SIZE];
        if (fgets(line, sizeof(line), f)) {
            int SSS, N;
            char PLID[7], CCCC[5], mode[16];
            if (sscanf(line, "%d %6s %4s %d %15s", &SSS, PLID, CCCC, &N, mode) == 5) {
                if (score_count < 1000) {
                    ScoreEntry *entry = &scores[score_count++];
                    entry->SSS = SSS;
                    strncpy(entry->PLID, PLID, 6);
                    entry->PLID[6] = '\0';
                    strncpy(entry->secret_key, CCCC, 4);
                    entry->secret_key[4] = '\0';
                    entry->total_plays = N;
                }
            }
        }

        fclose(f);
        free(filelist[i]);
    }
    free(filelist);

    if (score_count == 0) {
        send(client_fd, "RSS EMPTY\n", 10, 0);
        return;
    }

    qsort(scores, score_count, sizeof(ScoreEntry), compare_scores);

    int limit = score_count < 10 ? score_count : 10;

    char scoreboard_file[] = "top_scoreboard.txt";
    FILE *out = fopen(scoreboard_file, "w");
    if (!out) {
        perror("fopen top_scoreboard");
        send(client_fd, "RSS EMPTY\n", 10, 0);
        return;
    }

    // Print line: SSS PLID secret_key total_plays
    for (int i = 0; i < limit; i++) {
        fprintf(out, "%03d %s %s %d\n", scores[i].SSS, scores[i].PLID, scores[i].secret_key, scores[i].total_plays);
    }
    fclose(out);

    FILE *file = fopen(scoreboard_file, "r");
    if (!file) {
        perror("fopen top_scoreboard to send");
        send(client_fd, "RSS EMPTY\n", 10, 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    char header[MAX_BUFFER_SIZE];
    snprintf(header, sizeof(header), "RSS OK %s %ld\n", "scoreboard.txt", filesize);
    send(client_fd, header, strlen(header), 0);

    char file_buffer[MAX_BUFFER_SIZE];
    size_t nread;
    while ((nread = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        send(client_fd, file_buffer, nread, 0);
    }

    fclose(file);
}

void process_show_trials_command(int client_fd) {
    char PLID[7];
    char filename[128];

    sscanf(buffer, "STR %6s", PLID);
    if (!validate_plid(PLID)) {
        send(client_fd, "RST NOK\n", 8, 0);
        return;
    }

    int time_status = check_and_update_game_time(PLID, NULL, client_fd, 0, "STR");
    if (time_status == -1) {

        if (!FindLastGame(PLID, filename)) {
            send(client_fd, "RST NOK\n", 8, 0);
            return;
        }
        char temp_filename[64];
        snprintf(temp_filename, sizeof(temp_filename), "trials_%s.txt", PLID);
        if (!extract_trials_from_game_file(filename, temp_filename, NULL)) {
            send(client_fd, "RST NOK\n", 8, 0);
            return;
        }
        send_file_to_client(client_fd, "FIN", temp_filename);
        remove(temp_filename);
        return;
    }

    PlayerGame *game = get_game(PLID);
    
    char temp_filename[64];
    snprintf(temp_filename, sizeof(temp_filename), "trials_%s.txt", PLID);

    if (game) {
        snprintf(filename, sizeof(filename), "GAMES/%s.txt", PLID);
    } else {
        if (!FindLastGame(PLID, filename)) {
            send(client_fd, "RST NOK\n", 8, 0);
            return;
        }
    }

    if (!extract_trials_from_game_file(filename, temp_filename, game)) {
        send(client_fd, "RST NOK\n", 8, 0);
        return;
    }

    const char *status = game ? "ACT" : "FIN";
    send_file_to_client(client_fd, status, temp_filename);
    remove(temp_filename);
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
    char local_buffer[MAX_BUFFER_SIZE];
    int n = recv(client_fd, local_buffer, sizeof(local_buffer), 0);
    if (n <= 0) {
        if (n < 0) perror("recv failed");
        return;
    }

    local_buffer[n] = '\0';
    printf("TCP request: %s\n", local_buffer);

    strncpy(buffer, local_buffer, MAX_BUFFER_SIZE);

    if (strncmp(local_buffer, "STR", 3) == 0) {
        process_show_trials_command(client_fd);
    } else if (strncmp(local_buffer, "SSB", 3) == 0) {
        // Implement the scoreboard functionality
        process_scoreboard_command(client_fd);
    } else {
        printf("Unknown TCP request\n");
        send(client_fd, "RST NOK\n", 8, 0);
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

int FindLastGame(const char *PLID, char *filename) {
    struct dirent **filelist;
    char directory_path[64];
    snprintf(directory_path, sizeof(directory_path), "GAMES/%s/", PLID);

    int n = scandir(directory_path, &filelist, NULL, alphasort);
    if (n <= 0) return 0;

    for (int i = n - 1; i >= 0; i--) {
        if (filelist[i]->d_name[0] != '.') {
            snprintf(filename, 320, "%s%s", directory_path, filelist[i]->d_name);
            break;
        }
        free(filelist[i]);
    }

    for (int i = 0; i < n; i++) free(filelist[i]);
    free(filelist);
    return 1;
}

void cleanup_and_exit(int signum) {
    printf("\nShutting down server gracefully...\n");

    if (udp_fd > 0) close(udp_fd);
    if (tcp_fd > 0) close(tcp_fd);

    PlayerGame *current = player_games;
    while (current != NULL) {
        PlayerGame *next = current->next;
        free(current);
        current = next;
    }

    printf("Resources cleaned up successfully. Exiting.\n");
    exit(0);
}

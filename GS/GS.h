#ifndef GS_H
#define GS_H

#define MAX_PLAYERS 100

#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#define WIN "W"
#define FAIL "F"
#define QUIT "Q"
#define TIMEOUT "T"

#define PLAY "P"
#define DEBUG "D"

typedef struct PlayerGame {
    char PLID[7];
    char mode[7];
    char secret_key[5];
    int total_duration;
    int remaining_time;
    int elapsed_time;
    int current_trial;
    int expected_trial;
    char last_guess[5]; 
    time_t last_update_time;
    time_t start_time;
    struct PlayerGame *next; // Linked list pointer to the next player
} PlayerGame;

void handle_udp_commands();
void handle_tcp_connection(int client_fd);
void generate_secret_key(char *secret_key);
PlayerGame *find_or_create_game(const char *PLID, const char *time_str, const char *mode);
PlayerGame *get_game(const char *PLID);
void remove_game(const char *PLID, const char *status);
void send_file_to_client(int client_fd, const char *status, const char *filepath);
void format_secret_key(char *formatted_key, const char *secret_key);
void calculate_nB_nW(const char *guess, const char *secret_key, int *nB, int *nW);
void cleanup_and_exit(int signum);
void create_score_file(PlayerGame *game);
int calculate_score(int total_trials, int game_duration, int max_duration);

#endif
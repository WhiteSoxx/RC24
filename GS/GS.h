#ifndef GS_H
#define GS_H

#define MAX_PLAYERS 100

typedef struct {
    char PLID[7];
    char secret_key[5];
    int remaining_time;
    int trials_left;
    int current_trial;
} PlayerGame;


void handle_udp_commands();
void handle_tcp_connection(int client_fd);
void generate_secret_key(char *secret_key);
PlayerGame *find_or_create_game(const char *PLID);
PlayerGame *get_game(const char *PLID);
void remove_game(const char *PLID);
void calculate_nB_nW(const char *guess, const char *secret_key, int *nB, int *nW);

#endif
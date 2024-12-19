#include "../common.h"
#include "GS.h"
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>

int udp_fd, tcp_fd, errcode;
socklen_t addrlen;
char buffer[MAX_BUFFER_SIZE];
int verbose = 0;

/**
 * @brief Retrieves the trial number for a given player from their active game file.
 * 
 * @param PLID The player ID.
 * @return The current trial number or -1 if the information cannot be retrieved.
 */
int get_trial_number(const char *PLID) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "GAMES/GAME_%s.txt", PLID);

    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Error opening game file");
        return -1;
    }

    char line[MAX_BUFFER_SIZE];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return -1;
    }

    int trial_number;
    sscanf(line, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %d", &trial_number);
    fclose(file);
    return trial_number;
}

/**
 * @brief Retrieves the secret key for a given player from their active game file.
 * 
 * @param PLID The player ID.
 * @return A string representing the secret key, or NULL if the information cannot be retrieved.
 */
char* get_secret_key(const char *PLID) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "GAMES/GAME_%s.txt", PLID);

    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Error opening game file");
        return NULL;
    }

    char *secret_key = malloc(5);
    if (!secret_key) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    char line[MAX_BUFFER_SIZE];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        free(secret_key);
        return NULL;
    }

    sscanf(line, "%*s %*s %4s", secret_key);
    fclose(file);
    return secret_key;
}

/**
 * @brief Retrieves the remaining playtime for a given player from their active game file.
 * 
 * @param PLID The player ID.
 * @return The remaining playtime in seconds or -1 if the information cannot be retrieved.
 */
int get_remaining_time(const char *PLID) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "GAMES/GAME_%s.txt", PLID);

    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Error opening game file");
        return -1;
    }

    char line[MAX_BUFFER_SIZE];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return -1;
    }

    int remaining_time;
    sscanf(line, "%*s %*s %*s %d", &remaining_time);
    fclose(file);
    return remaining_time;
}

/**
 * @brief Updates the game file for a given player to reflect changes to the current trial.
 * 
 * @param PLID The player ID.
 * @param trial_number The updated trial number.
 */
void update_trial_number(const char *PLID, int trial_number) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "GAMES/GAME_%s.txt", PLID);

    FILE *file = fopen(filepath, "r+");
    if (!file) {
        perror("Error opening game file");
        return;
    }

    char line[MAX_BUFFER_SIZE];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return;
    }

    char updated_line[MAX_BUFFER_SIZE];
    sscanf(line, "%*s %*s %*s %*s %*s %*s %*s %*s %*s");
    snprintf(updated_line, sizeof(updated_line), "%s %d\n", line, trial_number);
    rewind(file);
    fputs(updated_line, file);
    fclose(file);
}

/**
 * @brief Creates a new game file for a given player.
 * 
 * @param PLID The player ID.
 * @param mode The game mode (P for Play, D for Debug).
 * @param secret_key The 4-letter secret key.
 * @param max_time The maximum playtime in seconds.
 */
void create_game_file(const char *PLID, char mode, const char *secret_key, int max_time) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "GAMES/GAME_%s.txt", PLID);

    FILE *file = fopen(filepath, "w");
    if (!file) {
        perror("Error creating game file");
        return;
    }

    time_t current_time = time(NULL);
    struct tm *timeinfo = localtime(&current_time);
    char date_time[20];
    strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", timeinfo);

    fprintf(file, "%s %c %s %d %s %ld\n", PLID, mode, secret_key, max_time, date_time, (long)current_time);
    fclose(file);
}

/**
 * @brief Removes the game file for a given player.
 * 
 * @param PLID The player ID.
 */
void remove_game_file(const char *PLID) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "GAMES/GAME_%s.txt", PLID);
    if (remove(filepath) != 0) {
        perror("Error deleting game file");
    }
}

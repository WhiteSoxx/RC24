#include "GS.h"
#include "../common.h"

void create_score_file(PlayerGame *game) {
    if (!game) {
        printf("[ERROR] PlayerGame is NULL. Cannot create score file.\n");
        return;
    }

    int game_duration = game->elapsed_time;
    int score = calculate_score(game->current_trial, game_duration, game->total_duration); // Assuming max_duration is 300 seconds

    // Generate timestamp for the file name
    struct tm *t = localtime(&game->last_update_time);
    char end_date[9];  // DDMMYYYY
    char end_time_str[7]; // HHMMSS
    strftime(end_date, sizeof(end_date), "%d%m%Y", t);
    strftime(end_time_str, sizeof(end_time_str), "%H%M%S", t);

    // Create the file name: SCORES/SSS_PLID_DDMMYYYY_HHMMSS.txt
    char filename[128];
    snprintf(filename, sizeof(filename), "SCORES/%03d_%s_%s_%s.txt", score, game->PLID, end_date, end_time_str);

    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Failed to create score file");
        return;
    }

    // Write the score information to the file
    fprintf(file, "%03d %s %s %d %s\n", score, game->PLID, game->secret_key, game->current_trial, game->mode);
    fclose(file);

    printf("[*] Score file created: %s\n", filename);
}



int calculate_score(int total_trials, int game_duration, int max_duration) {

    int penalty_from_trials = (total_trials - 1) * 5;

    double time_ratio = (double)game_duration / (double)max_duration; 
    int penalty_from_time = (int)(time_ratio * 50); 

    int score = 100 - penalty_from_trials - penalty_from_time;
    if (score < 0) score = 0;
    if (score > 100) score = 100;

    return score;
}

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

    char mode[16];
    if(!strcmp(game->mode, "D")) {
        strcpy(mode, "DEBUG");
    }
    else {
        strcpy(mode, "PLAY");
    }

    // Write the score information to the file
    fprintf(file, "%03d %s %s %d %s", score, game->PLID, game->secret_key, game->current_trial, mode);
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

int compare_scores(const void *a, const void *b) {
    const ScoreEntry *sa = (const ScoreEntry*)a;
    const ScoreEntry *sb = (const ScoreEntry*)b;
    // ascending order by attempts means smaller SSS first
    return sa->SSS - sb->SSS;
}

ScoreEntry* load_scores(int *count) {
    struct dirent **filelist;
    int n = scandir("SCORES", &filelist, NULL, alphasort);
    if (n < 0) {
        *count = 0;
        return NULL;
    }

    ScoreEntry *scores = NULL;
    int capacity = 0;
    int score_count = 0;

    for (int i = 0; i < n; i++) {
        if (filelist[i]->d_name[0] == '.') {
            free(filelist[i]);
            continue;
        }

        char fullpath[512]; 
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
                // Ensure capacity
                if (score_count >= capacity) {
                    int new_cap = capacity == 0 ? 64 : capacity * 2;
                    ScoreEntry *new_scores = realloc(scores, new_cap * sizeof(ScoreEntry));
                    if (!new_scores) {
                        perror("realloc scores");
                        fclose(f);
                        free(filelist[i]);
                        free(scores);
                        *count = 0;
                        return NULL;
                    }
                    scores = new_scores;
                    capacity = new_cap;
                }

                ScoreEntry *entry = &scores[score_count++];
                entry->SSS = SSS;
                strncpy(entry->PLID, PLID, 6); entry->PLID[6] = '\0';
                strncpy(entry->secret_key, CCCC, 4); entry->secret_key[4] = '\0';
                entry->total_plays = N;
                if(!strcmp(mode, "D")) {
                    strcpy(entry->mode, "DEBUG");
                }
                else {
                    strcpy(entry->mode, "PLAY");
                }

            }
        }

        fclose(f);
        free(filelist[i]);
    }
    free(filelist);

    *count = score_count;
    return scores;
}
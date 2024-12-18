#include "common.h"

// Validates a player ID (PLID) to ensure it is a 6-digit number
int validate_plid(const char *plid) {
    if (plid == NULL || strlen(plid) != 6) return 0;
    for (int i = 0; i < 6; i++) {
        if (!isdigit(plid[i])) return 0;
    }
    return 1;
}

// Validates that the time value is within the allowed range (1 to MAX_PLAYTIME)
int validate_play_time(const char *time) {
    if (time == NULL) return 0;
    int play_time = atoi(time);
    if (play_time <= 0 || play_time > MAX_PLAYTIME) return 0;
    return 1;
}

// Validates that the input color codes are valid (only R, G, B, Y, O, P) and there are exactly 4 colors
int validate_color_sequence(const char *c1, const char *c2, const char *c3, const char *c4) {
    if (!c1 || !c2 || !c3 || !c4) return 0;
    const char valid_colors[] = "RGBYOP";
    return (strchr(valid_colors, c1[0]) && strchr(valid_colors, c2[0]) && 
            strchr(valid_colors, c3[0]) && strchr(valid_colors, c4[0]));
}
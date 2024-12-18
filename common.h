#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>

// CONSTANTS
#define GN 54
#define DEFAULT_IP "localhost"
#define DEFAULT_PORT "58054"  // 58000 + GN
#define COLOR_SEQUENCE_LEN 4
#define MAX_TRIALS 8
#define MAX_PLAYTIME 600  // Maximum playtime in seconds
#define MAX_BUFFER_SIZE 1024


// FUNCTIONS

int validate_plid(const char *plid);
int validate_play_time(const char *time);
int validate_color_sequence(const char *c1, const char *c2, const char *c3, const char *c4);

#endif
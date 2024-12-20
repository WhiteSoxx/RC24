# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Source and output files
PLAYER_SRC = player.c
COMMON_SRC = ../common.c

# Header files
COMMON_HEADER = ../common.h

# Output executable (inside player folder)
PLAYER_EXEC = player

# Phony targets
.PHONY: all clean

# Default target: build Player
all: $(PLAYER_EXEC)

# Compile the Player application
$(PLAYER_EXEC): $(PLAYER_SRC) $(COMMON_SRC) $(COMMON_HEADER)
	$(CC) $(CFLAGS) -o $(PLAYER_EXEC) $(PLAYER_SRC) $(COMMON_SRC)

# Clean the compiled files
clean:
	rm -f $(PLAYER_EXEC) *.txt

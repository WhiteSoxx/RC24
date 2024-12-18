# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Source and output files
GS_SRC = GS.c
COMMON_SRC = ../common.c

# Header files
GS_HEADER = GS.h
COMMON_HEADER = ../common.h

# Output executable (inside GS folder)
GS_EXEC = GS

# Phony targets
.PHONY: all clean

# Default target: build GS
all: $(GS_EXEC)

# Compile the Game Server (GS)
$(GS_EXEC): $(GS_SRC) $(COMMON_SRC) $(GS_HEADER) $(COMMON_HEADER)
	$(CC) $(CFLAGS) -o $(GS_EXEC) $(GS_SRC) $(COMMON_SRC)

# Clean the compiled files
clean:
	rm -f $(GS_EXEC)

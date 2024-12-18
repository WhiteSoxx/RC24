# Phony targets
.PHONY: all clean run-gs run-player

# Default target: build both GS and Player
all:
	$(MAKE) -C GS
	$(MAKE) -C player

# Clean compiled files in both GS and Player directories
clean:
	$(MAKE) -C GS clean
	$(MAKE) -C player clean

# Run the Game Server with verbose mode
run-gs:
	./GS/GS -v

# Run the Player application
run-player:
	./player/player
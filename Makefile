GS:
	gcc -o player player.c 

clean:
	rm -f player

run: GS
	./player -n GSIP -p 34
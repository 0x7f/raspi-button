.DEFAULT: main

main: main.c
	gcc main.c -lwiringPi -o main
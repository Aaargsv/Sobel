sobel: sobel.c
	gcc -o sobel sobel.c -lm -lpthread -fsanitize=address

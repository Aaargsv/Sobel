#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <time.h>
#define SIZE 256

typedef struct {

	unsigned char R;
	unsigned char G;
	unsigned char B;

} pixel;

typedef struct {

	int start_row;
	int end_row;
	int width;
	int threshold;

} thread_info;

pixel **image;
unsigned char **filter_image;
unsigned char **gray_image;
unsigned char **filter_image;

void *SobelFilter(void *arg);
int isNumber(const char *str);
void ReadRecord(int fd, unsigned char *buffer);	//read string of file header
void RGB2GrayScale(int width, int height);
void Free_RGB(pixel ** memory, int height);
void Free_Gray(unsigned char **memory, int height);

int
main(int argc, char const *argv[])
{

	if (argc < 3) {
		printf("Too few arguments!\n");
		exit(1);
	}

	if (!isNumber(argv[2])) {
		printf("Number of threads is not correct!\n");
		exit(1);
	}

	unsigned char filename[SIZE];
	int number_threads;

	strcpy(filename, argv[1]);
	number_threads = atoi(argv[2]);

	if (number_threads == 0) {
		printf("Number of threads cannot be zero!\n");
		exit(1);
	}

	int fd_image;
	int fd_filter_image;

	fd_image = open(filename, O_RDONLY);
	if (fd_image == -1) {
		perror(filename);
		exit(1);
	}

	int width;
	int height;
	int maxval;
	int n;
	unsigned char buffer[SIZE];

	n = read(fd_image, buffer, sizeof (char) * 3);
	if (n <= 0) {
		perror("Error");
		exit(1);
	}
	//read file header
	if (strncmp(buffer, "P6", 2) != 0) {
		printf("Wrong format\n");
		exit(1);
	}

	ReadRecord(fd_image, buffer);

	for (int i = 0;; i++) {
		if (buffer[i] == 0) {

			printf("%s: Wrong resolution record!", buffer);
			exit(0);
		}

		if (buffer[i] == ' ') {
			buffer[i] = 0;

			if (!isNumber(buffer)) {
				printf("Wrong resolution record!!\n");
				exit(1);
			}

			if (!isNumber(buffer + i + 1)) {
				printf("Wrong resolution record!\n");
				exit(1);
			}

			width = atoi(buffer);
			height = atoi(buffer + i + 1);
			break;
		}

	}

	ReadRecord(fd_image, buffer);

	if (!isNumber(buffer)) {
		printf("Wrong maxval record!\n");
		exit(1);
	}

	maxval = atoi(buffer);

	image = (pixel **) malloc(height * sizeof (pixel *));
	if (image == NULL) {
		printf("Memory error image - height\n");
		exit(1);
	}

	for (int i = 0; i < height; i++) {
		image[i] = (pixel *) malloc(width * sizeof (pixel));
		if (image[i] == NULL) {
			printf("Memory error image  - width\n");
			exit(1);
		}
	}

	gray_image =
	    (unsigned char **) malloc(height * sizeof (unsigned char *));
	if (gray_image == NULL) {
		printf("Memory error gray_image  - height\n");
		exit(1);
	}

	for (int i = 0; i < height; i++) {
		gray_image[i] =
		    (unsigned char *) malloc(width * sizeof (unsigned char));
		if (gray_image[i] == NULL) {
			printf("Memory error gray_image - width\n");
			exit(1);
		}
	}

	filter_image =
	    (unsigned char **) malloc(height * sizeof (unsigned char *));
	if (filter_image == NULL) {
		printf("Memory error filter_image  - height\n");
		exit(1);
	}

	for (int i = 0; i < height; i++) {
		filter_image[i] =
		    (unsigned char *) calloc(width, sizeof (unsigned char));
		if (filter_image[i] == NULL) {
			printf("Memory error filter_image - width\n");
			exit(1);
		}
	}

	for (int i = 0; i < height; i++) {
		n = read(fd_image, image[i], width * sizeof (pixel));
		if (n <= 0) {
			printf("Error reading image\n");
			Free_RGB(image, height);
			Free_Gray(gray_image, height);
			Free_Gray(filter_image, height);
			exit(1);
		}
	}

	RGB2GrayScale(width, height);

	if (number_threads > height - 2)
		number_threads = height - 2;

	thread_info *ti_p = malloc(number_threads * sizeof (thread_info));
	if (ti_p == NULL) {
		printf("Memory error thread_info \n");
		exit(1);
	}

	pthread_t *threads = malloc(number_threads * sizeof (pthread_t));
	if (threads == NULL) {
		printf("Memory error threads\n");
		exit(1);
	}

	int rows = (height - 2) / number_threads;
	int remainder = (height - 2) % number_threads;

	for (int i = 0, start = 1; i < number_threads; i++) {
		ti_p[i].start_row = start;
		if (remainder) {
			remainder--;
			ti_p[i].end_row = start + rows;
			start += rows + 1;
		} else {
			ti_p[i].end_row = start + rows - 1;
			start += rows;
		}

		ti_p[i].width = width;
		ti_p[i].threshold = 20;
	}

	clockid_t clk_id;
 	struct timespec tp1;
	struct timespec tp2;
	double realtime;

	clock_gettime (CLOCK_REALTIME, &tp1);
	for (int i = 0; i < number_threads; i++) {
		int res;
		res = pthread_create(&threads[i], NULL, SobelFilter, (void *) (&(ti_p[i])));	//start threads
		if (res != 0) {
			perror("Thread creation failed");
			Free_RGB(image, height);
			Free_Gray(gray_image, height);
			Free_Gray(filter_image, height);
			free(threads);
			free(ti_p);
			exit(1);
		}
	}

	for (int i = 0; i < number_threads; i++) {
		int res = pthread_join(threads[i], NULL);	//join threads
		if (res != 0) {
			perror("Thread join failed");
			Free_RGB(image, height);
			Free_Gray(gray_image, height);
			Free_Gray(filter_image, height);
			free(threads);
			free(ti_p);
			exit(1);
		}
	}
	clock_gettime (CLOCK_REALTIME, &tp2);
	realtime = (tp2.tv_sec - tp1.tv_sec) +
						 (tp2.tv_nsec - tp1.tv_nsec) / 1000000000.;

	printf("time: %f sec\n", realtime);
	free(threads);
	free(ti_p);

	char header[SIZE];
	sprintf(header, "P5\n%d %d\n%d\n", width, height, maxval);
	fd_filter_image = creat("B.pgm", S_IRWXU);

	if (fd_filter_image == -1) {
		perror("Error creation filter_image file");
		Free_RGB(image, height);
		Free_Gray(gray_image, height);
		Free_Gray(filter_image, height);
		exit(1);
	}

	if (write
	    (fd_filter_image, header,
	     strlen(header) * sizeof (unsigned char)) <= 0) {
		perror("Error writing to file");
		Free_RGB(image, height);
		Free_Gray(gray_image, height);
		Free_Gray(filter_image, height);
		exit(1);
	}

	for (int i = 0; i < height; i++) {
		n = write(fd_filter_image, filter_image[i],
			  width * sizeof (unsigned char));
		if (n <= 0) {
			perror("Error writing to file");
			Free_RGB(image, height);
			Free_Gray(gray_image, height);
			Free_Gray(filter_image, height);
			exit(1);
		}
	}

	Free_RGB(image, height);
	Free_Gray(gray_image, height);
	Free_Gray(filter_image, height);
	close(fd_image);
	close(fd_filter_image);
	return 0;
}

int
isNumber(const char *str)
{
	if (str[0] == 0)
		return 0;

	for (int i = 0; str[i] != 0; i++)
		if (!isdigit(str[i]))
			return 0;

	return 1;
}

void
ReadRecord(int fd, unsigned char *buffer)
{

	for (int i = 0;; i++) {
		int n;
		n = read(fd, buffer + i, sizeof (char));

		if (n <= 0) {
			printf("file is not complete");
			exit(0);
		}

		if (iscntrl(buffer[i])) {
			buffer[i] = 0;
			break;
		}
	}
}

void
Free_RGB(pixel ** memory, int height)
{
	for (int i = 0; i < height; i++)
		free(memory[i]);

	free(memory);
}

void
Free_Gray(unsigned char **memory, int height)
{
	for (int i = 0; i < height; i++)
		free(memory[i]);
	free(memory);
}

void
RGB2GrayScale(int width, int height)
{
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; j++)
			gray_image[i][j] =
			    (int) ((0.3 * image[i][j].R) +
				   (0.59 * image[i][j].G) +
				   (0.11 * image[i][j].B));

}

void *
SobelFilter(void *arg)
{
	sleep(1);
	thread_info *inf = arg;
	int G;

 for (int i = inf->start_row; i <= inf->end_row; i++)
    for (int j = 1; j < inf->width - 1; j++)
    {

	G = sqrt
	      (
		(gray_image[i+1][j-1] + 2*gray_image[i+1][j] + gray_image[i+1][j+1] - gray_image[i-1][j-1] - 2*gray_image[i-1][j] - gray_image[i-1][j+1])*
		(gray_image[i+1][j-1] + 2*gray_image[i+1][j] + gray_image[i+1][j+1] - gray_image[i-1][j-1] - 2*gray_image[i-1][j] - gray_image[i-1][j+1])
		+
		(gray_image[i-1][j+1] + 2*gray_image[i][j+1] + gray_image[i+1][j+1] - gray_image[i-1][j-1] - 2*gray_image[i][j-1] - gray_image[i+1][j-1])*
		(gray_image[i-1][j+1] + 2*gray_image[i][j+1] + gray_image[i+1][j+1] - gray_image[i-1][j-1] - 2*gray_image[i][j-1] - gray_image[i+1][j-1])
	      );

	filter_image[i][j] = G;	//(G < inf->threshold) ? 0 : 255;
    }

	pthread_exit("Have a good sdfsdfsdf!!!!");
}

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <fec.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// CONFIG CONSTANTS
#define RS_ENC_PAYLOAD_SIZE 128
#define PAYLOAD_SIZE 64
#define PARITY_SIZE 64
#define CHIP_RATE 16
#define ALPHA 5.0


void haar_1d(double *data, int len) {

	double *temp = (double *)malloc(len * sizeof(double));
	int half = len / 2;

	double inv_sqrt2 = 1.0 / sqrt(2.0);

	for (int i = 0; i < half; i++) {
		temp[i] = (data[2*i] + data[2*i+1]) * inv_sqrt2;
		temp[half + i] = (data[2*i] - data[2*i+1]) * inv_sqrt2;
	}

	for (int i = 0; i < len; i++) {
		data[i] = temp[i];
	}

	free(temp);
}

int main(int argc, char *argv[]) {

	if (argc != 3) {
		printf("Error: usage is: ./%s <IMAGE_TITLE> <SECRET_KEY>\n", argv[0]);
		return 1;
	}

	// Initialize variables
	int width, height, channels;
	unsigned int secret_key = (unsigned int)strtoul(argv[2], NULL, 10);

	// Open image and extract blue channel
	unsigned char *raw_pixels = stbi_load(argv[1], &width, &height, &channels, 3);
	
	if (raw_pixels == NULL) {
		printf("Error: Image is missing\n");
		return 1;
	}
	
	double *blue = (double *)malloc(width * height * sizeof(double));
	for (int i = 0; i < width * height; i++) {
		blue[i] = (double)raw_pixels[i * 3 + 2];
	}

	stbi_image_free(raw_pixels);

	// Apply Haar on the rows
	for (int i = 0; i < height; i++) {
		haar_1d(&blue[i * width], width);
	}

	// Apply Haar on the columns
	double *col = (double *)malloc(height * sizeof(double));
	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			col[j] = blue[j * width + i];
		}

		haar_1d(col, height);

		for (int j = 0; j < height; j++) {
			blue[j * width + i] = col[j];
		}
	}

	free(col);

	// DSSS Extraction & Decoding logic goes here

	return 0;
}


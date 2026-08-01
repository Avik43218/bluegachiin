#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "headers/stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "headers/stb_image.h"

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


void init_dsss(unsigned int secret_key) {
	srand(secret_key);
}

int get_pn_chip() {
	return (rand() % 2 == 0) ? 1 : -1;
}

unsigned char* open_img(char *img_path, int *w, int *h, int *c) {
	
	unsigned char *raw_pixels = stbi_load(img_path, w, h, c, 3);

	if (raw_pixels == NULL) {
		printf("Error opening image!\n");
		return NULL;
	}

	printf("Image loaded successfully!\n");

	return raw_pixels;
}

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

void inv_haar_1d(double *data, int len) {

	double *temp = (double *)malloc(len * sizeof(double));
	int half = len / 2;
	double inv_sqrt2 = 1.0 / sqrt(2.0);

	for (int i = 0; i < half; i++) {
		temp[2*i] = (data[i] + data[half + i]) * inv_sqrt2;
		temp[2*i+1] = (data[i] - data[half + i]) * inv_sqrt2;
	}

	for (int i = 0; i < len; i++) {
		data[i] = temp[i];
	}

	free(temp);
}

void rs_dsss(double *data, unsigned char payload[PAYLOAD_SIZE], int width, unsigned int secret_key) {
	
	unsigned char encoded_payload[RS_ENC_PAYLOAD_SIZE];
	unsigned char parity[PARITY_SIZE];

	int pad_value = 255 - (PARITY_SIZE + PAYLOAD_SIZE);
	int start_x = width / 2;
	int start_y = 0;

	encode_rs_8(payload, parity, pad_value);

	memcpy(encoded_payload, payload, PAYLOAD_SIZE);
	memcpy(encoded_payload + PAYLOAD_SIZE, parity, PARITY_SIZE);

	init_dsss(secret_key);

	for (int idx = 0; idx < RS_ENC_PAYLOAD_SIZE; idx++) {
		unsigned char byte = encoded_payload[idx];

		for (int bit = 7; bit >= 0; bit--) {
			int payload_bit = (byte >> bit) & 1;
			int polar_bit = (payload_bit == 1) ? 1 : -1;

			for (int i = 0; i < CHIP_RATE; i++) {
				int pn = get_pn_chip();
				data[start_y * width + start_x] += (ALPHA * polar_bit * pn);
				start_x++;

				if (start_x >= width) {
					start_x = width / 2;
					start_y++;
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {

	if (argc != 4) {
		printf("Error: usage is: ./%s <FILE_NAME> <PAYLOAD> <SECRET_KEY>", argv[0]);
		return 1;
	}

	// Initialise variables
	int width, height, channels;
	char *image_title = argv[1];
	unsigned char *payload = (unsigned char *)argv[2];
	unsigned int secret_key = (unsigned int)strtoul(argv[3], NULL, 10);

	if (strlen((const char *)payload) != PAYLOAD_SIZE) {
		printf("Error: PAYLOAD_SIZE should be 64 bytes\n");
	}

	int new_len = strlen(image_title) + 3;
	char *new_title = (char *)malloc(new_len);

	if (!new_title) {
		printf("Error: Memory allocation for image title failed!\n");
		return 1;
	}

	new_title[0] = 'F';
	new_title[1] = '_';
	strcpy(&new_title[2], image_title);

	// Open image and extract the blue channel
	unsigned char *img = open_img(image_title, &width, &height, &channels);

	double *blue = (double *)malloc(width * height * sizeof(double));
	for (int i = 0; i < width * height; i++) {
		blue[i] = (double)img[i * 3 + 2];
	}

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

	// Encode the RS payload using DSSS
	rs_dsss(blue, payload, width, secret_key);

	// Apply Inverse Haar on the columns
	double *col_inv = (double *)malloc(height * sizeof(double));

	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			col_inv[j] = blue[j * width + i];
		}

		inv_haar_1d(col_inv, height);

		for (int j = 0; j < height; j++) {
			blue[j * width + i] = col_inv[j];
		}
	}
	free(col_inv);

	// Apply Inverse Haar on the rows
	for (int j = 0; j < height; j++) {
		inv_haar_1d(&blue[j * width], width);
	}

	// Convert the modified doubles into bytes
	for (int i = 0; i < width * height; i++) {

		int pixel_val = (int)round(blue[i]);

		if (pixel_val < 0) {
			pixel_val = 0;
		}
		else if (pixel_val > 255) {
			pixel_val = 255;
		}

		img[i * 3 + 2] = (unsigned char)pixel_val;
	}

	// Save the image to disk
	stbi_write_png(new_title, width, height, 3, img, width * 3);
	printf("Saved image to disk successfully!\n");

	// Free the allocated memory
	free(blue);
	free(new_title);
	stbi_image_free(img);

	return 0;
}

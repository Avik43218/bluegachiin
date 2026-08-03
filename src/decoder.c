#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

#include <fec.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// CONFIG CONSTANTS
#define RS_ENC_PAYLOAD_SIZE 96
#define PAYLOAD_SIZE 64
#define PARITY_SIZE 32
#define CHIP_RATE 16


void init_dsss(unsigned int secret_key) {
	srand(secret_key);
}

int get_pn_chip() {
	return (rand() % 2 == 0) ? 1 : -1;
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

void decode_rs_dsss(double *data, char message[PAYLOAD_SIZE], int width, unsigned int secret_key) {

	unsigned char extracted_payload[RS_ENC_PAYLOAD_SIZE];

	int start_x = width / 2;
	int start_y = 0;

	init_dsss(secret_key);

	for (int byte_idx = 0; byte_idx < RS_ENC_PAYLOAD_SIZE; byte_idx++) {
		unsigned char current_byte = 0;

		for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
			double sum = 0.0;

			for (int i = 0; i < CHIP_RATE; i++) {
				int pn = get_pn_chip();
				sum += (data[start_y * width + start_x] * (double)pn);
				start_x++;

				if (start_x >= width) {
					start_x = width / 2;
					start_y++;
				}
			}

			if (sum > 0.0) {
				current_byte |= (1 << bit_idx);
			}

		}

		extracted_payload[byte_idx] = current_byte;
	}

	// Decode the RS payload
	
	int pad = 255 - (PARITY_SIZE + PAYLOAD_SIZE);

	int errors_fixed = decode_rs_8(extracted_payload, NULL, 0, pad);

	if (errors_fixed < 0) {
		printf("Data could not be recovered!\n");
	}
	else {
		printf("Successfully recovered %d errors!\n", errors_fixed);

		memcpy(message, extracted_payload, PAYLOAD_SIZE);
		message[PAYLOAD_SIZE] = '\0';

		printf("Message successfully decoded!\n");
	}
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
	
	// Extract RS-DSSS payload and decode
	char message[PAYLOAD_SIZE + 1];
	decode_rs_dsss(blue, message, width, secret_key);

	printf("Secret Message: %s\n", message);

	free(blue);

	return 0;
}


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "bg_headers.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <fec.h>
#include <getopt.h>
#include <stdbool.h>

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

void decode_rs_dsss(double *data, char message[BUFFER_SIZE], int width, unsigned int secret_key) {

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
    char *input_path = NULL;
    char *output_path = NULL;
    char *key_str = NULL;
    bool verbose = false;

    static struct option long_options[] = {
        {"input",   required_argument, 0, 'i'},
        {"output",  required_argument, 0, 'o'},
        {"key",     required_argument, 0, 'k'},
        {"verbose", no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:o:k:v", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i': input_path = optarg; break;
            case 'o': output_path = optarg; break;
            case 'k': key_str = optarg; break;
            case 'v': verbose = true; break;
            default:
                fprintf(stderr, "Error: Invalid argument passed to PNG decoder.\n");
                return 1;
        }
    }

    // --- Validations ---
    if (!input_path) {
        fprintf(stderr, "Error: Missing input image path (-i).\n");
        return 1;
    }
    if (!key_str) {
        fprintf(stderr, "Error: Missing secret PRNG key (-k).\n");
        return 1;
    }

    // Parse secret key as unsigned int with strict termination check
    char *endptr = NULL;
    unsigned int secret_key = (unsigned int)strtoul(key_str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Provide a valid integer for the secret key!\n");
        return 1;
    }

    if (verbose) {
        printf("[PNG-DEC] Reading carrier: %s\n", input_path);
        printf("[PNG-DEC] PRNG Seed:       %u\n", secret_key);
        if (output_path) {
            printf("[PNG-DEC] Output dest:     %s\n", output_path);
        }
    }

    // Initialize variables
    int width, height, channels;

    // Open image and extract blue channel
    unsigned char *raw_pixels = stbi_load(input_path, &width, &height, &channels, 3);
    if (raw_pixels == NULL) {
        fprintf(stderr, "Error: Image is missing or failed to load: %s\n", input_path);
        return 1;
    }

    double *blue = (double *)malloc(width * height * sizeof(double));
    if (!blue) {
        fprintf(stderr, "Error: Memory allocation failed for blue channel buffer!\n");
        stbi_image_free(raw_pixels);
        return 1;
    }

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
    if (!col) {
        fprintf(stderr, "Error: Memory allocation failed for Haar column buffer!\n");
        free(blue);
        return 1;
    }

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
    char message[BUFFER_SIZE];
    decode_rs_dsss(blue, message, width, secret_key);

    // Output handling: write to disk if -o is specified, else print to stdout
    if (output_path) {
        FILE *out_fp = fopen(output_path, "wb");
        if (!out_fp) {
            perror("Error opening output destination file");
        } else {
            fputs(message, out_fp);
            fclose(out_fp);
            if (verbose) {
                printf("[PNG-DEC] Successfully saved message to %s\n", output_path);
            }
        }
    } else {
        printf("Secret Message: %s\n", message);
    }

    free(blue);

    return 0;
}

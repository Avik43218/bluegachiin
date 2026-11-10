#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

void encode_rs_dsss(double *data, unsigned char payload[PAYLOAD_SIZE], int width, unsigned int secret_key) {
	
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
				data[start_y * width + start_x] = (ALPHA * polar_bit * pn);
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
    char *input_path = NULL;
    char *output_path = NULL;
    char *key_str = NULL;
    char *msg_str = NULL;
    char *sig_file = NULL;
    bool verbose = false;

    static struct option long_options[] = {
        {"input",    required_argument, 0, 'i'},
        {"output",   required_argument, 0, 'o'},
        {"key",      required_argument, 0, 'k'},
        {"sig",      required_argument, 0, 's'},
        {"sig-file", required_argument, 0, 'f'},
        {"verbose",  no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:o:k:s:f:v", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i': input_path = optarg; break;
            case 'o': output_path = optarg; break;
            case 'k': key_str = optarg; break;
            case 's': msg_str = optarg; break;
            case 'f': sig_file = optarg; break;
            case 'v': verbose = true; break;
            default:
                fprintf(stderr, "Error: Invalid option provided to PNG encoder.\n");
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
    if (!msg_str && !sig_file) {
        fprintf(stderr, "Error: Provide payload string (-s) or payload file (-f).\n");
        return 1;
    }

    // Parse secret key as unsigned int with strict check
    char *endptr = NULL;
    unsigned int secret_key = (unsigned int)strtoul(key_str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Provide a valid integer for the secret key!\n");
        return 1;
    }

    // Initialize variables
    int width, height, channels;

    // Output destination fallback handling
    char *new_title = NULL;
    bool allocated_title = false;

    if (output_path) {
        new_title = output_path;
    } else {
        int new_len = strlen(input_path) + 3;
        new_title = (char *)malloc(new_len);
        if (!new_title) {
            printf("Error: Memory allocation for image title failed!\n");
            return 1;
        }
        new_title[0] = 'F';
        new_title[1] = '_';
        strcpy(&new_title[2], input_path);
        allocated_title = true;
    }

    if (verbose) {
        printf("[PNG-ENC] Input:   %s\n", input_path);
        printf("[PNG-ENC] Output:  %s\n", new_title);
        printf("[PNG-ENC] Seed:    %u\n", secret_key);
    }

    // Open image and extract the blue channel
    unsigned char *img = open_img(input_path, &width, &height, &channels);
    if (!img) {
        fprintf(stderr, "Error: Failed to open image: %s\n", input_path);
        if (allocated_title) free(new_title);
        return 1;
    }

    double *blue = (double *)malloc(width * height * sizeof(double));
    if (!blue) {
        fprintf(stderr, "Error: Memory allocation for blue channel buffer failed!\n");
        stbi_image_free(img);
        if (allocated_title) free(new_title);
        return 1;
    }

    for (int i = 0; i < width * height; i++) {
        blue[i] = (double)img[i * 3 + 2];
    }

    // Apply Haar on the rows
    for (int i = 0; i < height; i++) {
        haar_1d(&blue[i * width], width);
    }

    // Apply Haar on the columns
    double *col = (double *)malloc(height * sizeof(double));
    if (!col) {
        fprintf(stderr, "Error: Memory allocation for Haar column buffer failed!\n");
        free(blue);
        stbi_image_free(img);
        if (allocated_title) free(new_title);
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

    // Prepare payload from file or string
    unsigned char clean_payload[PAYLOAD_SIZE] = {0};

    if (sig_file) {
        FILE *fp = fopen(sig_file, "rb");
        if (!fp) {
            perror("Error opening signature file");
            free(blue);
            stbi_image_free(img);
            if (allocated_title) free(new_title);
            return 1;
        }
        size_t read_bytes = fread(clean_payload, 1, PAYLOAD_SIZE, fp);
        fclose(fp);
        if (read_bytes == 0) {
            fprintf(stderr, "Error: Signature file is empty.\n");
            free(blue);
            stbi_image_free(img);
            if (allocated_title) free(new_title);
            return 1;
        }
    } else {
        int arg_len = strlen(msg_str);
        if (arg_len > PAYLOAD_SIZE) {
            arg_len = PAYLOAD_SIZE;
        }
        memcpy(clean_payload, msg_str, arg_len);
    }

    // Encode the RS payload using DSSS
    encode_rs_dsss(blue, clean_payload, width, secret_key);

    // Apply Inverse Haar on the columns
    double *col_inv = (double *)malloc(height * sizeof(double));
    if (!col_inv) {
        fprintf(stderr, "Error: Memory allocation for Inverse Haar column buffer failed!\n");
        free(blue);
        stbi_image_free(img);
        if (allocated_title) free(new_title);
        return 1;
    }

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
        } else if (pixel_val > 255) {
            pixel_val = 255;
        }

        img[i * 3 + 2] = (unsigned char)pixel_val;
    }

    // Save the image to disk
    stbi_write_png(new_title, width, height, 3, img, width * 3);
    printf("Saved image to disk successfully!\n");

    // Free the allocated memory
    free(blue);
    if (allocated_title) {
        free(new_title);
    }
    stbi_image_free(img);

    return 0;
}

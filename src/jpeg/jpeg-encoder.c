#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <fec.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

#include "config.h"

typedef struct {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jvirt_barray_ptr *coeffs_array;
    FILE *file;
} JpegMatrixState;

typedef struct {
    int comp;
    int row;
    int col;
    int k;
} DctCoord;

// STEP 1
void rs_encode_payload(unsigned char payload[PAYLOAD_SIZE], unsigned char rs_payload[RS_ENC_PAYLOAD_SIZE]) {

    unsigned char parity[PARITY_SIZE];
	int pad_value = 255 - (PARITY_SIZE + PAYLOAD_SIZE);

    encode_rs_8(payload, parity, pad_value);

    memcpy(rs_payload, payload, PAYLOAD_SIZE);
    memcpy(rs_payload + PAYLOAD_SIZE, parity, PARITY_SIZE);
}

// Utility function
int get_bits(unsigned char *payload, int bit_idx) {

    // Locating First Bit
    int B1_idx = bit_idx / 8;
    int b1_offset = 7 - (bit_idx % 8);
    int b1 = (payload[B1_idx] >> b1_offset) & 1;

    // Locating Second Bit
    int nxt_bit_idx = bit_idx + 1;
    int B2_idx = nxt_bit_idx / 8;
    int B2_offset = 7 - (nxt_bit_idx % 8);
    int b2 = (payload[B2_idx] >> B2_offset) & 1;

    return (b1 << 1) | b2;
}

// STEP 2
JpegMatrixState extract_coefficients(const char *filename) {

    JpegMatrixState state;

    state.file = fopen(filename, "rb");
    if (!state.file) {
        fprintf(stderr, "Failed to open the image!");
        exit(1);
    }

    state.cinfo.err = jpeg_std_error(&state.jerr);
    jpeg_create_decompress(&state.cinfo);
    jpeg_stdio_src(&state.cinfo, state.file);

    jpeg_read_header(&state.cinfo, TRUE);
    state.coeffs_array = jpeg_read_coefficients(&state.cinfo);

    return state;
}

// STEP 4
void scatter(DctCoord *coord_list, int total_valid, unsigned int secret_key) {

    srand(secret_key);
    
    for (int i = total_valid - 1; i > 0; i--) {
        int j = rand() % (i + 1);

        DctCoord temp = coord_list[i];
        coord_list[i] = coord_list[j];
        coord_list[j] = temp;
    }
}

int extract_valid_coords(JpegMatrixState *state, DctCoord *hit_list) {
    int total_count = 0;
    
    for (int comp = 0; comp < state->cinfo.num_components; comp++) {
        jpeg_component_info *compptr = state->cinfo.comp_info + comp;
        
        for (JDIMENSION row = 0; row < compptr->height_in_blocks; row++) {
            for (JDIMENSION col = 0; col < compptr->width_in_blocks; col++) {
                // Still skip k=0 (DC coefficient), but grab ALL 63 ACs!
                for (int k = 1; k < 64; k++) {
                    hit_list[total_count].comp = comp;
                    hit_list[total_count].row = row;
                    hit_list[total_count].col = col;
                    hit_list[total_count].k = k;
                    total_count++;
                }
            }
        }
    }
    
    return total_count;
}

// STEP 5
void inject_payload(JpegMatrixState *state, DctCoord *coord_list, int valid_count, unsigned char *rs_payload, int total_bits) {

    int coord_idx = 0;
    int payload_idx = 0;

    DctCoord active[3];
    int active_count = 0;

    while (payload_idx < total_bits) {

        while (active_count < 3 && coord_idx < valid_count) {
            DctCoord c = coord_list[coord_idx++];

            JBLOCKARRAY buf = (state->cinfo.mem->access_virt_barray)((j_common_ptr)&state->cinfo, state->coeffs_array[c.comp], c.row, 1, TRUE);
            if (buf[0][c.col][c.k] != 0) {
                active[active_count++] = c;
            }
        }

        if (active_count < 3) {
            printf("WARNING: Matrix capacity exhausted!\n");
            break;
        }

        int m = get_bits(rs_payload, payload_idx);
        int b[3];
        JCOEF *blocks[3];

        for (int i = 0; i < 3; i++) {
            JBLOCKARRAY buf = (state->cinfo.mem->access_virt_barray)((j_common_ptr)&state->cinfo, state->coeffs_array[active[i].comp], active[i].row, 1, TRUE);
            blocks[i] = buf[0][active[i].col];
            b[i] = abs(blocks[i][active[i].k]) & 1;
        }

        int syn = (b[0] * 1) ^ (b[1] * 2) ^ (b[2] * 3);
        int target_idx = syn ^ m;

        if (target_idx != 0) {

            int target = target_idx - 1;
            int k = active[target].k;

            if (blocks[target][k] > 0)
                blocks[target][k]--;
            else
                blocks[target][k]++;

            if (blocks[target][k] == 0) {

                for (int i = target; i < 2; i++) {
                    active[i] = active[i + 1];
                }
                active_count--;
                continue;
            }
        }

        payload_idx += 2;
        active_count = 0;
    }

    printf("Payload successfully injected!\n");
}

// STEP 6
int save_image(JpegMatrixState *state, const char *out_fname) {

    struct jpeg_compress_struct cinfo_out;
    struct jpeg_error_mgr jerr_out;

    FILE *ofile = fopen(out_fname, "wb");
    if (!ofile) {
        printf("Error: Cannot create output image!\n");
        return 0;
    }

    cinfo_out.err = jpeg_std_error(&jerr_out);
    jpeg_create_compress(&cinfo_out);
    jpeg_stdio_dest(&cinfo_out, ofile);

    jpeg_copy_critical_parameters(&state->cinfo, &cinfo_out);
    jpeg_write_coefficients(&cinfo_out, state->coeffs_array);

    jpeg_finish_compress(&cinfo_out);
    jpeg_destroy_compress(&cinfo_out);
    fclose(ofile);

    printf("Image saved to disk successfully!\n");
    return 1;
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
                fprintf(stderr, "Error: Invalid argument passed to JPEG encoder.\n");
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

    // Parse secret key as unsigned int
    char *endptr = NULL;
    unsigned int secret_key = (unsigned int)strtoul(key_str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Provide a valid integer for the secret key!\n");
        return 1;
    }

    // Populate clean payload buffer (PAYLOAD_SIZE bytes)
    unsigned char clean_payload[PAYLOAD_SIZE] = {0};

    if (sig_file) {
        FILE *fp = fopen(sig_file, "rb");
        if (!fp) {
            perror("Error opening signature file");
            return 1;
        }
        size_t read_bytes = fread(clean_payload, 1, PAYLOAD_SIZE, fp);
        fclose(fp);
        if (read_bytes == 0) {
            fprintf(stderr, "Error: Signature file is empty.\n");
            return 1;
        }
    } else {
        int arg_len = strlen(msg_str);
        if (arg_len > PAYLOAD_SIZE) {
            arg_len = PAYLOAD_SIZE;
        }
        memcpy(clean_payload, msg_str, arg_len);
    }

    // Determine output title: use -o if provided, otherwise default to "F_<input>"
    char *new_title = NULL;
    bool allocated_title = false;

    if (output_path) {
        new_title = output_path;
    } else {
        int new_len = strlen(input_path) + 3;
        new_title = (char *)malloc(new_len);
        if (!new_title) {
            fprintf(stderr, "Error: Memory allocation for image title failed!\n");
            return 1;
        }
        snprintf(new_title, new_len, "F_%s", input_path);
        allocated_title = true;
    }

    if (verbose) {
        printf("[JPEG-ENC] Input:   %s\n", input_path);
        printf("[JPEG-ENC] Output:  %s\n", new_title);
        printf("[JPEG-ENC] Seed:    %u\n", secret_key);
    }

    // Extract coefficients from JPEG
    JpegMatrixState state = extract_coefficients(input_path);

    // Find maximum number of coordinates
    int max_possible_coords = 0;
    for (int comp = 0; comp < state.cinfo.num_components; comp++) {
        jpeg_component_info *compptr = state.cinfo.comp_info + comp;
        int total_blocks = compptr->width_in_blocks * compptr->height_in_blocks;
        max_possible_coords += (total_blocks * 63); // 63 valid AC indices per block
    }

    // Scatter the blocks and inject the payload
    unsigned char rs_payload[RS_ENC_PAYLOAD_SIZE] = {0};
    rs_encode_payload(clean_payload, rs_payload);

    DctCoord *coord_list = (DctCoord *)malloc(max_possible_coords * sizeof(DctCoord));
    if (!coord_list) {
        fprintf(stderr, "Error: Failed to allocate memory for coordinate list!\n");
        if (allocated_title) free(new_title);
        return 1;
    }

    int valid_count = extract_valid_coords(&state, coord_list);
    scatter(coord_list, valid_count, secret_key);
    inject_payload(&state, coord_list, valid_count, rs_payload, RS_ENC_PAYLOAD_SIZE * 8);

    save_image(&state, new_title);

    // Clean up
    jpeg_finish_decompress(&state.cinfo);
    jpeg_destroy_decompress(&state.cinfo);
    fclose(state.file);

    free(coord_list);
    if (allocated_title) {
        free(new_title);
    }

    return 0;
}

#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <fec.h>
#include <string.h>

// #include "bg_headers.h"
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

// STEP 3
int extract_valid_coords(JpegMatrixState *state, DctCoord *coord_list) {

    int valid_count = 0;

    for (int comp = 0; comp < state->cinfo.num_components; comp++) {
        jpeg_component_info *compptr = state->cinfo.comp_info + comp;

        for (unsigned int row = 0; row < compptr->height_in_blocks; row++) {

            JBLOCKARRAY buffer = (state->cinfo.mem->access_virt_barray)(
                (j_common_ptr) &state->cinfo,
                state->coeffs_array[comp],
                row, 1, TRUE
            );

            for (unsigned int col = 0; col < compptr->width_in_blocks; col++) {
                JCOEF *block = buffer[0][col];

                for (int k = 1; k < 64; k++) {

                    if (block[k] != 0) {
                        coord_list[valid_count].comp = comp;
                        coord_list[valid_count].col = col;
                        coord_list[valid_count].row = row;
                        coord_list[valid_count].k = k;

                        valid_count++;
                    }
                }
            }
        }
    }

    printf("Scan complete! %d payload injection sites available!\n", valid_count);

    return valid_count;
}

// STEP 5
void inject_payload(JpegMatrixState *state, DctCoord *coord_list, int valid_count, unsigned char *payload, int total_bits) {

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

        int m = get_bits(payload, payload_idx);
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

    if (argc != 4) {
        fprintf(stderr, "Error: Usage: ./jpeg-encoder <IMAGE_NAME> <MESSAGE> <SECRET_KEY>\n");
        return 1;
    }

    // Initialize variables
    unsigned int secret_key = (unsigned int)strtoul(argv[4], NULL, 10);

    unsigned char rs_paylod[RS_ENC_PAYLOAD_SIZE] = {0};
    unsigned char clean_payload[PAYLOAD_SIZE] = {0};
	int arg_len = strlen(argv[2]);

	if (arg_len > PAYLOAD_SIZE)
		arg_len = PAYLOAD_SIZE;

	memcpy(clean_payload, argv[2], arg_len);

    // Set output image title
    int new_len = strlen(argv[1]) + 3;
	char *new_title = (char *)malloc(new_len);

	if (!new_title) {
		printf("Error: Memory allocation for image title failed!\n");
		return 1;
	}

	new_title[0] = 'F';
	new_title[1] = '_';
	strcpy(&new_title[2], argv[1]);

    // Extract coefficients from JPEG
    JpegMatrixState state = extract_coefficients(argv[1]);

    // Find maximum number of coordinates
    int max_possible_coords = 0;
    for (int comp = 0; comp < state.cinfo.num_components; comp++) {
        jpeg_component_info *compptr = state.cinfo.comp_info + comp;
        
        int total_blocks = compptr->width_in_blocks * compptr->height_in_blocks;
        max_possible_coords += (total_blocks * 63); // 63 valid AC indices per block
    }

    // Scatter the blocks and inject the payload
    DctCoord *coord_list = (DctCoord *)malloc(max_possible_coords * sizeof(DctCoord));
    int valid_count = extract_valid_coords(&state, coord_list);
    scatter(coord_list, valid_count, secret_key);
    inject_payload(&state, coord_list, valid_count, rs_paylod, RS_ENC_PAYLOAD_SIZE * 8);

    save_image(&state, "stego.jpg");

    // Clean up
    jpeg_finish_decompress(&state.cinfo);
    jpeg_destroy_decompress(&state.cinfo);
    fclose(state.file);

    free(coord_list);

    return 0;
}

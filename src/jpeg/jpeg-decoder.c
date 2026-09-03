#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <string.h>
#include <fec.h>

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


void decode_message(unsigned char extracted_payload[RS_ENC_PAYLOAD_SIZE], char buffer[BUFFER_SIZE]) {

    int pad = 255 - (PARITY_SIZE + PAYLOAD_SIZE);

	int errors_fixed = decode_rs_8(extracted_payload, NULL, 0, pad);
    printf("Errors fixed: %d\n", errors_fixed);

	if (errors_fixed < 0) {
		printf("Data could not be recovered!\n");
	}
	else {
		printf("Successfully recovered %d errors!\n", errors_fixed);

		memcpy(buffer, extracted_payload, PAYLOAD_SIZE);
		buffer[PAYLOAD_SIZE] = '\0';

		printf("Message successfully decoded!\n");
	}
}

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

int extract_valid_coords(JpegMatrixState *state, DctCoord *coord_list) {
    int total_count = 0;
    
    for (int comp = 0; comp < state->cinfo.num_components; comp++) {
        jpeg_component_info *compptr = state->cinfo.comp_info + comp;
        
        for (JDIMENSION row = 0; row < compptr->height_in_blocks; row++) {
            for (JDIMENSION col = 0; col < compptr->width_in_blocks; col++) {
                
                for (int k = 1; k < 64; k++) {
                    coord_list[total_count].comp = comp;
                    coord_list[total_count].row = row;
                    coord_list[total_count].col = col;
                    coord_list[total_count].k = k;
                    total_count++;
                }
            }
        }
    }
    
    return total_count;
}

void scatter(DctCoord *coord_list, int total_valid, unsigned int secret_key) {

    srand(secret_key);
    
    for (int i = total_valid - 1; i > 0; i--) {
        int j = rand() % (i + 1);

        DctCoord temp = coord_list[i];
        coord_list[i] = coord_list[j];
        coord_list[j] = temp;
    }
}

void put_bits(unsigned char *payload, int bit_idx, int val) {

    int B1_idx = bit_idx / 8;
    int b1_offset = 7 - (bit_idx % 8);
    int b1 = (val >> 1) & 1;

    payload[B1_idx] &= ~(1 << b1_offset);
    payload[B1_idx] |= (b1 << b1_offset);

    int next = bit_idx + 1;
    int B2_idx = next / 8;
    int b2_offset = 7 - (next % 8);
    int b2 = val & 1;

    payload[B2_idx] &= ~(1 << b2_offset);
    payload[B2_idx] |= (b2 << b2_offset);
}

void extract_payload(JpegMatrixState *state, DctCoord *coord_list, int total_coords, unsigned char *payload, int total_bits) {
    int hit_idx = 0;
    int payload_idx = 0;
    DctCoord active[3];

    while (payload_idx < total_bits && hit_idx < total_coords) {
        int active_count = 0;

        while (active_count < 3 && hit_idx < total_coords) {
            DctCoord c = coord_list[hit_idx++];
            
            JBLOCKARRAY buf = (state->cinfo.mem->access_virt_barray)((j_common_ptr)&state->cinfo, state->coeffs_array[c.comp], c.row, 1, FALSE);
            
            if (buf[0][c.col][c.k] != 0) {
                active[active_count++] = c;
            }
        }

        if (active_count < 3) break;

        int b0 = abs( ((state->cinfo.mem->access_virt_barray)((j_common_ptr)&state->cinfo, state->coeffs_array[active[0].comp], active[0].row, 1, FALSE))[0][active[0].col][active[0].k] ) & 1;
        int b1 = abs( ((state->cinfo.mem->access_virt_barray)((j_common_ptr)&state->cinfo, state->coeffs_array[active[1].comp], active[1].row, 1, FALSE))[0][active[1].col][active[1].k] ) & 1;
        int b2 = abs( ((state->cinfo.mem->access_virt_barray)((j_common_ptr)&state->cinfo, state->coeffs_array[active[2].comp], active[2].row, 1, FALSE))[0][active[2].col][active[2].k] ) & 1;

        int syndrome = (b0 * 1) ^ (b1 * 2) ^ (b2 * 3);
        put_bits(payload, payload_idx, syndrome);
        payload_idx += 2;
    }
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Error: usage is: ./jd <FILENAME> <SECRET_KEY>");
        return 1;
    }

    // Initialize variables
    unsigned int secret_key = (unsigned int)strtoul(argv[2], NULL, 10);
    unsigned char extracted_payload[RS_ENC_PAYLOAD_SIZE];

    JpegMatrixState state = extract_coefficients(argv[1]);

    // Find maximum number of coordinates
    int max_possible_coords = 0;
    for (int comp = 0; comp < state.cinfo.num_components; comp++) {
        jpeg_component_info *compptr = state.cinfo.comp_info + comp;
        
        int total_blocks = compptr->width_in_blocks * compptr->height_in_blocks;
        max_possible_coords += (total_blocks * 63); // 63 valid AC indices per block
    }

    DctCoord *coord_list = (DctCoord *)malloc(max_possible_coords * sizeof(DctCoord));
    int valid_count = extract_valid_coords(&state, coord_list);
    scatter(coord_list, valid_count, secret_key);
    extract_payload(&state, coord_list, valid_count, extracted_payload, RS_ENC_PAYLOAD_SIZE * 8);

    char message[BUFFER_SIZE];
    decode_message(extracted_payload, message);

    printf("\nMessage: %s\n", message);

    jpeg_finish_decompress(&state.cinfo);
    jpeg_destroy_decompress(&state.cinfo);
    fclose(state.file);

    free(coord_list);

    return 0;
}

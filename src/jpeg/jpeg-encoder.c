#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

typedef struct {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jvirt_barray_ptr *coeffs_array;
    FILE *file;
} JpegMatrixState;

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

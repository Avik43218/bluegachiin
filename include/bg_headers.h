#ifndef BG_HEADERS_H
#define BG_HEADERS_H

#include "config.h"

void init_dsss(unsigned int secret_key);
int get_pn_chip();

unsigned char* open_img(char *img_path, int *w, int *h, int *c);

void haar_1d(double *data, int len);
void inv_haar_1d(double *data, int len);

void encode_rs_dsss(double *data, unsigned char payload[PAYLOAD_SIZE], int width, unsigned int secret_key);
void decode_rs_dsss(double *data, char message[PAYLOAD_SIZE], int width, unsigned int secret_key);

#endif


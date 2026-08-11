CC			:= gcc
CFLAGS			:= -Wall -Wextra -std=c11 -D_XOPEN_SOURCE=500 -Iinclude -I/usr/local/include -I/usr/include
LDFLAGS			:= -L/usr/local/lib -L/usr/lib
LDLIBS			:= -Wl,-Bstatic -lfec -Wl,-Bdynamic -lm

SRC_DIR			:= src
BIN_DIR			:= bin
ASSET_DIR		:= assets

all:			$(BIN_DIR)/bg $(BIN_DIR)/png-encoder $(BIN_DIR)/png-decoder

$(BIN_DIR)/bg:		$(SRC_DIR)/main.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

$(BIN_DIR)/encoder:	$(SRC_DIR)/png/png-encoder.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

$(BIN_DIR)/decoder:	$(SRC_DIR)/png/png-decoder.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(BIN_DIR) $(ASSET_DIR)/F_*.png

.PHONY: all clean


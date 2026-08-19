CC				:= gcc
CFLAGS			:= -O2 -g --coverage -pg -Wall -Wextra -std=c11 -D_XOPEN_SOURCE=500 -Iinclude -I/usr/local/include -I/usr/include
LDFLAGS			:= --coverage -pg -L/usr/local/lib -L/usr/lib
LDLIBS_PNG		:= -Wl,-Bdynamic -lm -lfec
LDLIBS_JPG		:= -Wl,-Bdynamic -ljpeg -lfec

SRC_DIR			:= src
BIN_DIR			:= bin
ASSET_DIR		:= assets

all:			$(BIN_DIR)/bg $(BIN_DIR)/pe $(BIN_DIR)/pd $(BIN_DIR)/je

$(BIN_DIR)/bg:		$(SRC_DIR)/main.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/pe:	$(SRC_DIR)/png/png-encoder.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS_PNG)

$(BIN_DIR)/pd:	$(SRC_DIR)/png/png-decoder.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS_PNG)

$(BIN_DIR)/je:	$(SRC_DIR)/jpeg/jpeg-encoder.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS_JPG)

clean:
	rm -rf $(BIN_DIR) $(ASSET_DIR)/F_*.png $(ASSET_DIR)/F_*.jpg ./gmon.out

.PHONY: all clean

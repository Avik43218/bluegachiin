CC			:= gcc
CFLAGS			:= -Wall -Wextra -std=c11 -Iinclude -I/usr/local/include -I/usr/include -MMD -MP
LDFLAGS			:= -L/usr/local/lib -L/usr/lib
LDLIBS			:= -lfec -lm

SRC_DIR			:= src
BUILD_DIR		:= build
BIN_DIR			:= bin

TARGET			:= $(BIN_DIR)/bg

SRCS			:= $(wildcard $(SRC_DIR)/*.c)
OBJS			:= $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS			:= $(OBJS:.o=.d)

all:			$(TARGET)

$(TARGET):		$(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o:	$(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean


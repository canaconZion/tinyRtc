#==============================================================================
# tinyRtc Makefile
# A lightweight WebRTC implementation
#==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -D_GNU_SOURCE -I.
TARGET = bin/tinyrtc
OBJ_DIR = obj
BIN_DIR = bin

SRCS = main.c platform.c tiny_rtc.c tiny_rtcp.c tiny_rtp.c \
       tiny_dtls.c tiny_ice.c tiny_sctp.c tiny_sdp.c \
       tiny_srtp.c tiny_stun.c tiny_turn.c

OBJS = $(addprefix $(OBJ_DIR)/, $(SRCS:.c=.o))

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build complete!"

$(OBJ_DIR)/%.o: %.c | dirs
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

rebuild: clean all

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean dirs rebuild run
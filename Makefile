CC = gcc

SRC_DIR = src
LIB_DIR = lib
BUILD_DIR = build

CLIENT = blocks
SERVER = server

CFLAGS = -O2 -I$(SRC_DIR) -I$(LIB_DIR)
LDFLAGS = -lglfw -lGL -lcglm -lm

CSRCS = $(shell find $(SRC_DIR)/client -name '*.c')
SSRCS = $(shell find $(SRC_DIR)/server -name '*.c')
LIB_SRCS = $(shell find $(LIB_DIR) -name '*.c')

COBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CSRCS) $(LIB_SRCS))
SOBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SSRCS) $(LIB_SRCS))

all: $(CLIENT) $(SERVER)

$(CLIENT): $(COBJS)
	$(CC) $(COBJS) $(LDFLAGS) -o $(CLIENT)

$(SERVER): $(SOBJS)
	$(CC) $(SOBJS) $(LDFLAGS) -o $(SERVER)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(CLIENT) $(SERVER)

.PHONY: all clean run-client run-server
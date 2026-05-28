CC = gcc

SRC_DIR = src
LIB_DIR = lib
BUILD_DIR = build

CLIENT = blocks
SERVER = server

CFLAGS = -O2 -I$(SRC_DIR) -I$(SRC_DIR)/client -I$(LIB_DIR)
LDFLAGS = -lglfw -lGL -lcglm -lm

CSRCS = $(shell find $(SRC_DIR)/client -name '*.c')
SSRCS = $(shell find $(SRC_DIR)/server -name '*.c')
LIB_SRCS = $(shell find $(LIB_DIR) -name '*.c')

COBJS = $(addprefix $(BUILD_DIR)/, $(CSRCS:.c=.o))
SOBJS = $(addprefix $(BUILD_DIR)/, $(SSRCS:.c=.o))
LIB_OBJS = $(addprefix $(BUILD_DIR)/, $(LIB_SRCS:.c=.o))

CLIENT_OBJS = $(COBJS) $(LIB_OBJS)
SERVER_OBJS = $(SOBJS) $(LIB_OBJS)

all: $(CLIENT) $(SERVER)

$(CLIENT): $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) $(LDFLAGS) -o $(CLIENT)

$(SERVER): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) $(LDFLAGS) -o $(SERVER)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(CLIENT) $(SERVER)

.PHONY: all clean run-client run-server
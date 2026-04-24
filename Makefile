CC = gcc

SRC_DIR = src
LIB_DIR = lib
BUILD_DIR = build

TARGET = blocks

CFLAGS = -O2 -I$(SRC_DIR) -I$(LIB_DIR)
LDFLAGS = -lglfw -lGL -lcglm -lm

SRCS = $(shell find $(SRC_DIR) -name '*.c') \
       $(shell find $(LIB_DIR) -name '*.c')

HDRS = $(shell find $(SRC_DIR) -name '*.h') \
       $(shell find $(LIB_DIR) -name '*.h')

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(BUILD_DIR)/%.o: %.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run:
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean
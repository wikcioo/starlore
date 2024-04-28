CC := gcc

WARNINGS := -Wall -Wpedantic -Wshadow

BUILD_DIR  := build
SRC_DIR    := src
CLIENT_DIR := src/client
SERVER_DIR := src/server
VENDOR_DIR := src/vendor

CLIENT_SOURCES := $(wildcard $(CLIENT_DIR)/*.c)
CLIENT_SOURCES += $(wildcard $(VENDOR_DIR)/glad/src/*.c)
SERVER_SOURCES := $(wildcard $(SERVER_DIR)/*.c)

CLIENT_OBJECTS := $(addprefix $(BUILD_DIR)/client/, $(addsuffix .c.o, $(basename $(notdir $(CLIENT_SOURCES)))))
SERVER_OBJECTS := $(addprefix $(BUILD_DIR)/server/, $(addsuffix .c.o, $(basename $(notdir $(SERVER_SOURCES)))))

CLIENT_LD_FLAGS := -lglfw

all:
	@echo "Building all..."
	@make --no-print-directory client
	@make --no-print-directory server

client:
	@echo "Building client..."
	@mkdir -p $(BUILD_DIR)/client
	@make --no-print-directory $(BUILD_DIR)/client/client

server:
	@echo "Building server..."
	@mkdir -p $(BUILD_DIR)/server
	@make --no-print-directory $(BUILD_DIR)/server/server

$(BUILD_DIR)/client/client: $(CLIENT_OBJECTS)
	$(CC) $^ $(CLIENT_LD_FLAGS) -o $@

$(BUILD_DIR)/server/server: $(SERVER_OBJECTS)
	$(CC) $^ -o $@

$(BUILD_DIR)/client/%.c.o: $(CLIENT_DIR)/%.c
	$(CC) -c $(WARNINGS) -I$(SRC_DIR) -I$(VENDOR_DIR)/glad/include $^ -o $@

$(BUILD_DIR)/client/%.c.o: $(VENDOR_DIR)/glad/src/%.c
	$(CC) -c -I$(VENDOR_DIR)/glad/include $^ -o $@

$(BUILD_DIR)/server/%.c.o: $(SERVER_DIR)/%.c
	$(CC) -c $(WARNINGS) -I$(SRC_DIR) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)

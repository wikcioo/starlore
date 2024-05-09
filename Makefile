ifndef config
    config=debug
endif

CC := clang

WARNINGS := -Wall -Wpedantic -Wshadow -Wno-gnu-zero-variadic-macro-arguments -Wno-language-extension-token

BUILD_DIR  := build/$(config)
SRC_DIR    := src
CLIENT_DIR := src/client
SERVER_DIR := src/server
VENDOR_DIR := src/vendor
COMMON_DIR := src/common

CLIENT_SOURCES := $(wildcard $(CLIENT_DIR)/*.c)
CLIENT_SOURCES += $(wildcard $(VENDOR_DIR)/glad/src/*.c)
SERVER_SOURCES := $(wildcard $(SERVER_DIR)/*.c)
COMMON_SOURCES := $(wildcard $(COMMON_DIR)/*.c)
COMMON_SOURCES += $(wildcard $(COMMON_DIR)/containers/*.c)

CLIENT_OBJECTS := $(addprefix $(BUILD_DIR)/client/, $(addsuffix .c.o, $(basename $(notdir $(CLIENT_SOURCES)))))
SERVER_OBJECTS := $(addprefix $(BUILD_DIR)/server/, $(addsuffix .c.o, $(basename $(notdir $(SERVER_SOURCES)))))
COMMON_OBJECTS := $(addprefix $(BUILD_DIR)/common/, $(addsuffix .c.o, $(basename $(notdir $(COMMON_SOURCES)))))

ifeq ($(config), debug)
    CFLAGS := -DDEBUG -DENABLE_ASSERTIONS -g
endif
ifeq ($(config), release)
    CFLAGS := -DNDEBUG -O3
endif

all:
	@echo "($(config)) Building all..."
	@make --no-print-directory client
	@make --no-print-directory server

client:
	@echo "($(config)) Building client..."
	@mkdir -p $(BUILD_DIR)/client $(BUILD_DIR)/common
	@make --no-print-directory $(BUILD_DIR)/client/client

server:
	@echo "($(config)) Building server..."
	@mkdir -p $(BUILD_DIR)/server $(BUILD_DIR)/common
	@make --no-print-directory $(BUILD_DIR)/server/server

$(BUILD_DIR)/client/client: $(CLIENT_OBJECTS) $(COMMON_OBJECTS)
	$(CC) $^ -lglfw -lfreetype -lm -o $@

$(BUILD_DIR)/server/server: $(SERVER_OBJECTS) $(COMMON_OBJECTS)
	$(CC) $^ -lm -o $@

$(BUILD_DIR)/client/%.c.o: $(CLIENT_DIR)/%.c
	$(CC) -c $(WARNINGS) $(CFLAGS) -I$(SRC_DIR) -I$(VENDOR_DIR)/glad/include -I/usr/include/freetype2 $^ -o $@

$(BUILD_DIR)/client/%.c.o: $(VENDOR_DIR)/glad/src/%.c
	$(CC) -c -I$(VENDOR_DIR)/glad/include $^ -o $@

$(BUILD_DIR)/server/%.c.o: $(SERVER_DIR)/%.c
	$(CC) -c $(WARNINGS) $(CFLAGS) -I$(SRC_DIR) $^ -o $@

$(BUILD_DIR)/common/%.c.o: $(COMMON_DIR)/%.c
	$(CC) -c $(WARNINGS) $(CFLAGS) -I$(SRC_DIR) $^ -o $@

$(BUILD_DIR)/common/%.c.o: $(COMMON_DIR)/containers/%.c
	$(CC) -c $(WARNINGS) $(CFLAGS) -I$(SRC_DIR) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)

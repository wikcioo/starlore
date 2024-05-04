CC := clang

WARNINGS := -Wall -Wpedantic -Wshadow -Wno-gnu-zero-variadic-macro-arguments -Wno-language-extension-token

BUILD_DIR := build
TESTS_DIR := src
COMMON_DIR := ../src/common

TEST_SOURCES := $(wildcard $(TESTS_DIR)/containers/*.c)
TEST_OBJECTS := $(addprefix $(BUILD_DIR)/, $(addsuffix .c.o, $(basename $(notdir $(TEST_SOURCES)))))

COMMON_SOURCES := $(COMMON_DIR)/logger.c
COMMON_SOURCES += $(COMMON_DIR)/containers/darray.c
COMMON_SOURCES += $(COMMON_DIR)/containers/ring_buffer.c
COMMON_OBJECTS := $(addprefix $(BUILD_DIR)/, $(addsuffix .c.o, $(basename $(notdir $(COMMON_SOURCES)))))

MANAGER_SOURCES := $(wildcard *.c)
MANAGER_OBJECTS := $(addprefix $(BUILD_DIR)/, $(addsuffix .c.o, $(basename $(notdir $(MANAGER_SOURCES)))))

CFLAGS := -DDEBUG -DENABLE_ASSERTIONS -g

all:
	@echo "Building test suite..."
	@mkdir -p $(BUILD_DIR)
	@make --no-print-directory $(BUILD_DIR)/test_suite

$(BUILD_DIR)/test_suite: $(TEST_OBJECTS) $(MANAGER_OBJECTS) $(COMMON_OBJECTS)
	$(CC) $^ -o $@

$(BUILD_DIR)/%.c.o: $(TESTS_DIR)/containers/%.c
	$(CC) -c $(WARNINGS) $(CFLAGS) -I../src $^ -o $@

$(BUILD_DIR)/%.c.o: ./%.c
	$(CC) -c $(WARNINGS) $(CFLAGS) -I../src $^ -o $@

$(BUILD_DIR)/%.c.o: $(COMMON_DIR)/%.c
	$(CC) -c $(WARNINGS) $(CFLAGS) -I../src $^ -o $@

$(BUILD_DIR)/%.c.o: $(COMMON_DIR)/containers/%.c
	$(CC) -c $(WARNINGS) $(CFLAGS) -I../src $^ -o $@

clean:
	rm -rf $(BUILD_DIR)

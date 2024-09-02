##
# CLI
#
# @file
# @version 0.1


PUBLIC_HEADERS := headers
UNITY_ROOT := lib/Unity

CFLAGS := -g -Wall -std=c11
SRC_DIR := src

TEST_DIR := test
EXAMPLE_DIR := example
INC_DIRS=-Isrc -I$(TEST_DIR) -I$(UNITY_ROOT)/src -I$(PUBLIC_HEADERS)
TESTS := test_cb
UNITY = $(UNITY_ROOT)/src/unity.c

OBJECTS := $(patsubst %.c,%.o,$(wildcard $(SRC_DIR)/*.c))
TEST_TARGETS:= $(patsubst %.c,%.out,$(wildcard $(TEST_DIR)/*.c))
EXAMPLE_TARGETS:= $(patsubst %.c,%.out,$(wildcard $(EXAMPLE_DIR)/*.c))
HEADERS:= $(patsubst %.c,%.out,$(wildcard $(PUBLIC_HEADERS)/*.h))

TARGET_EXTENSION=.out

CLEANUP = rm -f

all: clean test
	@echo $(TEST_OBJECTS)

clean:
	@echo Cleaning
	@rm -f $(SRC_DIR)/*.o
	@rm -f $(TEST_DIR)/*.out
	@rm -f $(TEST_TARGETS)


$(OBJECTS): %.o: %.c $(HEADERS)
	@$(CC) -c $(CFLAGS) $(INC_DIRS) $< -o $@

$(SRC_DIR)/main: %: %.c $(OBJECTS)
	@$(CC) $(CFLAGS) $(INC_DIRS) $< $(OBJECTS) -o $@
	@chmod u+x $(SRC_DIR)/main

$(TEST_TARGETS): %.out: %.c $(OBJECTS)
	@echo $(TEST_TARGETS)
	$(CC) $(CFLAGS) $(INC_DIRS) -o $@ $^ $(UNITY)
	@echo ============================================
	@echo Running Test: $<
	@echo ============================================
	-./$@

$(EXAMPLE_TARGETS): %.out: %.c $(OBJECTS)
	@echo ============================================
	@echo Building Example: $<
	@echo ============================================
	$(CC) $(CFLAGS) $(INC_DIRS) -o $@ $^

test: $(TEST_TARGETS)

examples: $(EXAMPLE_TARGETS)
# end

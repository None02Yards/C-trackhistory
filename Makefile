
# ----- config -----
CC       ?= gcc
BUILD    ?= release        # release | debug
SRC_DIR   = src/app
INC_DIR   = src/include
OBJ_DIR   = build
TARGET    = browser

# Common warnings + include path
CFLAGS_COMMON = -std=c11 -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -I$(INC_DIR)
# Auto-deps: generate .d files alongside .o
CFLAGS_DEPS   = -MMD -MP

ifeq ($(BUILD),debug)
  CFLAGS_OPT  = -O0 -g
else
  CFLAGS_OPT  = -O2
endif

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_OPT) $(CFLAGS_DEPS)
LDFLAGS =

# ----- sources/objects -----
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ----- rules -----
.PHONY: all clean run debug release

all: $(TARGET)

# Link
$(TARGET): $(OBJ_DIR) $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile (auto creates build dir)
$(OBJ_DIR):
	@mkdir -p "$(OBJ_DIR)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Handy shortcuts
run: $(TARGET)
	./$(TARGET)

debug:
	$(MAKE) BUILD=debug

release:
	$(MAKE) BUILD=release

clean:
	$(RM) -r "$(OBJ_DIR)" $(TARGET)

# Include auto-generated header deps if present
-include $(DEPS)

CXX := g++
CC  := gcc
AR  := ar

CXXFLAGS := -Og -g -Wall -Wextra -Werror -std=gnu++23 -fconcepts-diagnostics-depth=2
CFLAGS   := -Og -g -Wall -Wextra -Werror -fPIC
DEPFLAGS  = -MMD -MP -MF $(@:.o=.d)

BUILD_DIR := buildout
OBJ_DIR   := $(BUILD_DIR)/obj

# --- libtproc (from submodule) -------------------------------------------

LIBTPROC_DIR  := threadprocs/libtproc
LIBTPROC_A    := $(BUILD_DIR)/libtproc.a
LIBTPROC_SRCS := $(LIBTPROC_DIR)/tproc.c
LIBTPROC_OBJS := $(LIBTPROC_SRCS:$(LIBTPROC_DIR)/%.c=$(OBJ_DIR)/libtproc/%.o)

# --- libtpactor -----------------------------------------------------------

LIB        := $(BUILD_DIR)/libtpactor.a
LIB_SRCS   := $(wildcard src/*.cpp)
LIB_OBJS   := $(LIB_SRCS:src/%.cpp=$(OBJ_DIR)/tpactor/%.o)
LIB_CFLAGS := -Iinclude -I$(LIBTPROC_DIR)

# --- Examples --------------------------------------------------------------

EXAMPLE_SRCS := $(wildcard examples/*.cpp)
EXAMPLE_BINS := $(EXAMPLE_SRCS:examples/%.cpp=$(BUILD_DIR)/examples/%)
EXAMPLE_OBJS := $(EXAMPLE_SRCS:examples/%.cpp=$(OBJ_DIR)/examples/%.o)

# ======================================================================

EXAMPLE_SCRIPTS := $(wildcard examples/*.sh)

.PHONY: all lib examples test clean

all: lib examples

lib: $(LIB)

examples: $(EXAMPLE_BINS)

clean:
	rm -rf $(BUILD_DIR)

test: examples
	@for s in $(EXAMPLE_SCRIPTS); do \
		echo ">>> Running $$s ..."; \
		bash "$$s" || exit 1; \
	done

# --- libtproc build -------------------------------------------------------

$(OBJ_DIR)/libtproc/%.o: $(LIBTPROC_DIR)/%.c | $(OBJ_DIR)/libtproc
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(LIBTPROC_A): $(LIBTPROC_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

# --- libtpactor build -----------------------------------------------------

$(OBJ_DIR)/tpactor/%.o: src/%.cpp | $(OBJ_DIR)/tpactor
	$(CXX) $(CXXFLAGS) $(LIB_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(LIB): $(LIB_OBJS) $(LIBTPROC_A) | $(BUILD_DIR)
	rm -f $@
	$(AR) rcs $@ $(LIB_OBJS) $(LIBTPROC_OBJS)

# --- Example build (each .cpp produces one binary) -------------------------

EXAMPLE_CFLAGS := -Iinclude -Iexamples

$(OBJ_DIR)/examples/%.o: examples/%.cpp | $(OBJ_DIR)/examples
	$(CXX) $(CXXFLAGS) $(EXAMPLE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/%: $(OBJ_DIR)/examples/%.o $(LIB) | $(BUILD_DIR)/examples
	$(CXX) $(CXXFLAGS) -o $@ $< -L$(BUILD_DIR) -ltpactor

# --- Directory creation ----------------------------------------------------

$(BUILD_DIR) $(BUILD_DIR)/examples $(OBJ_DIR)/libtproc $(OBJ_DIR)/tpactor $(OBJ_DIR)/examples:
	mkdir -p $@

# --- Auto-generated header dependencies ------------------------------------

ALL_DEPS := $(patsubst %.o,%.d,$(LIB_OBJS) $(LIBTPROC_OBJS) $(EXAMPLE_OBJS))
-include $(ALL_DEPS)

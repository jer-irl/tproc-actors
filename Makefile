CXX := g++
CC  := gcc
AR  := ar

CXXFLAGS := -Og -g -Wall -Wextra -Werror -std=gnu++23
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

EXAMPLE_DIRS := $(foreach d,$(wildcard examples/*/.),\
                  $(if $(wildcard $(dir $(d))*.cpp),$(d)))
EXAMPLE_BINS := $(foreach d,$(EXAMPLE_DIRS),\
                  $(BUILD_DIR)/examples/$(notdir $(patsubst %/.,%,$(d))))

# ======================================================================

.PHONY: all lib examples clean

all: lib examples

lib: $(LIB)

examples: $(EXAMPLE_BINS)

clean:
	rm -rf $(BUILD_DIR)

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

# --- Example build (each subdir produces one binary) ----------------------

define EXAMPLE_template
$(BUILD_DIR)/examples/$(1): $(wildcard examples/$(1)/*.cpp) $(LIB) | $(BUILD_DIR)/examples
	$(CXX) $(CXXFLAGS) -Iinclude -Iexamples -o $$@ $(wildcard examples/$(1)/*.cpp) -L$(BUILD_DIR) -ltpactor
endef

$(foreach d,$(EXAMPLE_DIRS),\
  $(eval $(call EXAMPLE_template,$(notdir $(patsubst %/.,%,$(d))))))

# --- Directory creation ----------------------------------------------------

$(BUILD_DIR) $(BUILD_DIR)/examples $(OBJ_DIR)/libtproc $(OBJ_DIR)/tpactor:
	mkdir -p $@

# --- Auto-generated header dependencies ------------------------------------

ALL_DEPS := $(patsubst %.o,%.d,$(LIB_OBJS) $(LIBTPROC_OBJS))
-include $(ALL_DEPS)

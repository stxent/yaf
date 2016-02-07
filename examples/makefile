#Copyright (C) 2015 xent
#Project is distributed under the terms of the GNU General Public License v3.0

EXAMPLES_DIR = examples

ifeq ($(CONFIG_LIBSHELL),y)
  LIB_EXAMPLES += libshell
endif
ifeq ($(CONFIG_SHELL),y)
  BIN_EXAMPLES += shell
endif

BIN_EXAMPLE_FILES = $(BIN_EXAMPLES:%=$(OUTPUT_DIR)/bin/%)
LIB_EXAMPLE_FILES = $(LIB_EXAMPLES:%=$(OUTPUT_DIR)/%.a)
TARGETS += $(BIN_EXAMPLE_FILES) $(LIB_EXAMPLE_FILES)

ifneq ($(BIN_EXAMPLES) $(LIB_EXAMPLES),)
  INCLUDE_PATH += -I$(EXAMPLES_DIR)
endif
ifneq ($(BIN_EXAMPLES),)
  LDLIBS += -lshell
endif

define make-example
  $(1)_CSOURCES := $(shell find $(EXAMPLES_DIR)/$(1) -name *.c)
  CSOURCES += $$($(1)_CSOURCES)
  $(1)_CXXSOURCES := $(shell find $(EXAMPLES_DIR)/$(1) -name *.cpp)
  CXXSOURCES += $$($(1)_CXXSOURCES)
  $(1)_OBJECTS += $$($(1)_CSOURCES:%.c=$(OUTPUT_DIR)/%.o) $$($(1)_CXXSOURCES:%.cpp=$(OUTPUT_DIR)/%.o)
endef

$(foreach entry,$(BIN_EXAMPLES) $(LIB_EXAMPLES),$(eval $(call make-example,$(entry))))

.SECONDEXPANSION:
$(BIN_EXAMPLE_FILES): $(LIBRARY_FILE) $(LIB_EXAMPLE_FILES) $$($$(notdir $$@)_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(LIB_EXAMPLE_FILES): $$($$(patsubst $(OUTPUT_DIR)/%.a,%,$$@)_OBJECTS)
	@mkdir -p $(@D)
	$(AR) -r $@ $^

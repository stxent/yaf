#Copyright (C) 2015 xent
#Project is distributed under the terms of the GNU General Public License v3.0

EXAMPLES_DIR = examples
EXAMPLES := $(shell find $(EXAMPLES_DIR) -mindepth 1 -maxdepth 1 -type d -printf "%f\n" | grep -v shared)
EXAMPLE_FILES = $(addprefix $(OUTPUTDIR)/bin/,$(EXAMPLES))
TARGETS += $(EXAMPLE_FILES)

INCLUDEPATH += -I$(EXAMPLES_DIR)/shared

CSOURCES_SHARED := $(shell find $(EXAMPLES_DIR)/shared os -name *.c)
CSOURCES += $(CSOURCES_SHARED)
CXXSOURCES_SHARED := $(shell find $(EXAMPLES_DIR)/shared -name *.cpp)
CXXSOURCES += $(CXXSOURCES_SHARED)

define make-example
  $(1)_CSOURCES := $(shell find $(EXAMPLES_DIR)/$(1) -name *.c)
  CSOURCES += $$($(1)_CSOURCES)
  $(1)_CXXSOURCES := $(shell find $(EXAMPLES_DIR)/$(1) -name *.cpp)
  CXXSOURCES += $$($(1)_CXXSOURCES)
  $(1)_OBJECTS += $(CSOURCES_SHARED:%.c=$(OUTPUTDIR)/%.o) $(CXXSOURCES_SHARED:%.cpp=$(OUTPUTDIR)/%.o)
  $(1)_OBJECTS += $$($(1)_CSOURCES:%.c=$(OUTPUTDIR)/%.o) $$($(1)_CXXSOURCES:%.cpp=$(OUTPUTDIR)/%.o)
endef

$(foreach entry,$(EXAMPLES),$(eval $(call make-example,$(entry))))

.SECONDEXPANSION:
$(EXAMPLE_FILES): $(LIBRARY_FILE) $$($$(notdir $$@)_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

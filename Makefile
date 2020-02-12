.PHONY: all clean help

VERSION  := 0.0.1
AUTHORS  := sed

# ------------------------------------
# Address and leak sanitizers can be
# combined.
# ------------------------------------
SANITIZE_ADDRESS ?= NO
SANITIZE_LEAK    ?= NO

# ------------------------------------
# For the sanitizers to work, we need
# to enable debugging
# ------------------------------------
DEBUG ?= NO
ifeq "$(SANITIZE_ADDRESS)" "YES"
  DEBUG := YES
endif
ifeq "$(SANITIZE_LEAK)" "YES"
  DEBUG := YES
endif

# ------------------------------------
# The sources and includes are in...
# ------------------------------------
PROJECT_DIR := $(shell pwd -P)
DEPDIR      := dep
OBJDIR      := obj
INCLUDE     :=

# -----------------------------------------------------------------------------
# Tools to use
# -----------------------------------------------------------------------------
CC := $(shell which cc)
LD := gcc
MKDIR ?= $(shell which mkdir) -p
RM := $(shell which rm) -f
SED   ?= $(shell which sed)

# ------------------------------------
# Target name
# ------------------------------------
TARGET := $(shell basename $(PROJECT_DIR))

# Append '_d' on debug builds
ifeq (YES,$(DEBUG))
  TARGET := ${TARGET}_d
endif

# -----------------------------------------------------------------------------
# Store already set flags and append later
# -----------------------------------------------------------------------------
caller_CPPFLAGS := $(CPPFLAGS)
caller_CFLAGS   := $(CFLAGS)
caller_LDFLAGS  := $(LDFLAGS)
CPPFLAGS :=
CFLAGS   :=
LDFLAGS  :=

# -----------------------------------------------------------------------------
# Default flags
# -----------------------------------------------------------------------------
GCC_STACKPROT := -fstack-protector-strong
GCC_CSTD      := c11
COMMON_FLAGS  := -Wall -Wextra -Wpedantic -Wno-format

# -----------------------------------------------------------------------------
# Determine proper pedantic switch, and if and how stack protector works
# -----------------------------------------------------------------------------
GCCVERSGTEQ82 := 0

ifeq (,$(findstring clang,$(CC)))
  GCCVERSGTEQ82 := $(shell expr `$(CC) -dumpversion | cut -f1,2 -d. | tr -d '.'` \>= 82)
endif

ifeq "$(GCCVERSGTEQ82)" "1"
  GCC_CSTD    := c18
else
  GCC_CSTD    := c11
endif

# -----------------------------------------------------------------------------
# Debug Mode settings
# -----------------------------------------------------------------------------
HAS_DEBUG_FLAG := NO
OBJ_SUB_DIR    := release
ifneq (,$(findstring -g,$(CFLAGS)))
  ifneq (,$(findstring -ggdb,$(CFLAGS)))
    HAS_DEBUG_FLAG := YES
  endif
  DEBUG := YES
endif

ifeq (YES,$(DEBUG))
  ifeq (NO,$(HAS_DEBUG_FLAG))
    COMMON_FLAGS := -ggdb ${COMMON_FLAGS} -O0
  endif

  CPPFLAGS       := ${CPPFLAGS} -DPWX_DEBUG
  COMMON_FLAGS   := -march=core2 -mtune=generic ${COMMON_FLAGS} ${GCC_STACKPROT}
  HAVE_SANITIZER := NO

  # address sanitizer activition
  ifeq (YES,$(SANITIZE_ADDRESS))
    COMMON_FLAGS   := ${COMMON_FLAGS} -fsanitize=address
    LDFLAGS        := ${LDFLAGS} -fsanitize=address
    HAVE_SANITIZER := YES
  endif

  # Leak detector activation
  ifeq (YES,$(SANITIZE_LEAK))
    COMMON_FLAGS   := ${COMMON_FLAGS} -fsanitize=leak
    LDFLAGS        := ${LDFLAGS} -fsanitize=leak
    HAVE_SANITIZER := YES
  endif

  ifeq (NO,$(HAVE_SANITIZER))
    # If no other sanitizer was set, we check for the thread sanitizer
    ifeq (YES,$(SANITIZE_THREAD))
      COMMON_FLAGS   := ${COMMON_FLAGS} -fsanitize=thread
      LDFLAGS        := ${LDFLAGS} -fsanitize=thread
    endif
  endif

  # Put objects in debug folder
  OBJ_SUB_DIR := debug
else
  COMMON_FLAGS := -march=native ${COMMON_FLAGS} -O2 -Wno-unused-parameter
endif
OBJDIR := ${OBJDIR}/${OBJ_SUB_DIR}
DEPDIR := ${DEPDIR}/${OBJ_SUB_DIR}

# -----------------------------------------------------------------------------
# Source files and directories
# -----------------------------------------------------------------------------
SOURCES  := $(wildcard $(PROJECT_DIR)/src/*.c)
DEPENDS  := $(addprefix $(DEPDIR)/,$(subst $(PROJECT_DIR)/src/, ,$(SOURCES:.c=.d)))
MODULES  := $(addprefix $(OBJDIR)/,$(subst $(PROJECT_DIR)/src/, ,$(SOURCES:.c=.o)))


# -----------------------------------------------------------------------------
# Flags for compiler and linker
# -----------------------------------------------------------------------------
DEFINES  := -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DNO_GLOG -D_GNU_SOURCE
CPPFLAGS := -fPIC ${CPPFLAGS} $(DEFINES) $(INCLUDE)
CFLAGS   := $(COMMON_FLAGS) -std=$(GCC_CSTD) -pthread
LDFLAGS  := -fPIE ${LDFLAGS} -lpthread


# -----------------------------------------------------------------------------
# Eventually event caller flags for overrides
# -----------------------------------------------------------------------------
CPPFLAGS := ${CPPFLAGS} $(caller_CPPFLAGS)
CFLAGS   := ${CFLAGS} $(caller_CFLAGS)
LDFLAGS  := ${LDFLAGS} $(caller_LDFLAGS)

# ------------------------------------
# Default target
# ------------------------------------
all: $(TARGET)

# ------------------------------------
# Compile C modules
# ------------------------------------
$(MODULES): $(OBJDIR)/%.o: src/%.c Makefile
	@echo "[*] Compiling $@" ; \
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

# ------------------------------------
# help target to print settings
# ------------------------------------
help:
	@echo "$(TARGET) Version $(VERSION) settings:"
	@echo "  PROJECT_DIR: $(PROJECT_DIR)"
	@echo "  CC         : $(CC)"
	@echo "  LD         : $(LD)"
	@echo "  RM         : $(RM)"
	@echo "  SED        : $(SED)"
	@echo "  CPPFLAGS   : $(CPPFLAGS)"
	@echo "  CFLAGS     : $(CFLAGS)"
	@echo "  LDFLAGS    : $(LDFLAGS)"
	@echo "  DEPENDS    : $(DEPENDS)"
	@echo "  MODULES    : $(MODULES)"

# ------------------------------------
# Regular targets
# ------------------------------------
$(TARGET): $(MODULES)
	@echo "[*] Linking $@"
	$(LD) -o $@ $(MODULES) $(LDFLAGS)

clean:
	@echo "[*] Performing clean..."
	$(RM) $(MODULES) $(TARGET)

# ------------------------------------
# Create dependencies
# ------------------------------------
$(DEPDIR)/%.d: src/%.c
	@set -e; $(RM) $@; \
	$(MKDIR) $(dir $@); \
	$(CC) -MM $(CPPFLAGS) $(DEFINES) $(CFLAGS) $< > $@.$$$$; \
	$(SED) -e 's,\($(notdir $*)\)\.o[ :]*,$(OBJDIR)/$(dir $*)\1.o $@ : ,g' \
	       -e 's,$(PROJECT_DIR)/,,g' < $@.$$$$ > $@; \
	$(RM) $@.$$$$

# ------------------------------------
# Include all dependency files
# ------------------------------------
ifeq (,$(findstring clean,$(MAKECMDGOALS)))
  -include $(DEPENDS)
endif

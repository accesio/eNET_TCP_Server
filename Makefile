# ---------------------- Base, Debug & Release Flags ----------------------
MAKEFLAGS += --output-sync=target
NPROC := $(shell nproc 2>/dev/null || echo 1)
ifeq (,$(filter -j%,$(MAKEFLAGS)))
  MAKEFLAGS += -j$(NPROC)
endif

BASE_CFLAGS     = -MMD -MP

# ---- Detect sanitizer support in the current CXX ----
# GCC prints the literal name if not found; only treat as "present" when we get a real path.
SAN_SPEC_RAW := $(shell $(CXX) -print-file-name=libsanitizer.spec)
# If libsanitizer.spec is found, GCC returns an absolute path (not the bare name)
ifeq ($(SAN_SPEC_RAW),libsanitizer.spec)
  SAN_PRESENT :=
  $(info [Sanitize] libsanitizer.spec not found → disabling SAN_FLAGS)
else
  # Double-check the path actually exists
  SAN_PRESENT := $(wildcard $(SAN_SPEC_RAW))
  $(info [Sanitize] libsanitizer.spec found at $(SAN_SPEC_RAW))
endif

# Detect static libasan presence (for -static-libasan)
LIBASAN_A_RAW := $(shell $(CXX) -print-file-name=libasan.a)
ifeq ($(LIBASAN_A_RAW),libasan.a)
  LIBASAN_A :=
  $(info [Sanitize] libasan.a not found)
else
  LIBASAN_A := $(wildcard $(LIBASAN_A_RAW))
  $(info [Sanitize] libasan.a candidate: $(LIBASAN_A_RAW))
endif

# Allow manual override: SANITIZE=0 to force-off; default: 1
SANITIZE ?= 1

# Enable sanitizers only if requested AND toolchain provides them
ifeq ($(SANITIZE),1)
  ifneq ($(SAN_PRESENT),)
    SAN_FLAGS := -fsanitize=address,undefined,leak -fno-omit-frame-pointer
  else
    SAN_FLAGS :=
  endif
else
  SAN_FLAGS :=
endif

CXXFLAGS_DEBUG  = $(SAN_FLAGS) -gdwarf-4 -g2 \
                  -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused \
                  -Wshadow -Woverloaded-virtual -Wpedantic -Wconversion \
                  -Wnull-dereference -Wduplicated-cond -Wduplicated-branches \
                  -Wlogical-op -Wuseless-cast -Wfatal-errors -Wno-unknown-pragmas\
                  -Wno-unused-parameter -std=gnu++2a -O1

CXXFLAGS_RELEASE = -O2 -Wfatal-errors -std=gnu++2a

CXXFLAGS_DEBUG   += $(BASE_CFLAGS)
CXXFLAGS_RELEASE += $(BASE_CFLAGS)

LDFLAGS_DEBUG    = $(SAN_FLAGS)
ifneq ($(LIBASAN_A),)
  LDFLAGS_DEBUG += -static-libasan
endif
LDFLAGS_RELEASE  =

LDLIBS           = -lm -lpthread -latomic -ldl -lfmt

# By default, we will do "release"
CXXFLAGS         = $(CXXFLAGS_RELEASE)
LDFLAGS          = $(LDFLAGS_RELEASE)

# ---------------------- Directories / Files -----------------------
SRCDIR    = .
OBJDIR    = obj

SRCS      := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/DataItems/*.cpp)
HEADERS   := $(wildcard $(SRCDIR)/*.h)   $(wildcard $(SRCDIR)/DataItems/*.h)

SPECIFIC_EXCLUDED_TEST      = $(SRCDIR)/test.cpp
SPECIFIC_EXCLUDED_AIOENETD  = $(SRCDIR)/aioenetd.cpp

SRCS_FOR_AIOENETD   = $(filter-out $(SPECIFIC_EXCLUDED_TEST), $(SRCS))
SRCS_FOR_TEST       = $(filter-out $(SPECIFIC_EXCLUDED_AIOENETD), $(SRCS))

OBJS_FOR_AIOENETD   = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS_FOR_AIOENETD))
OBJS_FOR_TEST       = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS_FOR_TEST))

# ---------------------- Deploy settings ----------------------
# Use bash for string splitting in recipes
SHELL := /bin/bash
DEPLOY_PAIRS ?= \
  pb@strix.local::/home/pb \
  acces@acces-enet.local::/home/acces

# Subdirectory (under each pair's DIR) to mirror sources into
REMOTE_SUBDIR ?= eNET_TCP_Server

DEPLOY_AFTER_BUILD ?= 1                  # 1=auto-deploy after successful build, 0=off
DEPLOY_SOURCES     ?= 0                  # 1=also mirror sources (cpp/h/Makefile/DataItems)
RSYNC_FLAGS        ?= -azP               # tweak to taste
SSH                ?= ssh
RSYNC              ?= rsync

# ---------------------- ANSI Color Variables ----------------------
GREEN  = \033[0;32m
CYAN   = \033[0;36m
RESET  = \033[0m

# ---------------------- Pretty printing knobs ----------------------
WIDTH ?= 16
Q := @

WRAP_COL ?= 100
FMT_WRAP = | fmt -w $(WRAP_COL) | sed '2,$$s/^/  /'

# Silence by default; VERBOSE=1 shows raw commands
ifeq ($(VERBOSE),1)
  PRINT_COMPILE = @printf "$(CYAN)Compiling %23s$(RESET) " "$<"; \
                  (echo "  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@") $(FMT_WRAP)
else
  PRINT_COMPILE = @printf "$(CYAN)Compiling %23s$(RESET) " "$<"
endif

LINK_BANNER_QUIET = @printf "$(GREEN)Linking %-$(WIDTH)s($(words $^) objs)$(RESET)\n" "$@"

LINK_BANNER_VERBOSE = @printf "$(GREEN)Linking %-$(WIDTH)s$(RESET)\n" "$@"; \
  ( printf "  %s " "$(CXX) $(LDFLAGS) -o $@"; \
    printf "%s " $(notdir $^); \
    printf "%s\n" "$(LDLIBS)" ) \
  | fmt -w $(WRAP_COL) | sed '2,$$s/^/  /'

ifeq ($(VERBOSE),1)
  LINK_BANNER = $(LINK_BANNER_VERBOSE)
else
  LINK_BANNER = $(LINK_BANNER_QUIET)
endif

# ---------------------- Default Target ----------------------
all: release

LINK_CMD = $(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# ---------------------- Build Targets ----------------------

test: $(OBJS_FOR_TEST)
	$(LINK_BANNER)
	$(Q)$(LINK_CMD)

aioenetd: $(OBJS_FOR_AIOENETD)
	$(LINK_BANNER)
	$(Q)$(LINK_CMD)
ifneq ($(DEPLOY_AFTER_BUILD),0)
	$(MAKE) --no-print-directory deploy
ifeq ($(DEPLOY_SOURCES),1)
	$(MAKE) --no-print-directory deploy-src
endif
endif


# Debug build
debug: CXXFLAGS = $(CXXFLAGS_DEBUG)
debug: LDFLAGS = $(LDFLAGS_DEBUG)
debug: aioenetd

# Release build
release: CXXFLAGS = $(CXXFLAGS_RELEASE)
release: LDFLAGS = $(LDFLAGS_RELEASE)
release: aioenetd

# Pattern rule for creating .o files from .cpp
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(PRINT_COMPILE)
	$(Q)$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Ensure obj/ and obj/DataItems exist
$(OBJDIR):
	@mkdir -p $@
	@mkdir -p $@/DataItems

clean:
	@printf "$(GREEN)cleaning...$(RESET)\n"
	@rm -rf $(OBJDIR) aioenetd test
	@printf "$(GREEN)done.$(RESET)\n"


.PHONY: deploy deploy-src

# Deploy binary: try each pair until one works
deploy: aioenetd
	@{	set -e; ok=0; \
		for pair in $(DEPLOY_PAIRS); do \
			host="$${pair%%::*}"; dir="$${pair##*::}"; \
			echo "Deploying $< -> $$host:$$dir"; \
			if $(SSH) "$$host" 'mkdir -p '"$$dir" && \
			$(RSYNC) $(RSYNC_FLAGS) "$<" "$$host:$$dir/"; then \
			echo "Deploy OK to $$host:$$dir"; ok=1; break; \
			else \
			echo "Deploy failed to $$host:$$dir — trying next..."; \
			fi; \
		done; \
		test $$ok -eq 1 || { echo "ERROR: deploy failed to all targets" >&2; exit 1; }; \
	}

# Mirror sources: same fallback logic; target dir = DIR/REMOTE_SUBDIR
deploy-src:
	@{	set -e; ok=0; \
		for pair in $(DEPLOY_PAIRS); do \
			host="$${pair%%::*}"; dir="$${pair##*::}"; srcdir="$$dir/$(REMOTE_SUBDIR)"; \
			echo "Mirroring sources -> $$host:$$srcdir"; \
			if $(SSH) "$$host" 'mkdir -p '"$$srcdir" && \
			$(RSYNC) $(RSYNC_FLAGS) \
				--include='*/' \
				--include='*.cpp' --include='*.c' --include='*.hpp' --include='*.h' \
				--include='Makefile' \
				--include='DataItems/***' \
				--exclude='*' \
				./ "$$host:$$srcdir/"; then \
			echo "Sources OK to $$host:$$srcdir"; ok=1; break; \
			else \
			echo "Source mirror failed to $$host:$$srcdir — trying next..."; \
			fi; \
		done; \
		test $$ok -eq 1 || { echo "ERROR: source mirror failed to all targets" >&2; exit 1; }; \
	}

# Automatic dependencies for each compiled .cpp
-include $(OBJDIR)/*.d
-include $(OBJDIR)/DataItems/*.d

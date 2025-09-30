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
  # $(info [Sanitize] libsanitizer.spec not found -> disabling SAN_FLAGS)
else
  # Double-check the path actually exists
  SAN_PRESENT := $(wildcard $(SAN_SPEC_RAW))
  # $(info [Sanitize] libsanitizer.spec found at $(SAN_SPEC_RAW))
endif

# Detect static libasan presence (for -static-libasan)
LIBASAN_A_RAW := $(shell $(CXX) -print-file-name=libasan.a)
ifeq ($(LIBASAN_A_RAW),libasan.a)
  LIBASAN_A :=
  # $(info [Sanitize] libasan.a not found)
else
  LIBASAN_A := $(wildcard $(LIBASAN_A_RAW))
  # $(info [Sanitize] libasan.a candidate: $(LIBASAN_A_RAW))
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
  root@acces-enet.local::/home/acces \
  root@acces-enet-2.local::/home/acces \
  root@strix.local::/home/pb

# Subdirectory (under each pair's DIR) to mirror sources into
REMOTE_SUBDIR ?= eNET_TCP_Server
REMOTE_SERVICE     ?=#aioenetd
REMOTE_SERVICE_MODE  ?= auto       # auto | system | user
REMOTE_NEEDS_SUDO  ?= 1            # 1 = prefix systemctl/mv/chmod with sudo, 0 = no sudo
REMOTE_TMP         ?= /tmp/aioenetd.new
RESTART_WITH_SUDO  ?= 1

DEPLOY_AFTER_BUILD ?= 0
DEPLOY_SOURCES     ?= 0
RSYNC_FLAGS        ?= -azP
SSH                ?= ssh
RSYNC              ?= rsync
SCP                ?= scp
SCP_FLAGS          ?= -O -p

# ---------------------- ANSI Color Variables ----------------------
GREEN  = \033[0;32m
YELLOW = \033[0;33m
RED    = \033[0;31m
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
  PRINT_COMPILE = @printf "$(CYAN)Compiling %23s$(RESET) \n" "$<"
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

# Resolve sudo prefix once per host
define _sudo_prefix
$$( $(SSH) "$1" 'sudo -n true >/dev/null 2>&1' && [ "$(REMOTE_NEEDS_SUDO)" = "1" ] && echo sudo || echo )
endef


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


.PHONY: deploy deploy-src help vars

# Deploy binary: try each pair until one works
deploy: aioenetd
	@{ ok=0; \
	for pair in $(DEPLOY_PAIRS); do \
		host="$${pair%%::*}"; dir="$${pair##*::}"; \
		printf "$(CYAN)-> Deploying %s -> %s:%s$(RESET)\n" "$<" "$$host" "$$dir"; \
		if ! $(SSH) "$$host" 'mkdir -p '"$$dir" 2>/dev/null; then \
		printf "$(YELLOW)   host unreachable or mkdir failed: %s — trying next...$(RESET)\n" "$$host"; \
		continue; \
		fi; \
		SVC="$(strip $(REMOTE_SERVICE))"; \
		if [ -n "$$SVC" ] && [ "$(RESTART_WITH_SUDO)" = "1" ]; then \
		if $(SSH) "$$host" 'sudo -n true >/dev/null 2>&1'; then \
			printf "   stopping service: %s (sudo systemctl)\n" "$$SVC"; \
			$(SSH) "$$host" "sudo systemctl stop '$$SVC' 2>/dev/null || true"; \
			$(SSH) "$$host" "for i in 1 2 3 4 5; do sudo systemctl is-active --quiet '$$SVC' || exit 0; sleep 1; done; exit 0"; \
		else \
			printf "$(YELLOW)   sudo -n unavailable; skipping service stop$(RESET)\n"; \
		fi; \
		fi; \
		if $(SSH) "$$host" 'command -v rsync >/dev/null 2>&1'; then \
		printf "   using rsync -> stage %s\n" "$(REMOTE_TMP)"; \
		if ! $(RSYNC) $(RSYNC_FLAGS) "$<" "$$host:'$(REMOTE_TMP)'"; then \
			printf "$(YELLOW)   rsync failed — trying next host...$(RESET)\n"; \
			continue; \
		fi; \
		else \
		printf "$(YELLOW)   rsync not found; using scp -> stage %s$(RESET)\n" "$(REMOTE_TMP)"; \
		if ! $(SCP) $(SCP_FLAGS) "$<" "$$host:'$(REMOTE_TMP)'"; then \
			printf "$(YELLOW)   scp failed — trying next host...$(RESET)\n"; \
			continue; \
		fi; \
		fi; \
		if ! $(SSH) "$$host" "mv -f '$(REMOTE_TMP)' '$$dir/aioenetd' && chmod +x '$$dir/aioenetd' 2>/dev/null || { sudo mv -f '$(REMOTE_TMP)' '$$dir/aioenetd' && sudo chmod +x '$$dir/aioenetd'; }"; then \
		printf "$(YELLOW)   move/chmod failed — trying next host...$(RESET)\n"; \
		continue; \
		fi; \
		if [ -n "$$SVC" ] && [ "$(RESTART_WITH_SUDO)" = "1" ]; then \
			if $(SSH) "$$host" 'sudo -n true >/dev/null 2>&1'; then \
				printf "   starting service: %s (sudo systemctl)\n" "$$SVC"; \
				if ! $(SSH) "$$host" "sudo systemctl start '$$SVC' 2>/dev/null"; then \
				printf "$(YELLOW)   sudo systemctl start failed — verify manually$(RESET)\n"; \
				elif ! $(SSH) "$$host" "sudo systemctl is-active --quiet '$$SVC'"; then \
				printf "$(YELLOW)   service not active after start — verify manually$(RESET)\n"; \
				fi; \
			else \
				printf "$(YELLOW)   sudo -n unavailable; skipping service start$(RESET)\n"; \
			fi; \
		fi; \
		printf "$(GREEN)Deploy complete on %s:%s$(RESET)\n" "$$host" "$$dir"; \
		ok=1; break; \
	done; \
	test $$ok -eq 1 || { printf "$(RED)ERROR: deploy failed to all targets$(RESET)\n" >&2; exit 1; }; \
	}

deploy-src:
	@{ ok=0; \
	for pair in $(DEPLOY_PAIRS); do \
		host="$${pair%%::*}"; dir="$${pair##*::}"; srcdir="$$dir/$(REMOTE_SUBDIR)"; \
		printf "$(CYAN)-> Mirroring sources -> %s:%s$(RESET)\n" "$$host" "$$srcdir"; \
		if ! $(SSH) "$$host" 'mkdir -p '"$$srcdir" 2>/dev/null; then \
		printf "$(YELLOW)   host unreachable or mkdir failed: %s — trying next...$(RESET)\n" "$$host"; \
		continue; \
		fi; \
		if $(SSH) "$$host" 'command -v rsync >/dev/null 2>&1'; then \
		printf "   using rsync\n"; \
		if $(RSYNC) $(RSYNC_FLAGS) \
				--include='*/' \
				--include='*.cpp' --include='*.c' --include='*.hpp' --include='*.h' \
				--include='Makefile' \
				--include='DataItems/***' \
				--exclude='*' \
				./ "$$host:$$srcdir/"; then \
			printf "$(GREEN)Sources OK to %s:%s$(RESET)\n" "$$host" "$$srcdir"; ok=1; break; \
		else \
			printf "$(YELLOW)   rsync failed to %s:%s — trying next...$(RESET)\n" "$$host" "$$srcdir"; \
		fi; \
		else \
		printf "$(YELLOW)   rsync not found; using tar over ssh$(RESET)\n"; \
		if tar czf - \
				--exclude='.*.swp' \
				--exclude-vcs \
				--include='./*.cpp' --include='./*.c' --include='./*.hpp' --include='./*.h' \
				--include='./Makefile' \
				--include='./DataItems' --include='./DataItems/**' \
				--exclude='*' . \
			| $(SSH) "$$host" "tar xzf - -C '$$srcdir'"; then \
			printf "$(GREEN)Sources OK to %s:%s (tar)$(RESET)\n" "$$host" "$$srcdir"; ok=1; break; \
		else \
			printf "$(YELLOW)   tar-over-ssh failed to %s:%s — trying next...$(RESET)\n" "$$host" "$$srcdir"; \
		fi; \
		fi; \
	done; \
	test $$ok -eq 1 || { printf "$(RED)ERROR: source mirror failed to all targets$(RESET)\n" >&2; exit 1; }; \
	}

# ---------------------- Automatic dependencies for each compiled .cpp
-include $(OBJDIR)/*.d
-include $(OBJDIR)/DataItems/*.d

.PHONY: help vars

help:
	@printf "$(CYAN)ACCES eNET TCP Server — Make help$(RESET)\n"
	@printf "$(GREEN)\nTargets$(RESET)\n"
	@printf "  $(CYAN)release$(RESET)        Build release (default)\n"
	@printf "  $(CYAN)debug$(RESET)          Build debug (with sanitizers when available)\n"
	@printf "  $(CYAN)aioenetd$(RESET)       Link only (no config change)\n"
	@printf "  $(CYAN)test$(RESET)           Build the test binary\n"
	@printf "  $(CYAN)deploy$(RESET)         Service-aware deploy: stop -> stage -> swap -> start\n"
	@printf "  $(CYAN)deploy-src$(RESET)     Mirror sources (rsync or tar over ssh)\n"
	@printf "  $(CYAN)deployrsync$(RESET)    Deploy via rsync only (no fallback)\n"
	@printf "  $(CYAN)deploy-srcrsync$(RESET)Mirror sources via rsync only (no fallback)\n"
	@printf "  $(CYAN)clean$(RESET)          Remove obj/ and built binaries\n"
	@printf "$(GREEN)\nExamples$(RESET)\n"
	@printf "  make release\n"
	@printf "  make debug VERBOSE=1\n"
	@printf "  make deploy DEPLOY_AFTER_BUILD=1 REMOTE_SERVICE=aioenetd\n"
	@printf "  make deploy-src REMOTE_SUBDIR=eNET_TCP_Server\n"
	@printf "  make vars to see modifiable make variables\n"


vars:
	@printf "$(GREEN)\nVariables (set with VAR=value)$(RESET)\n"
	@printf "  CXX=%s\n" "$(CXX)"
	@printf "  SANITIZE=%s\n" "$(SANITIZE)"
	@printf "  SAN_FLAGS=%s\n" "$(SAN_FLAGS)"
	@printf "  CXXFLAGS=%s\n" "$(CXXFLAGS)"
	@printf "  LDFLAGS=%s\n" "$(LDFLAGS)"
	@printf "  LDLIBS=%s\n" "$(LDLIBS)"
	@printf "  WRAP_COL=%s  VERBOSE=%s\n" "$(WRAP_COL)" "$(VERBOSE)"
	@printf "  SRCDIR=%s  OBJDIR=%s\n" "$(SRCDIR)" "$(OBJDIR)"
	@printf "  $(GREEN)Deploy knobs$(RESET)\n"
	@printf "  DEPLOY_AFTER_BUILD=%s  DEPLOY_SOURCES=%s\n" "$(DEPLOY_AFTER_BUILD)" "$(DEPLOY_SOURCES)"
	@printf "  REMOTE_SERVICE=%s  REMOTE_NEEDS_SUDO=%s\n" "$(strip $(REMOTE_SERVICE))" "$(REMOTE_NEEDS_SUDO)"
	@printf "  REMOTE_TMP=%s  REMOTE_SUBDIR=%s\n" "$(REMOTE_TMP)" "$(REMOTE_SUBDIR)"
	@printf "  SSH=%s  RSYNC=%s  RSYNC_FLAGS=%s\n" "$(SSH)" "$(RSYNC)" "$(RSYNC_FLAGS)"
	@printf "  SCP=%s  SCP_FLAGS=%s\n" "$(SCP)" "$(SCP_FLAGS)"
	@printf "  DEPLOY_PAIRS:\n"; \
	for p in $(DEPLOY_PAIRS); do printf "    - %s\n" "$$p"; done

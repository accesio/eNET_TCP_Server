# ---------------------- Base, Debug & Release Flags ----------------------
MAKEFLAGS += --output-sync=target

BASE_CFLAGS     = -MMD -MP

SAN_FLAGS = -fsanitize=address,undefined,leak -fno-omit-frame-pointer

CXXFLAGS_DEBUG  = $(SAN_FLAGS) -gdwarf-4 -g2 \
                  -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused \
                  -Wshadow -Woverloaded-virtual -Wpedantic -Wconversion \
                  -Wnull-dereference -Wduplicated-cond -Wduplicated-branches \
                  -Wlogical-op -Wuseless-cast -Wfatal-errors -Wno-unknown-pragmas\
                  -Wno-unused-parameter -std=gnu++2a -O1

CXXFLAGS_RELEASE = -O2 -Wfatal-errors -std=gnu++2a

CXXFLAGS_DEBUG   += $(BASE_CFLAGS)
CXXFLAGS_RELEASE += $(BASE_CFLAGS)

LDFLAGS_DEBUG    = $(SAN_FLAGS) -static-libasan
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

# ---------------------- ANSI Color Variables ----------------------
GREEN  = \033[0;32m
CYAN   = \033[0;36m
RESET  = \033[0m

# -------- Pretty printing knobs ----------
WIDTH ?= 16
Q := @

WRAP_COL ?= 120
FMT_WRAP = | fmt -w $(WRAP_COL) | sed '2,$$s/^/  /'

# Silence by default; VERBOSE=1 shows raw commands
ifeq ($(VERBOSE),1)
  PRINT_COMPILE = @printf "$(CYAN)Compiling %23s$(RESET)\n" "$<"; \
                  (echo "  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@") $(FMT_WRAP)
else
  PRINT_COMPILE = @printf "$(CYAN)Compiling %23s$(RESET)\n" "$<"
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

# ---------------------- Build Targets -----------------------

test: $(OBJS_FOR_TEST)
	$(LINK_BANNER)
	$(Q)$(LINK_CMD)

aioenetd: $(OBJS_FOR_AIOENETD)
	$(LINK_BANNER)
	$(Q)$(LINK_CMD)

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


# Automatic dependencies for each compiled .cpp
-include $(OBJDIR)/*.d
-include $(OBJDIR)/DataItems/*.d

# ---------------------- Base, Debug & Release Flags ----------------------
# Compile-only base flags
BASE_CFLAGS     = -MMD -MP

CXXFLAGS_DEBUG  = -fsanitize=address,undefined,leak -fno-omit-frame-pointer -g \
                  -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused \
                  -Wshadow -Woverloaded-virtual -Wpedantic -Wconversion \
                  -Wnull-dereference -Wduplicated-cond -Wduplicated-branches \
                  -Wlogical-op -Wuseless-cast -Wfatal-errors -Wno-unknown-pragmas\
                  -Wno-unused-parameter -std=gnu++2a

CXXFLAGS_RELEASE = -O2 -Wfatal-errors -std=gnu++2a

# Append compile-only base flags
CXXFLAGS_DEBUG   += $(BASE_CFLAGS)
CXXFLAGS_RELEASE += $(BASE_CFLAGS)

# By default, we will do "release"
CXXFLAGS         = $(CXXFLAGS_RELEASE)

# Link flags are separate (keeps link line clean)
LDFLAGS          =
LDLIBS           = -lm -lpthread -latomic -ldl -lfmt

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
QLINK := @

# Silence by default; VERBOSE=1 shows raw commands
ifeq ($(VERBOSE),1)
  Q :=
  PRINT_COMPILE = @printf "$(CYAN)Compiling %23s$(RESET) : " "$<"
else
  Q := @
  PRINT_COMPILE = @printf "$(CYAN)Compiling %23s$(RESET)\n" "$<"
endif

LINK_BANNER_QUIET = @printf "$(GREEN)Linking %-$(WIDTH)s($(words $^) objs)$(RESET)\n" "$@"

WRAP_COL ?= 120

LINK_BANNER_VERBOSE = @printf "$(GREEN)Linking %-$(WIDTH_LINK)s$(RESET)\n" "$@"; \
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
	$(QLINK)$(LINK_CMD)

aioenetd: $(OBJS_FOR_AIOENETD)
	$(LINK_BANNER)
	$(QLINK)$(LINK_CMD)

# Debug build => override CXXFLAGS with CXXFLAGS_DEBUG
debug: CXXFLAGS = $(CXXFLAGS_DEBUG)
debug: aioenetd

# Release build => override CXXFLAGS with CXXFLAGS_RELEASE
release: CXXFLAGS = $(CXXFLAGS_RELEASE)
release: aioenetd

# Pattern rule for creating .o files from .cpp
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(PRINT_COMPILE)
	$(Q)$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Ensure obj/ and obj/DataItems exist
$(OBJDIR):
	mkdir -p $@
	mkdir -p $@/DataItems

clean:
	rm -rf $(OBJDIR) aioenetd test

# Automatic dependencies for each compiled .cpp
-include $(OBJDIR)/*.d
-include $(OBJDIR)/DataItems/*.d

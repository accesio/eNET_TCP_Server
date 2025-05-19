# ---------------------- ANSI Color Variables ----------------------
GREEN  = \033[0;32m
CYAN   = \033[0;36m
RESET  = \033[0m

# ---------------------- Base, Debug & Release Flags ----------------------
BASEFLAGS       = -MMD -MP     # Always included in both debug & release

CXXFLAGS_DEBUG  = -fsanitize=address,undefined,leak -fno-omit-frame-pointer -g \
                  -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused \
                  -Wshadow -Woverloaded-virtual -Wpedantic -Wconversion \
                  -Wnull-dereference -Wduplicated-cond -Wduplicated-branches \
                  -Wlogical-op -Wuseless-cast -Wfatal-errors \
                  -Wno-unused-parameter -std=gnu++2a

CXXFLAGS_RELEASE = -O2 -Wfatal-errors -std=gnu++2a

# Append the base flags to each
CXXFLAGS_DEBUG   += $(BASEFLAGS)
CXXFLAGS_RELEASE += $(BASEFLAGS)

# By default, we will do "release"
CXXFLAGS         = $(CXXFLAGS_RELEASE)

# ---------------------- Libraries ----------------------
LDLIBS    = -lm -lpthread -latomic -ldl -lfmt
LDLIBS    = -lm -lpthread -latomic -ldl -lfmt
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

# ---------------------- Default Target ----------------------
all: release

# ---------------------- Build Targets -----------------------

# Creates "test" using the current CXXFLAGS
test: $(OBJS_FOR_TEST)
	@echo -n "$(GREEN)Linking test...$(RESET)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# Creates "aioenetd" using the current CXXFLAGS
aioenetd: $(OBJS_FOR_AIOENETD)
	@echo -n "$(GREEN)Linking aioenetd...$(RESET)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# Debug build => override CXXFLAGS with CXXFLAGS_DEBUG
debug: CXXFLAGS = $(CXXFLAGS_DEBUG)
debug: aioenetd

# Release build => override CXXFLAGS with CXXFLAGS_RELEASE
release: CXXFLAGS = $(CXXFLAGS_RELEASE)
release: aioenetd

# Pattern rule for creating .o files from .cpp
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@printf "$(CYAN)Compiling %23s$(RESET) : " "$<"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Ensure obj/ and obj/DataItems exist
$(OBJDIR):
	mkdir -p $@
	mkdir -p $@/DataItems

clean:
	rm -rf $(OBJDIR)

# Automatic dependencies for each compiled .cpp
-include $(OBJDIR)/*.d
-include $(OBJDIR)/DataItems/*.d

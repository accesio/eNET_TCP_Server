# ANSI color codes
BLACK=\033[0;30m
RED=\033[0;31m
GREEN=\033[0;32m
YELLOW=\033[0;33m
BLUE=\033[0;34m
MAGENTA=\033[0;35m
CYAN=\033[0;36m
WHITE=\033[0;37m
RESET_COLOR=\033[0m


# Set CXX variable for AARCH64 architecture
ifneq ($(shell uname -m),aarch64)
    CXX ?= aarch64-linux-gnu-g++
else
    CXX ?= g++
endif

# Set CXXFLAGS and LDLIBS
# CXXFLAGS_DEEBUG = -fsanitize=address,undefined,leak -fno-omit-frame-pointer -g -Wall -Wshadow -Wnon-virtual-dtor -Wcast-align -Wunused -Wshadow -Woverloaded-virtual -Wpedantic -Wconversion -Wnull-dereference -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast -Wfatal-errors -Wno-unused-parameter -std=gnu++2a

CXXFLAGS_RELEASE = -Wfatal-errors -std=gnu++2a
# CXXFLAGS_RELEASE = -fsanitize=address,undefined,leak -g -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused -Wshadow -Woverloaded-virtual -Wpedantic -Wconversion -Wnull-dereference -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast -Wfatal-errors -Wno-unknown-pragmas -Wconversion -Wno-unused-parameter -std=gnu++2a
CXXFLAGS = $(CXXFLAGS_RELEASE)

LDLIBS = -lm -lpthread -latomic -ldl -lfmt

# Define object directory
SRCDIR = .
OBJDIR = obj

# Define source and object files
SRCS := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/DataItems/*.cpp)
HEADERS := $(wildcard $(SRCDIR)/*.h) $(wildcard $(SRCDIR)/DataItems/*.h)
SPECIFIC_EXCLUDED_TEST = $(SRCDIR)/test.cpp
SPECIFIC_EXCLUDED_AIOENETD = $(SRCDIR)/aioenetd.cpp
SRCS_FOR_AIOENETD = $(filter-out $(SPECIFIC_EXCLUDED_TEST), $(SRCS))
SRCS_FOR_TEST = $(filter-out $(SPECIFIC_EXCLUDED_AIOENETD), $(SRCS))

OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
OBJS_FOR_AIOENETD := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS_FOR_AIOENETD))
OBJS_FOR_TEST := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS_FOR_TEST))

# Build rules
all: aioenetd

test: $(OBJS_FOR_TEST)
	@echo -n "$(GREEN)Linking test...$(RESET_COLOR)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

aioenetd: $(OBJS_FOR_AIOENETD)
	@echo -n "$(GREEN)Linking aioenetd...$(RESET_COLOR)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS) Makefile | $(OBJDIR)
	@printf "$(CYAN)Compiling %23s$(RESET_COLOR) : " "$<"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $@ & mkdir -p $@/DataItems

clean:
	rm -rf obj

# Set CXX variable for AARCH64 architecture
ifneq ($(shell uname -m),aarch64)
    CXX = aarch64-linux-gnu-g++
else
    CXX = g++
endif

# Set CXXFLAGS and LDLIBS
CXXFLAGS = -g -Wfatal-errors -std=gnu++2a
LDLIBS = -lm -lpthread -latomic

# Define object directory
SRCDIR = .
OBJDIR = obj

# Define source and object files
SRCS := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/DataItems/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

# Build rules
all: aioenetd

aioenetd: $(OBJS) 
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $@ & mkdir -p $@/DataItems

clean:
	rm -rf $(OBJDIR)

.PHONY: all clean

CXX = clang++
CXXFLAGS = -Wall -Wextra -O2 -std=c++20
WAYLAND_FLAGS = $(shell pkg-config --cflags wayland-client)
WAYLAND_LIBS = $(shell pkg-config --libs wayland-client)

TARGET = lime

# Fix the wildcard pattern and object file names
SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.cc=.o)

INCLUDES = -I.

BUILD_DIR = build

EXECUTABLE = $(BUILD_DIR)/$(TARGET)

all: dirs $(EXECUTABLE)

dirs:
	mkdir -p $(BUILD_DIR)

# Fix the linking rule
$(EXECUTABLE): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(WAYLAND_LIBS)

# Add compilation rule for object files
%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(WAYLAND_FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f *.o

rebuild: clean all

install: all
	install -D -m 755 $(EXECUTABLE) /usr/bin/$(TARGET)

uninstall:
	rm -f /usr/bin/$(TARGET)

.PHONY: all dirs clean rebuild install uninstall

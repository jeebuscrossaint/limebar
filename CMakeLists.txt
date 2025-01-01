# CMakeLists.txt
cmake_minimum_required(VERSION 3.30.5)

# Project name and version
project(limebar VERSION 1.0 LANGUAGES CXX)

# Set CXX standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add include directory so lime.hh can be included as lime.hh
include_directories(include)

# Source files
set(SRC_FILES
	src/backend.cc
	src/bar.cc
	src/data.cc
	src/decor.cc
	src/flags.cc
	src/loop.cc
	src/main.cc
	src/text.cc
	src/token.cc
)

# Create the executable
add_executable(limebar ${SRC_FILES})

# Link libraries
find_package(Wayland REQUIRED)
target_link_libraries(limebar PRIVATE Wayland::Client)

# Include paths for the project
target_include_directories(limebar PRIVATE include)

# Install configuration
install(TARGETS limebar DESTINATION bin)
install(FILES docs/limebar.1 DESTINATION share/man/man1)
install(FILES LICENSE README.md CONTRIBUTING DESTINATION share/limebar)

# Enable warnings
if (CMAKE_CXX_COMPILER ID MATCHES "GNU|Clang")
	target_compile_options(limebar PRIVATE -Wall -Wextra -pedantic)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(limebar PRIVATE /W4)
endif

cmake_minimum_required(VERSION 3.20)
project(UNO___The_GAME CXX C)

set(CMAKE_CXX_STANDARD 17)

# Find the GLFW headers and library
find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h PATHS /opt/homebrew/Cellar/glfw/3.4/include)
find_library(GLFW_LIBRARY NAMES glfw PATHS /opt/homebrew/Cellar/glfw/3.4/lib)

if(NOT GLFW_INCLUDE_DIR OR NOT GLFW_LIBRARY)
    message(FATAL_ERROR "GLFW was not found. Please ensure it is installed.")
endif()

find_package(OpenGL REQUIRED)

# Create a static library from the glad.c file
add_library(glad STATIC src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Add the executable
add_executable(UNO___The_GAME src/main.cpp)

# Tell the compiler where to find header files
target_include_directories(UNO___The_GAME PRIVATE ${CMAKE_SOURCE_DIR}/include ${GLFW_INCLUDE_DIR})

# Link the libraries to the executable
target_link_libraries(UNO___The_GAME PRIVATE glad OpenGL::GL ${GLFW_LIBRARY})
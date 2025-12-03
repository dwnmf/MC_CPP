cmake_minimum_required(VERSION 3.17)
project(MC-CPP)

if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW) # ENABLE IPO
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
if(MSVC)
    add_compile_options(/O2 /DNDEBUG /permissive-)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
    add_compile_options(-O3 -march=native -mtune=native -DNDEBUG -flto=6)
    add_link_options(-flto=6)
endif()
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

# Включаем stb_image
include_directories(
    include
    include/glad/include
    include/glm
    include/miniaudio
    include/stb
    include/zlib
)

# Настройка GLFW из исходников
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(include/glfw)

# Создание библиотеки GLAD
add_library(glad STATIC include/glad/src/glad.c)
target_include_directories(glad PUBLIC include/glad/include)

# Создание библиотеки ZLib
# Исправление ошибки с lseek (ZLib)
if(UNIX)
    # Сообщаем ZLib, что у нас есть unistd.h (Linux/macOS)
    add_compile_definitions(Z_HAVE_UNISTD_H)
    # Включаем расширения GNU для доступа к системным функциям
    add_compile_definitions(_GNU_SOURCE)
endif()
file(GLOB ZLIB_SOURCES "include/zlib/*.c")
add_library(zlib STATIC ${ZLIB_SOURCES})

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h" "src/*/*.cpp" "src/*/*.h")

# Создаем исполняемый файл
add_executable(${PROJECT_NAME} ${SOURCES})

# Тесты (headless)
set(TEST_SOURCES ${SOURCES})
list(FILTER TEST_SOURCES EXCLUDE REGEX "src[/\\\\]main\\.cpp$")
add_executable(mc_tests ${TEST_SOURCES} tests/tests_main.cpp)
target_compile_definitions(mc_tests PRIVATE UNIT_TEST)
target_link_libraries(mc_tests PRIVATE glfw glad zlib)

add_executable(mc_bench ${TEST_SOURCES} tests/perf_hotspots.cpp)
target_compile_definitions(mc_bench PRIVATE UNIT_TEST)
target_link_libraries(mc_bench PRIVATE glfw glad zlib)

# Присоединяем (линкуем) библиотеки
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad zlib)

# Платформо-зависимые библиотеки
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 gdi32)
elseif (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation")
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE GL X11 pthread dl)
endif()

# Копирование ассетов при сборке (опционально)
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/data DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/save DESTINATION ${CMAKE_BINARY_DIR})

cmake_minimum_required(VERSION 3.24)
project(ModularUGCEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Output folders
if (MSVC)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)
endif()

add_subdirectory(engine)
add_subdirectory(editor)
add_subdirectory(docs)
add_subdirectory(tools)

# Set hello_engine as the default startup project in Visual Studio
if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT hello_engine)
    set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT hello_engine)
endif()

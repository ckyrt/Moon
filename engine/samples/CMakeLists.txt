add_executable(hello_engine
  hello_win32.cpp
)
target_link_libraries(hello_engine PRIVATE engine_core engine_render)
set_target_properties(hello_engine PROPERTIES FOLDER "samples")

# Windows specific settings
if(WIN32)
  set_target_properties(hello_engine PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug"
  )
  target_compile_definitions(hello_engine PRIVATE UNICODE _UNICODE)
endif()

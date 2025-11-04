add_library(editor_bridge STATIC
  NativeHost.h
  WebSocketHostStub.cpp
)
target_include_directories(editor_bridge PUBLIC ${CMAKE_CURRENT_LIST_DIR})
set_target_properties(editor_bridge PROPERTIES FOLDER "editor")

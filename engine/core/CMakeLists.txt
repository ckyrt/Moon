add_library(engine_core STATIC
  EngineCore.cpp
  EngineCore.h
  IEngine.h
)
target_include_directories(engine_core PUBLIC ${CMAKE_CURRENT_LIST_DIR})

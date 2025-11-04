add_library(engine_render STATIC
  IRenderer.h
  RenderCommon.h
  NullRenderer.cpp
  NullRenderer.h
  BgfxRenderer.h
  BgfxRendererStub.cpp
)
target_include_directories(engine_render PUBLIC ${CMAKE_CURRENT_LIST_DIR})

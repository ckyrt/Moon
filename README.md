# Modular UGC Engine + WebUI Editor (Version 2 - Minimal Demo)

This is a **minimal runnable skeleton** for Windows that shows:
- A Win32 window + message loop
- A **Null renderer** that clears the background (no external deps)
- EngineCore lifecycle (Initialize/Tick/Shutdown)
- WebUI React skeleton + IPC stubs (not required to run the C++ demo)

> ✅ You can build and run the **Hello Engine** sample on Windows without any third‑party libraries.

## Build (Windows, MSVC + CMake >= 3.24)
```bat
mkdir build
cd build
cmake .. -A x64
cmake --build . --config Debug
bin\Debug\hello_engine.exe
```
(Or run the `Release` binary accordingly.)

## Notes
- `/engine/render` contains a **NullRenderer** (default) and a **BgfxRenderer** stub.
  - NullRenderer requires no dependencies and is used by the sample.
  - BgfxRenderer is a placeholder for future integration (AI/you can wire bgfx later).
- The WebUI (`/editor/webui`) is a basic React/Vite skeleton; start it separately with `npm install && npm run dev`.

## Layout
```
/engine
  /core           # EngineCore (Initialize/Tick/Shutdown)
  /render         # IRenderer + NullRenderer (default) + bgfx stub
  /samples        # hello_engine (Win32 window + render loop)
/editor
  /webui          # React skeleton (Blueprint-like layout)
  /bridge         # IPC stubs
/docs             # ADR/spec placeholders
```

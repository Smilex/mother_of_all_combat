@echo off

cl /std:c++latest -Zi -DGRAPHICS=GRAPHICS_API_OPENGL_33 -DPLATFORM=PLATFORM_DESKTOP src/main.cpp raylib.lib

@echo off

SETLOCAL

SET my_flags=/EHsc /Od /Z7 /Fd /Fetetris.exe

SET my_include=/I "..\Libraries\imgui" /I "..\Libraries\imgui\examples" /I "..\Libraries\DX\Include"
REM SET my_source=..\Source\*.cpp ..\Libraries\imgui\*.cpp ..\Libraries\imgui\examples\imgui_impl_dx11.cpp ..\Libraries\imgui\examples\imgui_impl_win32.cpp
SET my_source=..\source\unit_windows.cpp ..\source\platform_windows\network_client.cpp

SET my_lib=user32.lib gdi32.lib shell32.lib Ws2_32.lib ..\Libraries\DX\Lib\x64\dxgi.lib ..\Libraries\DX\Lib\x64\d3d11.lib ..\Libraries\DX\Lib\x64\d3dx11.lib ..\Libraries\DX\Lib\x64\d3dx10.lib





pushd .

cd build
cl %my_flags% %my_include% %my_source% %my_lib% > build_log.txt
color_output.exe build_log.txt

popd

copy build\tetris.exe .


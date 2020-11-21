
LINUX_SOURCE=source/tetris.cpp source/platform_linux/game_presentation.cpp source/platform_linux/main.cpp source/platform_linux/renderer.cpp

linux:
	g++ -O2 -std=gnu++11 -lX11 -lGL $(LINUX_SOURCE) -I"source" -otetris.exe

pi:
	g++ -O2 -std=gnu++11 -ldl source/unit_pi.cpp -otetris.exe


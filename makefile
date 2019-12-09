
SOURCE=source/unit_pi.cpp

pi:
	g++ -O2 -std=gnu++11 -ldl $(SOURCE) -otetris.exe


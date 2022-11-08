if not exist build mkdir build
cd build
if exist main.exe rm main.exe
cmake -S ../ -B . -G "MinGW Makefiles"
mingw32-make.exe && mingw32-make.exe Shaders
if exist main.exe .\main.exe
cd ..

rd /s /q build
cmake -B build -A Win32
cmake --build build --config Release
copy .\build\Release\chuniio.dll .\chuniio.dll
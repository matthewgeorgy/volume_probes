@echo off

set CPP_FLAGS=/W4 /Zi /wd4201 /I ../include
set CPP_SRC=../source/*.cpp

if not exist build (
	mkdir build
)

pushd build\

cl %CPP_FLAGS% %CPP_SRC% kernel32.lib user32.lib gdi32.lib

popd

@echo off

set CPP_FLAGS=/W4 /Zi /wd4201 /I ../include /nologo
set CPP_SRC=../source/*.cpp

if not exist build (
	mkdir build
)

pushd build\

cl %CPP_FLAGS% %CPP_SRC% kernel32.lib user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib

for %%f in (..\source\shaders\*.vs) do (
    fxc /Zi /nologo /E main /T vs_5_0 /Fo %%~nf_vs.cso /Fd %%~nf_vs.pdb %%f
)

for %%f in (..\source\shaders\*.ps) do (
    fxc /Zi /nologo /E main /T ps_5_0 /Fo %%~nf_ps.cso /Fd %%~nf_ps.pdb %%f
)

popd

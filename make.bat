@echo off

set CPP_FLAGS=/W4 /MP /Zi /wd4201 /I ../include /I ../include/imgui /I ../include/implot /nologo /MT /Fe"main" /FC
set CPP_SRC=../source/*.cpp ../source/imgui/*.cpp ../source/implot/*.cpp
set CPP_LIBS=glfw3_mt.lib user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib shell32.lib

if not exist build (
	mkdir build
)

pushd build\

cl %CPP_FLAGS% %CPP_SRC% /link %CPP_LIBS%

for %%f in (..\source\shaders\*.vs) do (
    fxc /Zi /nologo /E main /T vs_5_0 /Fo %%~nf_vs.cso /Fd %%~nf_vs.pdb %%f
)

for %%f in (..\source\shaders\*.ps) do (
    fxc /Zi /nologo /E main /T ps_5_0 /Fo %%~nf_ps.cso /Fd %%~nf_ps.pdb %%f
)

popd

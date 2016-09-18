@echo off

set CommonCompilerFlags=/MTd /nologo /Gm- /GR- /EHa- /Od /Oi /WX /W4 /wd4201 /wd4100 /wd4189 /wd4505 /Zi /FC -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1
set CommonLinkerFlags=/incremental:no /opt:ref user32.lib gdi32.lib winmm.lib

If NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 32 bits build
REM cl  %CommonCompilerFlags% ..\code\win32_handmade.cpp /link /subsystem:windows,5.1 %CommonLinkerFlags% 

REM 64 bits build
del *.pdb > NUL 2> NUL
REM set timestamp=%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
REM can use %random% also
REM Optimization switches /O2 /Oi /fp:fast
cl  %CommonCompilerFlags% ..\code\handmade.cpp /Fmhandmade.map /LD /link /incremental:no /opt:ref /PDB:handmade_%random%.pdb /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples
cl  %CommonCompilerFlags% ..\code\win32_handmade.cpp /Fmwin32_handmade.map /link %CommonLinkerFlags% 

REM xcopy win32_handmade.exe ..\data\ /Y
REM xcopy handmade.dll ..\data\ /Y

popd

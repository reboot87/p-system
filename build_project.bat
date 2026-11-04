@echo off
call "C:\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
"C:\2017\Community\MSBuild\15.0\Bin\MSBuild.exe" "F:/project/github/p-system/build/AireRecord.sln" /p:Configuration=Release /p:Platform=x64
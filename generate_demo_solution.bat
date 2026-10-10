@pushd %~dp0
set GAME_ROOT=%~dp0\demo
cmake -S . ^
  -B %GAME_ROOT%\workspace ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DOPTION_BUILD_GAME=ON ^
  -DOPTION_BUILD_EDITOR=OFF ^
  -DOPTION_BUILD_GAME_LAUNCHER=OFF ^
  -DOPTION_BUILD_TESTS=OFF ^
  -DOPTION_GAME_DIR=%GAME_ROOT%
@popd

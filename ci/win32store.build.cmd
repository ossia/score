set SCORE_DIR=%cd%
mkdir build
cd build

mkdir install

set PATH=%PATH%;c:\ossia-sdk\llvm\bin
cmake -GNinja %SCORE_DIR% ^
  -DCMAKE_C_COMPILER=c:/ossia-sdk/llvm/bin/clang.exe ^
  -DCMAKE_CXX_COMPILER=c:/ossia-sdk/llvm/bin/clang++.exe ^
  -DCMAKE_C_FLAGS=" -g0 -s -flto=full -fno-stack-protector -Ofast -fno-finite-math-only -D_WIN32_WINNT_=0x0A00 -DWINVER=0x0A00 " ^
  -DCMAKE_CXX_FLAGS=" -g0 -s -flto=full -fno-stack-protector -Ofast -fno-finite-math-only -D_WIN32_WINNT_=0x0A00 -DWINVER=0x0A00 " ^
  -DCMAKE_EXE_LINKER_FLAGS=" -flto=full " ^
  -DOSSIA_SDK=c:\ossia-sdk ^
  -DCMAKE_INSTALL_PREFIX=install ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_UNITY_BUILD=1 ^
  -DOSSIA_STATIC_EXPORT=0 ^
  -DSCORE_INSTALL_HEADERS=0 ^
  -DSCORE_DISABLED_PLUGINS="score-plugin-faust;score-plugin-jit" ^
  -DSCORE_DEPLOYMENT_BUILD=1 ^
  -DSCORE_MSSTORE_DEPLOYMENT=1

cmake --build .
cmake --build . --target install/strip

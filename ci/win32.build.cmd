mkdir build
cd build

set PATH=%PATH%;c:\ossia-sdk\llvm\bin
cmake -GNinja %BUILD_SOURCESDIRECTORY% ^
  -DCMAKE_C_COMPILER=c:/ossia-sdk/llvm/bin/clang.exe ^
  -DCMAKE_CXX_COMPILER=c:/ossia-sdk/llvm/bin/clang++.exe ^
  -DOSSIA_SDK=c:\ossia-sdk ^
  -DCMAKE_INSTALL_PREFIX=install ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_UNITY_BUILD=1 ^
  -DOSSIA_STATIC_EXPORT=1 ^
  -DSCORE_INSTALL_HEADERS=1 ^
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,--export-all-symbols" ^
  -DDEPLOYMENT_BUILD=1

cmake --build .
cmake --build . --target package

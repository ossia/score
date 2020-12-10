mkdir build
cd build

set PATH=%PATH%;c:\score-sdk\llvm\bin
cmake -GNinja $(Build.SourcesDirectory) ^
  -DCMAKE_C_COMPILER=c:/score-sdk/llvm/bin/clang.exe ^
  -DCMAKE_CXX_COMPILER=c:/score-sdk/llvm/bin/clang++.exe ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DOSSIA_SDK=c:\score-sdk ^
  -DCMAKE_INSTALL_PREFIX=install ^
  -DCMAKE_PREFIX_PATH="c:/score-sdk/qt5-static;c:/score-sdk/llvm-libs;c:/score-sdk/SDL2;c:/score-sdk" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_UNITY_BUILD=1 ^
  -DOSSIA_STATIC_EXPORT=1 ^
  -DSCORE_INSTALL_HEADERS=1 ^
  -DDEPLOYMENT_BUILD=1

cmake --build .
cmake --build . --target package

dir
move "ossia score-3.0.0-win64.exe" "$(Build.ArtifactStagingDirectory)\ossia score-$(gitTagNoV)-win64.exe"
dir
set SCORE_DIR=%cd%
mkdir build
cd build

cmake %SCORE_DIR% ^
  -G"Visual Studio 17 2022" ^
  -T ClangCL ^
  -DCMAKE_PREFIX_PATH="%Qt6_DIR%" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DOSSIA_SDK=c:\ossia-sdk-msvc ^
  -DCMAKE_INSTALL_PREFIX=install ^
  -DCMAKE_UNITY_BUILD=1 ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ^
  -DSCORE_DEPLOYMENT_BUILD=1

cmake --build . --config Debug
cmake --build . --config Debug --target INSTALL

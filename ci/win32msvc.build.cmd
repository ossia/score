set SCORE_DIR=%cd%
mkdir build
cd build

cmake %SCORE_DIR% ^
  -DCMAKE_PREFIX_PATH="%Qt6_DIR%" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DQT_VERSION="Qt6;6.2" ^
  -DOSSIA_SDK=c:\ossia-sdk-msvc ^
  -DCMAKE_INSTALL_PREFIX=install ^
  -DCMAKE_UNITY_BUILD=1 ^
  -DSCORE_DEPLOYMENT_BUILD=1

cmake --build . --config Debug
cmake --build . --config Debug --target INSTALL

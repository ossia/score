The Spout SDK files can either be included directly in your project
or compiled as a dll. 

The Spout applications and examples have been compiled with the SDK
files included in the projects and so are not dependent on a Spout dll.

The installation folders are set up correctly for building the project.
However, building from within the installation folder is not recommended.
Copy the distribution "SPOUTSDK" folder elsewhere before compilation.
Open the "SpoutSDK" solution, change to "Release" and build the project.
Spout.dll and Spout.lib will be in the Win32\Release folder.

To use the .dll created from this project, in your code define
SPOUT_IMPORT_DLL in your preprocessor compilation defines.

Thanks and credit to Malcolm Bechard for creating the dll project.

https://github.com/mbechard

COMPATIBILITY

The dll created from this project has the advantage that all the functions
in the Spout SDK classes are available. However, it is only suitable for 
use with Visual Studio compilers. For other compilers, see the C compatible
dll in the SPOUT_LIBRARY folder.

CHANGES REQUIRED FOR SPOUT 2.006

o Include spoutCopy.cpp/h in your project.
o Include the fbo ID as an argument in DrawSharedTexture if an fbo is currently bound.
o There are new functions to get and set sharing mode if you require them.

The SDK should rebuild without any further action. If you are using a dll, there are updated binaries for the Spout and SpoutLibrary dlls.


# BUILD INSTRUCTIONS 

  * i-score requirements : Qt5, CMake 3.0
  * Network plug-in requirements : boost, an implementation of dns_sd.h (ex. : libavahi-compat-libdnssd-dev on Debian/Ubuntu).

### Build : 
  
    $ mkdir -p build_folder
    $ cd build_folder
    $ cmake path/to/i-score/repo
    $ make install
    
### And run : 

    $ ./iscore_core

# DOCUMENTATION

In order to have documentation of the sources you have to install Doxygen (www.doxygen.org),
and the Graphviz package to generate graph with Doxygen (www.graphviz.org).
To generate documentation : 
  
    $ cd Doxygen
    $ doxygen
    
And open : `Doxygen/html/index.html`

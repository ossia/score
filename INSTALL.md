# BUILD INSTRUCTIONS 

  * i-score requirements : Qt5.4, CMake 3.0, boost
  * Network plug-in requirements : libkf5-kdnssd for Bonjour

### Build : 
  
    $ mkdir -p build_folder
    $ cd build_folder
    $ cmake path/to/i-score/repo
    $ make
    
### And run : 

    $ ./i-score

# DOCUMENTATION

In order to have documentation of the sources you have to install Doxygen (www.doxygen.org),
and the Graphviz package to generate graph with Doxygen (www.graphviz.org).
To generate documentation : 
  
    $ cd Doxygen
    $ doxygen
    
And open : `Doxygen/html/index.html`

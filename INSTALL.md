# BUILD INSTRUCTIONS 

  * i-score requirements : Qt >= 5.3, CMake >= 3.1, boost >= 1.55
  * Network plug-in requirements : libkf5-kdnssd for Bonjour

### Packages : 

#### On debian-like systesms
    
    sudo apt-get install cmake qtbase5-dev qtdeclarative5-dev qttools5-dev libqt5svg5-dev libboost-dev build-essentials g++

### Build : 
  
    $ mkdir -p build_folder
    $ cd build_folder
    $ cmake path/to/i-score/repo
    $ make -j4
    
### And run : 

    $ ./i-score

# DOCUMENTATION

In order to have documentation of the sources you have to install Doxygen (www.doxygen.org),
and the Graphviz package to generate graph with Doxygen (www.graphviz.org).
To generate documentation : 
  
    $ cd Doxygen
    $ doxygen
    
And open : `Doxygen/html/index.html`

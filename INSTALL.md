==  BUILD INSTRUCTIONS  ==

  * i-score requirements : Qt5, CMake 3.0
  * Network plug-in requirements : boost, an implementation of dns_sd.h (ex. : libavahi-compat-libdnssd-dev on Debian/Ubuntu).
  
    $ cmake path/to/i-score
    $ make install
    
    And run : 
    $ ./iscore_core

   ==<[ Doxygen Documentation ]>==


In order to have documentation of the sources you have to install Doxygen (www.doxygen.org),
and the Graphviz package to generate graph with Doxygen (www.graphviz.org).
To generate documentation,
load ./doxygen/Doxyfile with Doxygen and run it.

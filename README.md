# AttractorSynth
Sonification of a Chaotic attractor, using KNN and Eigendecomposition for shape recognition.

## Description:
This application starts with a gaussian distributed set of points set by constant nPoints. These points are then fed into the graphics card and ping-ponged between two vertex buffer objects. The vertex shader contains the algorithm which recursively transforms the points according to four parameters (alpha, beta, gamma, mu) whilst the timestep controls the rate of change. The transform feedback means the points don't leave the graphics card buffer so there is no overhead in transferring these points to the cpu. 

## Install:
-Built with openframeworks of_v20161230_vs_nightly build **important** only the nightly build seems to support transform feedback, use this instead of the main branch.  
-Requires [ofxMaxim]( https://github.com/micknoise/Maximilian ) and [ofxImGui]( https://github.com/jvcleave/ofxImGui )  
-All other libraries are header only and are included.  
- This uses [Eigen 3.3.1]( http://eigen.tuxfamily.org/ ) for the Matrix processing, and [nanoflann]( https://github.com/jlblancoc/nanoflann ) for the light KNN implementation. Both are header-only libraries and have been included in /lib.
-tested on Windows VS 2015 only, this should work on OSX but only if the openframeworks transform feedback example in 'examples/gl' also works for your computer.   

# AttractorSynth
Realtime sonification of a Chaotic attractor, using KNN and Eigendecomposition for shape recognition, and maximilian for sound synthesis.

## Description:
### Transform Feedback  
This application starts with a gaussian distributed set of n Points. These points are then fed into the graphics card and ping-ponged between two vertex buffer objects. The vertex shader contains the algorithm which recursively transforms the points according to four parameters (alpha, beta, gamma, mu) whilst the timestep controls the rate of change. The transform feedback means the points don't leave the graphics card buffer, so there is no overhead in transferring these points to the cpu. In the vertex shader I also calculate each particle speed and tack this onto the opacity component (.w) of the colour vec4 (colour and position are interleaved). 
### Shape Analysis  
In order to do shape analysis we allocate a buffer ('buf') to look at a random sample of n of these points, then use Eigen library to perform eigen decomposition on the covariance matrix of x, y, z & w(speed). This is the first step in PCA, and gives use 4 eigenvalues and their corresponding eigenvectors. When the 'learn' button is pressed, the largest eigenvector(4 components), all the eigenvalues and the average speed are sampled into an n X 9 eigen matrix for several frames, this should give us a good general description of each shape the attractor makes.
### The Audio State  
Whilst the shape descriptors are being learnt an audio state is saved, containing audio parameters for two FM oscillators, noise, and a granulator sample - this audio state is associated with the sampled shape descriptors. When 'play' is pressed a weighted knn is performed, with the current_descriptors used as input on the trained state_samples with nanoflann. A similarity score (which sums to 1) is given to each seperate state. The oscillator/noise parameters are then linearly interpolated, and each grain in the granulator has a weighted random selection, according to the states similarity.  
i.e.  
State A contains sample tree.wav and has 0.1 similarity score.  
State B contains sample apple.wav and has 0.9 similarity score.  
When playing, one buffer of audio is likely to contain 90 grains of apple, and 10 grains of tree.  
  
The granulator's other parameters such as speed, position and duration are determined by iterating through the graphics card point buffer and creating a new grain based on each particles position and speed. These particle values can also played directly with the 'noisefactor' parameter, adding a sheen of noise that follows the movement and point distribution. This produces quite a convincing sonic effect that closely follows the visuals and shape.  

## Install:
- Built with openframeworks of_v20161230_vs_nightly build **important** only the nightly build seems to support transform feedback, use this instead of the main branch.  
- Requires [ofxMaxim]( https://github.com/micknoise/Maximilian ) and [ofxImGui]( https://github.com/jvcleave/ofxImGui )  
- All other libraries are header only and are included.  
- This uses [Eigen 3.3.1]( http://eigen.tuxfamily.org/ ) for the Matrix processing, and [nanoflann]( https://github.com/jlblancoc/nanoflann ) for the light KNN implementation. Both are header-only libraries and have been included in /lib.  
- tested on Windows VS 2015 only, this should work on OSX but only if the openframeworks transform feedback example in 'examples/gl' also works for your computer.   

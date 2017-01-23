# SpringSynth
Sonified Verlet Spring Simulation - using ofxMaxim and ofxImGui

## Description:
Contains custom PointMass and Link classes - each pointmass acts as a doubly linked list and is connected to one or two other pointmasses via a link. Each pointmass can be anchored or free, and has an oscillator, where frequency is determined by distance to the nearest anchor to the power of frequency ratio supplied by user. Similarly with FM frequency and Modulation index.

## Install:
-Built with openframeworks of_v20161230_vs_nightly build, though any recent version will work.
-Requires [ofxMaxim]( https://github.com/micknoise/Maximilian ) and [ofxImGui]( https://github.com/jvcleave/ofxImGui )  
-tested on Windows only, but should work on OSX.  

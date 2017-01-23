#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){
	//ofSetupOpenGL(1024, 768, OF_WINDOW);				// <-------- setup the GL context

	//MUST BE OPENGL3 supported GphcsCard
	ofGLWindowSettings settings;
	settings.setGLVersion(3, 3);
	ofCreateWindow(settings);

	ofRunApp(new ofApp());
}

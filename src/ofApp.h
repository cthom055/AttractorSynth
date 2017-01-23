#pragma once

#include "ofMain.h"
#include "ofxImGui.h"
#include "ofxMaxim.h"
#include "maximilian.h"
#include "maxiStereoGrain.h"

#include <vector>
#include <random>
#include <experimental/filesystem>
#include <algorithm>>

#include <../lib/Eigen/Core>
#include <../lib/Eigen/Dense>
#include <../lib/Eigen/QR>
#include <../lib/Eigen/Eigenvalues>
#include "../lib/nanoflann/include/nanoflann.hpp"

//this is a quick struct to save the audio state parameters
struct audioState {
	int							id_;
	float						similarity, weight;
	int							knn;
	//audio params
	float						audio_freq1, audio_freq2, audio_pan1, audio_pan2;
	float						audio_fmvals1[2], audio_fmvals2[2], audio_noise1, audio_noise2, audio_amp1, audio_amp2;
	float						audio_noisefactor;
	maxiSample*					audio_granSample;
	audioState(float f1, float f2, float fmv1[2], float fmv2[2], float mi1, float mi2, float a1, float a2, float nsfct, maxiSample *sample, int id)
		: audio_freq1(f1), audio_freq2(f2), audio_pan1(mi1), audio_pan2(mi2), audio_amp1(a1), audio_amp2(a2),
		audio_noisefactor(nsfct), audio_granSample(sample), similarity(0), weight(0), id_(id)
	{
		audio_fmvals1[0] = fmv1[0];
		audio_fmvals1[1] = fmv1[1];
		audio_fmvals2[0] = fmv2[0];
		audio_fmvals2[1] = fmv2[1];
	}
};

//function to remove rows from within an Eigen Matrix
//adapted from SO http://stackoverflow.com/questions/13290395/how-to-remove-a-certain-row-or-column-while-using-eigen-library-c
inline void remove_rows(Eigen::Matrix<float, -1, 9> &matrix, unsigned int startrow, unsigned int size)
{
	matrix.block(startrow, 0, (matrix.rows() - startrow) - size, matrix.cols()) = matrix.block(startrow + size, 0, (matrix.rows() - (startrow + size)), matrix.cols());
	matrix.conservativeResize(matrix.rows() - size, matrix.cols());
}

typedef nanoflann::KDTreeEigenMatrixAdaptor< Eigen::Matrix<float, -1, 9> >  kd_tree_t;
const int LEARNSAMPLE_SIZE = 30;	//this defines how many frames we snapshot for each state
using namespace std::experimental::filesystem;

class ofApp : public ofBaseApp {

public:
	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);
	void audioOut(float * output, int bufferSize, int nChannels, int deviceID, long unsigned long tickCount);


	bool imGui();			
	void doStateInterp();
	void exit();

	ofEasyCam					camera;
	ofMesh						mesh;
	ofVbo						vbos[2];							//vbos for our particles 
	ofFbo						fbo;								//fbo for blurring
	int							idx;								//indexes our vbos
	ofBufferObject				buffers[2];
	glm::vec4					*buf;
	ofShader					shader, blurshader;
	std::vector<ofVec3f>		points;
	std::default_random_engine	generator;							//for generating gaussian ball of originl points
	ofShader::TransformFeedbackSettings settings;					//GLSL settings

	//ReaderWriterQueue<float, 2048> q;
	//ReaderWriterQueue<maxiGrain<triangleWinFunctor, maxiSample>*, 2048> q2;
	ofLight						light;

	//PARAMS FOR ATTRACTOR

	// GUI + Parameters
	unsigned int				nPoints, nSamples;
	ofxImGui::Gui gui;
	bool guiVisible;
	bool mouseOverGui;
	bool learning, playing;		//gui/interpolating states
	unsigned int				idcount;

	ofParameter<float> alpha{ "Alpha", 0.2f, 0.01f, 25.0f };
	ofParameter<float> beta{ "Beta", 0.2f, 0.01f, 25.0f };
	ofParameter<float> gamma{ "Gamma", 0.2f, 0.01f, 25.0f };
	ofParameter<float> mu{ "Mu", 0.2f, 0.01f, 25.0f };
	ofParameter<float> dt{ "Timestep", 0.05f, -.4f, .4f };
	ofParameterGroup attractorparams{ "Attractor Parameters", alpha, beta, gamma, mu };

	ofParameter<bool> follow{ "Follow", false };
	ofParameter<bool> drawcentroid{ "DrawCentroid", false };
	ofParameterGroup drawparams{ "Camera", follow, drawcentroid };

	ofParameterGroup learnparams{ "Learn" };
	std::vector<audioState*> states;

	//MAXIM
	maxiSample					*sample1, *sample2, *sample3;
	maxiSample					*selectedSample;
	bool						selected;
	std::vector<maxiSample*>	samplebank;
	std::vector<path>			samplebank_paths;
	maxiStereoGrainPlayer		*grainplayer;
	maxiGrainWindowCache<hannWinFunctor> *windowCache;
	maxiOsc						osc1, osc2, fm1, fm2;

	float						freq1, freq2, fmvals1[2], fmvals2[2], pan1, pan2, amp1, amp2, noisefactor;
	int							nGrains;

	//SOUND
	ofSoundStream				soundstream;
	unsigned int				samplerate;
	unsigned int				buffersize;

	std::vector<double>			tstfwt;
	std::vector<double>			copyfwt;

	int							j, k;
	float						myoutput[2];

	//Eigen / KNN////////////////////////////////
	kd_tree_t					*state_index;
	audioState					*active_state;	//saves our audio params
	Eigen::Matrix<float, -1, 9> state_matrix;	
	Eigen::Matrix<float, 1, 9>	current_state;
	std::vector<int>			state_labels;
	int							rowcount; //tells us where we are when learning
	Eigen::MatrixX4f			matrix; //matrix for randomly sampled raw points
	Eigen::SelfAdjointEigenSolver<Eigen::MatrixX4f> es;
	Eigen::Matrix<float, 1, 4>	centroid;
	float						avgspd;
	Eigen::Vector4f				eigenvec1, eigenvec2, eigenvec3, eigenvec4;
	float						eigenval1, eigenval2, eigenval3, eigenval4;
	bool						copying;
};

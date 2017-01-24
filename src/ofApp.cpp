#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {

	//SOUND params//////////////////////////////////////////////////
	buffersize = 2048;
	samplerate = 44100;
	freq1 = 50;
	freq2 = 50;
	fmvals1[0] = 0;
	fmvals1[1] = 0;
	fmvals2[0] = 0;
	fmvals2[1] = 0;
	pan1 = 0;
	pan2 = 0;
	amp1 = 0;
	amp2 = 0;
	noisefactor = 0;
	nGrains = 50;

	//GUI/////////////////////////////////////////////////////
	learning = false;
	playing = false;
	this->gui.setup();
	this->guiVisible = true;
	active_state = NULL;
	idcount = 0;

	//GRANULATOR/SAMPLE STUFF////////////////////////////////
	sample1 = new maxiSample();
	sample2 = new maxiSample();
	sample3 = new maxiSample();

	grainplayer = new maxiStereoGrainPlayer();
	windowCache = new maxiGrainWindowCache<hannWinFunctor>();

	//load default samples////////////////////////////////////////////////
	path p1 = ofToDataPath("guitarloop2.wav");
	if ((sample1->load(p1.string(), 0))) {
		cout << sample1->getSummary() << endl;
		sample1->normalise();
		samplebank.push_back(sample1);
		samplebank_paths.push_back(p1);	
	}
	else {
		cout << "couldn't load default sample 1" << endl;
	}
	path p2 = ofToDataPath("glitch1a.wav");
	if ((sample2->load(ofToDataPath("glitch1a.wav"), 0))) {
		cout << sample2->getSummary() << endl;
		sample2->normalise();
		samplebank.push_back(sample2);
		samplebank_paths.push_back(p2);
	}
	else {
		cout << "couldn't load default sample 2" << endl;
	}
	path p3 = ofToDataPath("synth1a.wav");
	if ((sample3->load(ofToDataPath("synth1a.wav"), 0))) {
		cout << sample3->getSummary() << endl;
		sample3->normalise();
		samplebank.push_back(sample3);
		samplebank_paths.push_back(p3);
	}
	else {
		cout << "couldn't load default sample 3" << endl;
	}

	selectedSample = (*samplebank.begin());	//set initial sample ptr, this will change for each state
	selected = false;

	//_____GRAPHICS CARD CHECKING____//////////////////////////////////
	if (!ofIsGLProgrammableRenderer()) {
		cout << "!OpenGL3 Not Supported!" << endl;
		ofSleepMillis(1000);
		exit();
	}

	//INIT MATRIX (with mesh)////////////////////////////////////////
	mesh.setMode(OF_PRIMITIVE_POINTS);
	mesh.getVertices().resize(nPoints);
	std::normal_distribution<double> distribution(.5, 0.2);
	nPoints = 65536;
	nSamples = 8192;
	matrix.resize(nPoints, 4);

	//Fill the mesh with random distribution
	for (int i = 0; i < nPoints; i++) {
		float x = matrix(i, 0) = max(min(distribution(generator), 0.9), 0.);
		float y = matrix(i, 1) = max(min(distribution(generator), 0.9), 0.);
		float z = matrix(i, 2) = max(min(distribution(generator), 0.9), 0.);
		matrix(i, 3) = 0;
		mesh.addVertex(ofVec3f(x, y, z));
	}

	cout << "initial array of" << nPoints << "points" << endl;

	//our gl settings for Transform feedback - important: capture varyings
	settings.shaderFiles[GL_VERTEX_SHADER] = "vert.glsl";
	settings.bindDefaults = false;
	settings.varyingsToCapture = { "v_position", "v_color" };	//captures these variables from shader - to put into buffers
	shader.setup(settings);

	//allocate space
	buffers[0].allocate(sizeof(glm::vec4) * 2 * nPoints, GL_DYNAMIC_DRAW);
	buffers[1].allocate(sizeof(glm::vec4) * 2 * nPoints, GL_DYNAMIC_DRAW);
	//draw initial mesh into both buffers
	shader.beginTransformFeedback(GL_POINTS, buffers[0]);
	mesh.draw();
	shader.endTransformFeedback(buffers[0]);
	shader.beginTransformFeedback(GL_POINTS, buffers[1]);
	mesh.draw();
	shader.endTransformFeedback(buffers[1]);
	// now set vbos to read buffers, colours and vertices are interlaced
	vbos[0].setVertexBuffer(buffers[1], 4, sizeof(glm::vec4) * 2, 0);
	vbos[0].setColorBuffer(buffers[1], sizeof(glm::vec4) * 2, sizeof(glm::vec4));
	vbos[1].setVertexBuffer(buffers[0], 4, sizeof(glm::vec4) * 2, 0);
	vbos[1].setColorBuffer(buffers[0], sizeof(glm::vec4) * 2, sizeof(glm::vec4));

	//our equation parameters
	alpha = 2.2;
	beta = 2.22;
	gamma = 2.45;
	mu = 0.23;
	dt = .01;		//timestep

	//setup camera & draw settings
	camera.setupPerspective(false, 60, 0, 0);
	camera.setScale(0.01, 0.01, 0.01);
	camera.setDistance(17.5);
	ofSetVerticalSync(true);
	ofSetFrameRate(60);
	ofEnablePointSprites();
	glPointSize(0.5);
	glEnable(GL_DEPTH_TEST);

	//init indexes
	idx = 0;		//idx for gl buffers
	j = 0;			//j is for audio cycling through gpu buffers

	//EIGEN//////////////////////////////////////////////////
	eigenval1 = 0;
	eigenval2 = 0;
	eigenval3 = 0;
	eigenval4 = 0;
	//KNN///////////////////////////////////////////////////
	centroid << 0, 0, 0, 0;
	state_index = new kd_tree_t(9, state_matrix, 20);	//construct a kdtree with leafsize 20
	state_index->index->buildIndex();
	state_index->kdtree_get_point_count();

	//set buf initially
	buf = (glm::vec4*) buffers[0].map(GL_READ_ONLY);
	buffers[0].unmap();	

	soundstream.setup(2, 0, samplerate, buffersize, 4);
	soundstream.start();
}

//--------------------------------------------------------------
void ofApp::update() {
	float val = 0;
	//this reads the buffer from the gfx card
	buf = (glm::vec4*) buffers[idx].map(GL_READ_ONLY);

	if (buf) {
		centroid << 0, 0, 0, 0;
		int rnd_indx = 0;

		//a random sampling of nSamples from the total buffer of points - these will be used to calculate the covariance matrix
		for (int i = 0; i < nSamples; i++) {
			srand(time(NULL) + i);
			rnd_indx = rand() % nPoints;	//constrain to samplesize..
			centroid(0) += matrix(i, 0) = buf[rnd_indx * 2].x;			//remember every other buffer is a vec4 color in the gfx..
			centroid(1) += matrix(i, 1) = buf[rnd_indx * 2].y;
			centroid(2) += matrix(i, 2) = buf[rnd_indx * 2].z;
			centroid(3) += matrix(i, 3) = buf[(rnd_indx * 2) + 1].w;	//this is the speed of each particle, stored in the 4th component of color vec4
		}
		centroid /= nSamples;

		//calculate the covariance matrix
		Eigen::MatrixX4f centered = matrix.rowwise() - centroid;
		Eigen::MatrixX4f cov = (centered.adjoint() * centered) / double(matrix.rows() - 1);			
		es.compute(cov);
		eigenvec1 = es.eigenvectors().col(0);
		eigenvec2 = es.eigenvectors().col(1);
		eigenvec3 = es.eigenvectors().col(2);
		eigenvec4 = es.eigenvectors().col(3);
		eigenval1 = es.eigenvalues() (0);
		eigenval2 = es.eigenvalues() (1);
		eigenval3 = es.eigenvalues() (2);
		eigenval4 = es.eigenvalues() (3);
		current_state << (es.eigenvectors().col(3)).transpose(), (es.eigenvalues() (3)), (es.eigenvalues() (2)), (es.eigenvalues() (1)), (es.eigenvalues() (0)), centroid.w();

		if (learning) {
			if (ofGetFrameNum() % 5 == 0) {
				if (rowcount == 0) {
					audioState *state = new audioState(freq1, freq2, fmvals1, fmvals2, pan1, pan2, amp1, amp2, noisefactor, selectedSample, idcount++); //save audio state before capturing geom info
					states.push_back(state);
					state_matrix.conservativeResize(state_matrix.rows() + LEARNSAMPLE_SIZE, state_matrix.cols());	//resize matrix to add n empty rows
				}

				state_labels.push_back(idcount - 1);		//add labels to each training sample
				state_matrix.row(state_matrix.rows() - (rowcount + 1)) << current_state;	//add current row
				rowcount++;

				if (rowcount == LEARNSAMPLE_SIZE) {
					state_index->index->buildIndex();		//build the KNN tree
					rowcount = 0;
					learning = false;
					cout << "... done" << endl;
				}
			}
		}

		if (playing && !learning) {
			active_state = NULL;
			doStateInterp();
		}

		else if (!playing && active_state) {
			freq1 = active_state->audio_freq1;
			freq2 = active_state->audio_freq2;
			fmvals1[0] = active_state->audio_fmvals1[0];
			fmvals1[1] = active_state->audio_fmvals1[1];
			fmvals2[0] = active_state->audio_fmvals2[0];
			fmvals2[1] = active_state->audio_fmvals2[1];
			pan1 = active_state->audio_pan1;
			pan2 = active_state->audio_pan2;
			amp1 = active_state->audio_amp1;
			amp2 = active_state->audio_amp2;
			noisefactor = active_state->audio_noisefactor;
			selectedSample = active_state->audio_granSample;
		}
	}

	buffers[idx].unmap();							//this is important
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackgroundGradient(220, 40);
	//alternate between 1 - 0
	idx = 1 - idx;
	//Do the feedback, this ping pongs between both buffers and vbos (both vbos access the opposite buffer)
	shader.beginTransformFeedback(GL_POINTS, buffers[idx]);
	shader.setUniform1f("time", ofGetElapsedTimeMillis());
	shader.setUniform1f("a", alpha.get());
	shader.setUniform1f("b", beta.get());
	shader.setUniform1f("g", gamma.get());
	shader.setUniform1f("m", mu.get());
	shader.setUniform1f("dt", dt.get());
	vbos[idx].draw(GL_POINTS, 0, nPoints);
	shader.endTransformFeedback(buffers[idx]);

	//lights, camera...
	if (follow.get()) {
		camera.setScale(0.01, 0.01, 0.01);
		camera.lookAt(ofVec3f(centroid.x(), centroid.y(), centroid.z()));
	}

	else {
		camera.lookAt(ofVec3f(0, 0, 0));
	}

	ofPushMatrix();
	ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2);
	camera.begin();
	//...action
	ofSetColor(0, 0, 0, 90);
	vbos[!idx].draw(GL_POINTS, 0, nPoints);

	if (drawcentroid.get()) {
		ofVec3f cent(centroid.x(), centroid.y(), centroid.z());
		ofSetColor(0, 255);
		ofDrawSphere(cent, 0.07);
		ofSetColor(255, 0, 0, 255);
		ofDrawLine(cent, cent + ofVec3f(eigenvec1.x() * eigenval1, eigenvec1.y() * eigenval1, eigenvec1.z() * eigenval1));
		ofSetColor(0, 255, 0, 255);
		ofDrawLine(cent, cent + ofVec3f(eigenvec2.x() * eigenval2, eigenvec2.y() * eigenval2, eigenvec2.z() * eigenval2));
		ofSetColor(0, 0, 255, 255);
		ofDrawLine(cent, cent + ofVec3f(eigenvec3.x() * eigenval3, eigenvec3.y() * eigenval3, eigenvec3.z() * eigenval3));
		ofSetColor(255, 0, 255, 255);
		ofDrawLine(cent, cent + ofVec3f(eigenvec4.x() * eigenval4, eigenvec4.y() * eigenval4, eigenvec4.z() * eigenval4));
	}

	camera.end();
	ofPopMatrix();
	// Gui
	this->mouseOverGui = false;

	if (guiVisible)
	{
		mouseOverGui = imGui();
	}

	if (mouseOverGui)
	{
		camera.disableMouseInput();
	}

	else
	{
		camera.enableMouseInput();
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
}

//--------------------------------------------------------------
bool ofApp::imGui()
{
	auto mainSettings = ofxImGui::Settings();
	this->gui.begin();

	if (ofxImGui::BeginWindow("Params", mainSettings, false))  //<< the left hand parameter window - uses ofParams
	{
		ImGui::Text("%.1f FPS (%.3f ms/frame)", ofGetFrameRate(), 1000.0f / ImGui::GetIO().Framerate);

		if (ofxImGui::BeginTree(this->attractorparams, mainSettings))
		{
			ofxImGui::AddParameter(this->alpha);
			ofxImGui::AddParameter(this->beta);
			ofxImGui::AddParameter(this->gamma);
			ofxImGui::AddParameter(this->mu);
			ofxImGui::AddParameter(this->dt);
			ofxImGui::EndTree(mainSettings);
		}

		if (ofxImGui::BeginTree(this->drawparams, mainSettings))
		{
			ofxImGui::AddParameter(this->follow);
			ofxImGui::AddParameter(this->drawcentroid);
			ofxImGui::EndTree(mainSettings);
		}
	}

	ofxImGui::EndWindow(mainSettings);
	ImGui::SetNextWindowCollapsed(false, ImGuiSetCond_Once);
	ImGui::SetNextWindowPos(ImVec2(300, 100), ImGuiSetCond_Once);
	ImGui::Begin("Learning ##1");		//<< the learning window - contains all the audio param states
	ImGui::Separator();

	if (ImGui::Button("Learn"))
	{
		cout << "learning..." << endl;
		learning = true;
		playing = false;
	}

	ImGui::SameLine();
	ImGui::Text("\t\t");

	if (learning) {
		ImGui::SameLine();
		ImGui::ProgressBar((float)rowcount / LEARNSAMPLE_SIZE);
	}

	if (!playing && !learning) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(.5f, 0.6f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(.5f, 0.7f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(.5f, 0.8f, 0.8f));

		if (ImGui::Button("Play"))
		{
			cout << "playing..." << endl;
			playing = true;
		}

		ImGui::PopStyleColor(3);
	}

	else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(.1f, 0.6f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(.1f, 0.7f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(.1f, 0.8f, 0.8f));

		if (ImGui::Button("Stop"))
		{
			cout << "stopping..." << endl;
			playing = false;
		}

		ImGui::PopStyleColor(3);
	}

	//STATES;
	ImGui::Separator();
	ImGui::Separator();
	ImGui::TextWrapped("Current State:");
	ImGui::Text("EigenVec (%.2f,%.2f,%.2f, %.2f), Eigen Vals (%.2f,%.2f,%.2f,%.2f),\t Speed (%.2f)", current_state(0, 0), current_state(0, 1), current_state(0, 2), current_state(0, 3), current_state(0, 4),
		current_state(0, 5), current_state(0, 6), current_state(0, 7), current_state(0, 8));
	ImGui::Separator();
	ImGui::TextWrapped("Learnt States:");

	for (auto it = states.begin(); it < states.end(); it++)
	{
		int id = (*it)->id_;
		std::string state_id = std::to_string(id);

		if (!playing && !learning) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV((id % 10) * 0.1f, 0.6f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV((id % 10) * 0.1f, 0.7f, 0.7f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV((id % 10) * 0.1f, 0.8f, 0.8f));

			if (ImGui::Button(("State - " + state_id).c_str())) {
				active_state = (*it);
			}

			ImGui::PopStyleColor(3);
		}

		else {					//grey out buttons + disable if playing
			ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(.0f, 0.6f, 0.05f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(.0f, 0.6f, 0.05f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(.0f, 0.6f, 0.05f));
			ImGui::Button(("State - " + state_id).c_str());
			ImGui::PopStyleColor(3);
		}

		ImGui::SameLine();

		if (!playing && !learning) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(.0f, 0.6f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(.0f, 0.7f, 0.7f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(.0f, 0.8f, 0.8f));

			if (ImGui::Button(("Delete " + state_id).c_str())) {
				auto m_it = std::find(state_labels.begin(), state_labels.end(), id);		//find the first occurence of the id in the labels vector
				if (m_it != state_labels.end()) {
					unsigned int startrow = std::distance(state_labels.begin(), m_it);		//find the distance from start to that label
					remove_rows(state_matrix, startrow, LEARNSAMPLE_SIZE);					//remove from the state matrix
					state_labels.erase(std::remove(state_labels.begin(), state_labels.end(), id), state_labels.end());			//now remove matching labels
				}
				delete (*it);				//delete the state
				states.erase(it);
				active_state = NULL;		//must do this or problems..
			}
			ImGui::PopStyleColor(3);
		}

		else {					//grey out delete
			ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(.0f, 0.6f, 0.05f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(.0f, 0.6f, 0.05f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(.0f, 0.6f, 0.05f));
			ImGui::Button(std::string("Delete " + state_id).c_str());
			ImGui::PopStyleColor(3);
		}

		// Similarity bar
		float weight = 0;
		float similarity = 0;
		if (playing) { weight = (*it)->weight; similarity = (*it)->similarity; }

		ImGui::ProgressBar(similarity, ImVec2(0.0f, 0.0f));
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::Text("Weight : %f", weight);
		ImGui::Separator();
		ImGui::Separator();
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Once);

	//AUDIO HERE
	if (ImGui::CollapsingHeader("Audio"))
	{
		ImGui::Separator();
		float *f1, *f2, (*fmv1)[2], (*fmv2)[2], *pn1, *pn2, *a1, *a2, *ns;
		maxiSample **smp;

		if (!playing && active_state) {
			f1 = &(active_state->audio_freq1);
			f2 = &(active_state->audio_freq2);
			fmv1 = &(active_state->audio_fmvals1);
			fmv2 = &(active_state->audio_fmvals2);
			pn1 = &(active_state->audio_pan1);
			pn2 = &(active_state->audio_pan2);
			a1 = &(active_state->audio_amp1);
			a2 = &(active_state->audio_amp2);
			ns = &(active_state->audio_noisefactor);
			smp = &(active_state->audio_granSample);
			//int selected = std::distance(samplebank.begin(), std::find(samplebank.begin(), samplebank.end(), smp));		//find the sample ptr in the samplebank, get the index
		}

		else {
			f1 = &freq1;
			f2 = &freq2;
			fmv1 = &fmvals1;
			fmv2 = &fmvals2;
			pn1 = &pan1;
			pn2 = &pan2;
			a1 = &amp1;
			a2 = &amp2;
			ns = &noisefactor;
			smp = &selectedSample;
		}

		//Draw Sliders
		ImGui::Indent();
		const float spacing = 4;
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
		if (active_state) {
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImColor::HSV((active_state->id_ % 10) * 0.1f, 0.5f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImColor::HSV((active_state->id_ % 10) * 0.1f, 0.6f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImColor::HSV((active_state->id_ % 10) * 0.1f, 0.7f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImColor::HSV((active_state->id_ % 10) * 0.1f, 0.9f, 0.9f));
		}

		ImGui::DragFloat("Freq 1", f1, 0.5f, 0.1f, 5000.f, "%.1f", 2.0f);
		ImGui::DragFloat2("FM1 Freq / Mod Index", *fmv1, 0.5f, 0.1f, 1000.f, "%.1f", 2.0f);
		ImGui::SliderFloat("Pan 1", pn1, -50.f, 50.f, "%.1f", 2.0f);
		ImGui::DragFloat("Amp 1", a1, 0.01f, 0.f, .99f, "%.3f", 1.0f);
		ImGui::Separator();
		ImGui::DragFloat("Freq 2", f2, 0.5f, 0.1f, 5000.f, "%.1f", 2.0f);
		ImGui::DragFloat2("FM2 Freq / Mod Index ", *fmv2, 0.5f, 0.1f, 1000.f, "%.1f", 2.0f);
		ImGui::SliderFloat("Pan 2", pn2, -50.f, 50.f, "%.1f", 2.0f);
		ImGui::DragFloat("Amp 2", a2, 0.01f, 0.f, .99f, "%.3f", 1.0f);
		ImGui::Separator();
		ImGui::DragFloat("Noise Factor", ns, 0.01f, 0.f, .99f, "%.3f", 1.0f);

		ImGui::Separator();

		//DO SAMPLE LOAD, get Path, check 16bit wav
		if (ImGui::Button("Load Sample")) {
			ofFileDialogResult res = ofSystemLoadDialog("Load Wav File");
			if (res.bSuccess) {
				path p = res.getPath();
				cout << p.extension();
				std::string ext = p.extension().string();
				if (ext.find("wav") != std::string::npos) {
					cout << "loading sample..." << endl;
					maxiSample* smpl = new maxiSample();
					if ((smpl->load(p.string(), 0))) {
						if (smpl->myBitsPerSample == 16) {
							cout << smpl->getSummary() << endl;
							smpl->normalise();
							samplebank.push_back(smpl);
							samplebank_paths.push_back(p);
						}
						else {
							cout << "...bitrate not supported!" << endl;
						}
					}
					else {
						cout << "filetype not supported" << endl;
					}
				}
			}
		}
		ImGui::Columns(4, "Samplebank");
		ImGui::Separator();
		ImGui::Text("ID/Name"); ImGui::NextColumn();
		ImGui::Text("Bitrate"); ImGui::NextColumn();
		ImGui::Text("Length(s)"); ImGui::NextColumn();
		ImGui::Text("Path"); ImGui::NextColumn();
		ImGui::Separator();
		int i = 0;
		for (auto sample: samplebank)	{
			char label[32];
			sprintf(label, "%02d ", i);
			std::string name = samplebank_paths[i].filename().string();
			std::string l = label + name;

			if (ImGui::Selectable(l.c_str(), (*smp) == sample, ImGuiSelectableFlags_SpanAllColumns)) {
					selectedSample = sample;
					(*smp) = sample;
			}
			ImGui::NextColumn();
			std::string bc = (std::to_string(sample->myBitsPerSample));
			std::string length = std::to_string((float)sample->length / ((float)sample->mySampleRate));
			std::string path = samplebank_paths[i].stem().string();
			ImGui::Text(bc.c_str()); ImGui::NextColumn();
			ImGui::Text(length.c_str()); ImGui::NextColumn();
			ImGui::Text(path.c_str()); ImGui::NextColumn();
			i++;
		}
		ImGui::Columns(1);
		ImGui::Separator();

		ImGui::PopStyleVar();
		if (active_state) {
			ImGui::PopStyleColor(4);
		}
	}

	ImGui::End();
	this->gui.end();
	return ImGui::IsMouseHoveringAnyWindow();
}

//--------------------------------------------------------------
void ofApp::doStateInterp() {
	float sim_tot = 0;
	//do temp variables to pass to sound vars later
	float f1 = 0, f2 = 0, fmv1[2] = { 0, 0 }, fmv2[2] = { 0, 0 }, mi1 = 0, mi2 = 0, a1 = 0, a2 = 0, ns = 0;

	for (auto state : states) {
		state->similarity = 0;
		state->weight = 0;
		state->knn = 0;
	}

	int num_results = state_labels.size();
	vector<size_t>   ret_indexes(num_results);
	vector<float> out_dists_sqr(num_results);
	vector<float> query_pt;
	query_pt.resize(9);

	for (int i = 0; i < 9; i++) {
		query_pt[i] = current_state(0, i);
	}

	nanoflann::KNNResultSet<float> resultSet(num_results);
	resultSet.init(&ret_indexes[0], &out_dists_sqr[0]);
	state_index->index->findNeighbors(resultSet, &query_pt[0], nanoflann::SearchParams(10));
	float total_weight = 0;

	//results & weighted KNN
	for (size_t i = 0; i < num_results; i++) {
		int label_indx = state_labels[ret_indexes[i]];
		float w = 1.f / max(min(out_dists_sqr[i], 10000.f), 0.0001f);
		auto it = std::find_if(states.begin(), states.end(), [label_indx](audioState *st)->bool { return st->id_ == label_indx; });		//find the unique audio state matching each result id and update weight
		(*it)->weight += w;					
		total_weight += w;
		(*it)->knn += 1;
	}

	//interpolate 
	for (auto state : states) {
		if (state->knn > 0) {
			state->similarity = ((float)state->weight / max(total_weight, 0.001f));
			f1 += state->audio_freq1 * state->similarity;
			f2 += state->audio_freq2 * state->similarity;
			fmv1[0] += state->audio_fmvals1[0] * state->similarity;
			fmv1[1] += state->audio_fmvals1[1] * state->similarity;
			fmv2[0] += state->audio_fmvals2[0] * state->similarity;
			fmv2[1] += state->audio_fmvals2[1] * state->similarity;
			mi1 += state->audio_pan1 * state->similarity;
			mi2 += state->audio_pan2 * state->similarity;
			a1 += state->audio_amp1 * state->similarity;
			a2 += state->audio_amp2 * state->similarity;
			ns += state->audio_noisefactor * state->similarity;
		}
	}

	freq1 = f1;
	freq2 = f2;
	pan1 = mi1;
	pan2 = mi2;
	amp1 = min(a1, 0.99f);
	amp2 = min(a2, 0.99f);
	noisefactor = ns;
	fmvals1[0] = fmv1[0];
	fmvals1[1] = fmv1[1];
	fmvals2[0] = fmv2[0];
	fmvals2[1] = fmv2[1];
}

//--------------------------------------------------------------
void ofApp::audioOut(float * output, int bufferSize, int nChannels, int deviceID, long unsigned long tickCount) {
	if (j + bufferSize >= nPoints)
	{
		j = 0;
	}

	float bL = 0;
	float bR = 0;



	for (int i = 0; i < bufferSize; i++) {
		float pan = 0;
		if (buf) {
			if (!isnan(buf[j * 2].y)) {			
				float val = max(min((buf[j * 2].y + buf[j * 2].z) / 20.f, 1.0f), -1.0f);		//buf is our gfx card buffer
				pan = max(min(buf[j * 2].x, 5.0f), -5.0f);
				bL = val * (1 - ((pan + 5.f) / 10));
				bR = val * ((pan + 5.f) / 10);
				if (grainplayer->grains.size() < nGrains) {
					float speed = max(min(buf[(j * 2) + 1].w * abs(dt), 1.f), 0.001f);		//gets indiv particle speed from clr vector.w
					float duration1 = min(max(eigenval4 * 0.01f, 0.001f), 0.35f);
					float duration2 = min(max(eigenval3 * 0.5f, 0.001f), 0.35f);
					if (playing) { //if playing perform a weighted random selection of sample from states, given their similarity ratings
						auto state_it = states.begin();
						float running_total = 0;
						selected = false;
						float rnd = rand() * (1.0f / RAND_MAX);	//random number from 0 to total-weight
						while (!selected && state_it != states.end()) {
							running_total += (*state_it)->similarity;
							if (running_total >= rnd) {		
								selectedSample = (*state_it)->audio_granSample;
								selected = true;
							}
							state_it++;
						}
					}
					maxiStereoGrain<hannWinFunctor, maxiSample> *grain = new maxiStereoGrain<hannWinFunctor, maxiSample>(selectedSample, (val + 1) / 2, duration1, speed, pan, windowCache);
					grainplayer->addGrain(grain);
				}
			}
		}

		float p1 = (pan1 + 50) * 0.5;
		float p2 = (pan2 + 50) * 0.5;
		float o1 = osc1.sinewave((freq1 * (bL * noisefactor * 10 + 1)) + fmvals1[1] * fm1.sinewave(fmvals1[0])) * 0.25 * amp1;
		float o2 = osc2.sinewave((freq2 * (bL * noisefactor * 10 + 1)) + fmvals2[1] * fm2.sinewave(fmvals2[0])) * 0.25 * amp2;

		doublearray g1;
		if (!grainplayer->grains.empty()) {
			g1 = grainplayer->play();
		}

		myoutput[0] = ((bL * noisefactor * 0.1) + (o1*(1-p1) + o2*(1-p2)) * 0.5 + (g1[0] * 0.1));
		myoutput[1] = ((bR * noisefactor * 0.1) + (o1*p1 + o2*p2) * 0.5 + (g1[1] * 0.1));
		j++;
		output[i * nChannels] = myoutput[0];
		output[i * nChannels + 1] = myoutput[1];
	}
}


//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
}

//--------------------------------------------------------------
void ofApp::exit() {
	cout << "closing and freeing data...." << endl;
	soundstream.close();

	for (auto grain : grainplayer->grains) {
		delete grain;
	}
	for (auto sample : samplebank) {
		delete sample;
	}
	for (auto state : states) {
		delete state;
	}

	delete state_index;
	grainplayer, windowCache;
	ofSleepMillis(20);
}

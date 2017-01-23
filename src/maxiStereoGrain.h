#pragma once
#include "ofApp.h"
#include "maximilian.h"

//Customised maxiGrain class to output stereo grains

typedef std::array<double, 2> doublearray;

class maxiStereoGrainBase {
public:
	virtual doublearray play() = 0;
	virtual ~maxiStereoGrainBase() {};
	bool finished;
};

template<class F, class maxiSample>
class maxiStereoGrain : public maxiStereoGrainBase {
public:
	maxiSample *sample;
	double pos;
	double dur;
	double pan;	//--added pan member
	long sampleStartPos;
	long sampleIdx;
	long sampleDur;
	long sampleEndPos;
	double freq;
	double speed;
	double inc;
	double frequency;
	double* window;
	/* position between 0.0 and 1.0
	duration in seconds */
	maxiStereoGrain(maxiSample *_sample, const double position, const double duration, const double speed, const double pan, 
	maxiGrainWindowCache<F> *windowCache) :sample(_sample), pos(position), dur(duration), speed(speed), pan(pan) {
		sampleStartPos = (sample->length) * pos;
		sampleDur = dur * (double)maxiSettings::sampleRate;
		sampleDurMinusOne = sampleDur - 1;
		sampleIdx = 0;
		finished = 0;
		freq = 1.0 / dur;
		sampleEndPos = min(sample->length, sampleStartPos + sampleDur);
		frequency = freq * speed;
		if (frequency > 0) {
			pos = sampleStartPos;
		}
		else {
			pos = sampleEndPos;
		}
		if (frequency != 0) {
			inc = sampleDur / (maxiSettings::sampleRate / frequency);
		}
		else {
			inc = 0;
		}
		window = windowCache->getWindow(sampleDur);
	}

	~maxiStereoGrain() {
	}

	inline doublearray play() {
		doublearray output;
		if (!finished) {
			envValue = window[sampleIdx];
			double remainder;
			pos += inc;
			if (pos >= sample->length)
				pos -= sample->length;
			else if (pos < 0)
				pos += sample->length;

			long posl = floor(pos);
			remainder = pos - posl;
			long a = posl;
			long b = posl + 1;
			if (b >= sample->length)
				b = 0;

			double val = (double)((1 - remainder) * sample->temp[a] + remainder * sample->temp[b]) / 32767.0;//linear interpolation
			//cout << "val: " << val << " remainder " << remainder << " length " << sample->length << " temp: " << sample->temp[a] << endl;
			output[0] = (val * (1 - pan)) * envValue; //linear pan
			output[1] = (val * (pan)) * envValue;
		}

		sampleIdx++;
		if (sampleIdx == sampleDur) finished = true;
		return output;
	}

protected:
	maxiStereoGrain();
	double envValue;
	ulong sampleDurMinusOne;
};

typedef std::list<maxiStereoGrainBase*> stGrainList;
class maxiStereoGrainPlayer {
public:
	stGrainList grains;

	void addGrain(maxiStereoGrainBase *g) {
		grains.push_back(g);
	}

	inline doublearray play() {
		doublearray total = { 0.0, 0.0 };
		stGrainList::iterator it = grains.begin();
		while (it != grains.end()) {
			doublearray values = (*it)->play();
			total[0] += values[0];
			total[1] += values[1];
			//std::cout << "total:" << total[0] << endl;
			if ((*it)->finished) {
				delete(*it);
				it = grains.erase(it);
			}
			else {
				it++;
			}
		}
		return total;
	}
};
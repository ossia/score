//Parameters for harpsichord.dsp
//©Romain Michon (rmichon@ccrma.stanford.edu), 2011
//licence: FAUST-STK

#include "instrument.h"

#define _LOOKUP_TABLE_H_

double DryTapAmpT60harpsichord_points[16*2] = {
//  NoteNumber(A440=69), T60(sec)
	21.001,0.491,
	26.587,0.498,
	34.249,0.470,
	40.794,0.441,
	47.977,0.392,
	55.000,0.370,
	60.000,0.370,
	66.000,0.370,
	71.934,0.370,
	78.000,0.370,
	83.936,0.390,
	88.557,0.387, 
	92.858,0.400,
	97.319,0.469,
	102.400,0.500,
	107.198,0.494	};
extern LookupTable DryTapAmpT60harpsichord;
LookupTable DryTapAmpT60harpsichord(&DryTapAmpT60harpsichord_points[0], 16);
STK_EXPORT double getValueDryTapAmpT60harpsichord (double index){
	return DryTapAmpT60harpsichord.getValue(index);
}

double releaseLoopGainharpsichord_points[10*2] = {
	21.000,0.865,
	24.000,0.880,
	29.000,0.896,
	36.000,0.910,
	48.000,0.920,
	60.000,0.950,
	72.000,0.965,
	84.000,0.988,
	88.000,0.997,
	99.000,0.988	};
extern LookupTable releaseLoopGainharpsichord;
LookupTable releaseLoopGainharpsichord(&releaseLoopGainharpsichord_points[0], 10);

STK_EXPORT double getValueReleaseLoopGainharpsichord(double index){
	return releaseLoopGainharpsichord.getValue(index);
}

double loopFilterb0harpsichord_points[18*2] = {
	35.000,0.94373,
	36.000,0.94731,
	46.000,0.94731,
	47.000,0.96202,
	52.000,0.96202,
	53.000,0.97477,
	58.000,0.97477,
	59.000,0.97733,
	64.000,0.97733,
	65.000,0.97971,
	70.000,0.97971,
	71.000,0.97971,
	76.000,0.97971,
	77.000,0.98698,
	82.000,0.98698,
	83.000,0.98462,
	86.000,0.98462,
	87.000,0.98611,
};
extern LookupTable loopFilterb0harpsichord;
LookupTable loopFilterb0harpsichord(&loopFilterb0harpsichord_points[0], 18);

STK_EXPORT double getValueLoopFilterb0harpsichord(double index){
	return loopFilterb0harpsichord.getValue(index);
}

double loopFilterb1harpsichord_points[18*2] = {
	35.000,0.60010,
	36.000,-0.59124,
	46.000,-0.59124,
	47.000,-0.21243,
	52.000,-0.21243,
	53.000,-0.39280,
	58.000,-0.39280,
	59.000,-0.48307,
	64.000,-0.48307,
	65.000,0.51965,
	70.000,0.51965,
	71.000,0.51965,
	76.000,0.51965,
	77.000,-0.42463,
	82.000,-0.42463,
	83.000,0.85655,
	86.000,0.85655,
	87.000,0.68851,
};
extern LookupTable loopFilterb1harpsichord;
LookupTable loopFilterb1harpsichord(&loopFilterb1harpsichord_points[0], 18);

STK_EXPORT double getValueLoopFilterb1harpsichord(double index){
	return loopFilterb1harpsichord.getValue(index);
}

double loopFilterb2harpsichord_points[18*2] = {
	35.000,-0.00360,
	36.000,-0.12249,
	46.000,-0.12249,
	47.000,-0.16044,
	52.000,-0.16044,
	53.000,-0.21680,
	58.000,-0.21680,
	59.000,-0.16346,
	64.000,-0.16346,
	65.000,0.22162,
	70.000,0.22162,
	71.000,0.22162,
	76.000,0.22162,
	77.000,-0.14973,
	82.000,-0.14973,
	83.000,0.24937,
	86.000,0.24937,
	87.000,0.14838,
};
extern LookupTable loopFilterb2harpsichord;
LookupTable loopFilterb2harpsichord(&loopFilterb2harpsichord_points[0], 18);

STK_EXPORT double getValueLoopFilterb2harpsichord(double index){
	return loopFilterb2harpsichord.getValue(index);
}

double loopFiltera1harpsichord_points[18*2] = {
	35.000,0.5941,
	36.000,-0.65928,
	46.000,-0.65928,
	47.000,-0.24222,
	52.000,-0.24222,
	53.000,-0.41402,
	58.000,-0.41402,
	59.000,-0.50837,
	64.000,-0.50837,
	65.000,0.51263,
	70.000,0.51263,
	71.000,0.51263,
	76.000,0.51263,
	77.000,-0.43976,
	82.000,-0.43976,
	83.000,0.85396,
	86.000,0.85396,
	87.000,0.68332,
};
extern LookupTable loopFiltera1harpsichord;
LookupTable loopFiltera1harpsichord(&loopFiltera1harpsichord_points[0], 18);

STK_EXPORT double getValueLoopFiltera1harpsichord(double index){
	return loopFiltera1harpsichord.getValue(index);
}

double loopFiltera2harpsichord_points[18*2] = {
	35.000,-0.02641,
	36.000,-0.10275,
	46.000,-0.10275,
	47.000,-0.15842,
	52.000,-0.15842,
	53.000,-0.21653,
	58.000,-0.21653,
	59.000,-0.15833,
	64.000,-0.15833,
	65.000,0.22025,
	70.000,0.22025,
	71.000,0.22025,
	76.000,0.22025,
	77.000,-0.14583,
	82.000,-0.14583,
	83.000,0.24405,
	86.000,0.24405,
	87.000,0.14370,
};
extern LookupTable loopFiltera2harpsichord;
LookupTable loopFiltera2harpsichord(&loopFiltera2harpsichord_points[0], 18);

STK_EXPORT double getValueLoopFiltera2harpsichord(double index){
	return loopFiltera2harpsichord.getValue(index);
}

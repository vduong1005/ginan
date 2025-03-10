
#pragma GCC optimize ("O0")



#include <iostream>
#include <random>

#include "eigenIncluder.hpp"
// #include "common.hpp"

#include "minimumConstraints.hpp"
#include "rtsSmoothing.hpp"
#include "observations.hpp"
#include "algebraTrace.hpp"
#include "linearCombo.hpp"
#include "navigation.hpp"
#include "acsConfig.hpp"
#include "station.hpp"
#include "algebra.hpp"
// #include "common.hpp"
#include "debug.hpp"
#include "sinex.hpp"
#include "trace.hpp"
#include "enums.h"

std::random_device					randoDev;
std::mt19937						randoGen(randoDev());
std::normal_distribution<double>	rando(0, 15);

void minimumTest(
	Trace&					trace)
{
	GTime gtime;
	gtime++;
	trace << std::endl;
	map<string, Vector3dInit> pointMap;
	Vector3d p;

	pointMap["STN0"] = {+1,	0,	0};
	pointMap["STN1"] = {-1, 	0,	0};
	pointMap["STN2"] = {0, 	+1,	0};
	pointMap["STN3"] = {0, 	-1,	0};
	pointMap["STN4"] = {0,		0,		+1};
	pointMap["STN5"] = {0,		0,		-1};
// 	pointMap["STN6"] = {1/sqrt(3),		1/sqrt(3),		1/sqrt(3)};
// 	pointMap["DONT"] = {-1/sqrt(2),	-1/sqrt(2),		0};
// 
// 	pointMap["STN4"] *= 0.997;
// 	pointMap["STN5"] *= 0.998;
// 	pointMap["STN6"] *= 0.999;
	
	
	acsConfig.output_residuals = true;
	KFState kfStateStations;
	kfStateStations.output_residuals = true;

	map<string, Station> stationMap;
	
	
	//generate fake station data
	{
		KFMeasEntryList stationEntries;
		for (auto& [id, a] : pointMap)
		{
			a *= 5000000;// * sqrt(2);
// 			a += Vector3d(0.0000001,0.0000001,0.0000001);
			
			auto& rec = stationMap[id];
			rec.id = id;
// 			a += Vector3d{0,1000,0};

			for (int i = 0; i < 3; i++)
			{
				rec.snx.pos[i] = a[i];
				rec.aprioriVar(i) = 1;
			}

			double angz = 0.000000001 * 500/2063;
			Matrix3d rotz;
			rotz <<
			+cos(angz),	+sin(angz),	0,
			-sin(angz),	+cos(angz),	0,
			0,			0,			1;
			
			double angx = 0.000000003 * 400 / 6188;
			Matrix3d rotx;
			rotx <<
			1,			0,			0,			
			0,			+cos(angx),	+sin(angx),	
			0,			-sin(angx),	+cos(angx);	
			
			rec.minconApriori = a;
			Vector3d p = a;
			p = rotz * p;
			p = rotx * p;
			p += Vector3d{0.001,0.0025,0.003};
// 			p = p * 1.0000001;
// 			trace << "\t" << p(0) << "\t" << p(1) << "\t" << p(2) << std::endl;
			
			rec.aprioriPos = a;
			
			for (int i = 0; i < 3; i++)
			{
				KFMeasEntry meas(&kfStateStations);
			
				KFKey kfKey;
				kfKey.type		= KF::REC_POS;
				kfKey.str		= id;
				kfKey.rec_ptr	= &rec;
				kfKey.num		= i;
				
				meas.addDsgnEntry(kfKey,	1);
				
				double val = p(i);
// 				val +=  stationEntries.size()/1000.0;
				meas.setValue(val);
			
// 				if (stationEntries.size() < 3)
// 					meas.setNoise(5000);
// 				else
					meas.setNoise(100);
					
			
				stationEntries.push_back(meas);
			}
		}

		KFMeasEntry dummyMeas(&kfStateStations);
		dummyMeas.setValue(1);
		dummyMeas.setNoise(100);
// 		dummyMeas.addDsgnEntry({KF::IONOSPHERIC, 	{}, "1", 3},	1);
// 		dummyMeas.addDsgnEntry({KF::REC_POS,		{}, "2", 2},	1);
// 		stationEntries.push_back(dummyMeas);


		//add process noise to existing states as per their initialisations.
		kfStateStations.stateTransition(trace, gtime);

		KFMeas combinedMeas = kfStateStations.combineKFMeasList(stationEntries);

		/* network parameter estimation */
		if (kfStateStations.lsqRequired)
		{
			kfStateStations.lsqRequired = false;
			trace << std::endl << "-------DOING LEAST SQUARES--------";
			kfStateStations.leastSquareInitStates(trace, combinedMeas);
		}
	}

	kfStateStations.P(3,3) = 400;
	
// 	kfStateStations.P(10,13) = 30;
// 	kfStateStations.P(13,10) = 30;
// 	kfStateStations.P(1,9) = 30;
// 	kfStateStations.P(9,1) = 30;
	kfStateStations.outputStates(trace);
	sinexPostProcessing(kfStateStations.time, stationMap, kfStateStations);
	
// 	getStationsFromSinex(kfStateStations, stationMap);
	
	mincon(trace, kfStateStations);
	
	for (auto [kfKey, index] : kfStateStations.kfIndexMap)
	{
		if (kfKey.type != KF::REC_POS)
		{
			continue;
		}
		if (kfKey.num != 0)
		{
			continue;
		}
		
		Vector3d point1 = kfStateStations.x.segment(index, 3) + pointMap["STN" + std::to_string(index/3)];
		
		for (auto [kfKey, index] : kfStateStations.kfIndexMap)
		{
			if (kfKey.type != KF::REC_POS)
			{
				continue;
			}
			if (kfKey.num != 0)
			{
				continue;
			}
			
			Vector3d point2 = kfStateStations.x.segment(index, 3) + pointMap["STN" + std::to_string(index/3)];
			
			std::cout << std::endl << "Distance:  " << ((point1-point2).norm() - 10000000);
		}
	}
	kfStateStations.outputStates(trace);
}

#if 0
#include "sinex.hpp"
#if 0
void outputMeas(
			Trace&		trace,   	///< Trace to output to
			GTime 		time,
			KFMeas		meas)
{
	trace << std::endl << "+MEAS" << std::endl;
	
	tracepdeex(2, trace, "#\t%19s\t%15s\t%15s\t%15s\n", "Time", "Observed", "Meas Noise", "Design Mat");

	for (int i = 0; i < meas.H.rows(); i++)
	{
		tracepdeex(2, trace, "*\t%19s\t%15.4f\t%15.9f", time.to_string(0).c_str(), meas.Y(i), meas.R(i, i));

		for (int j = 0; j < meas.H.cols(); j++)		tracepdeex(2, trace, "\t%15.5f", meas.H(i, j));
		tracepdeex(2, trace, "\n");
	}
	trace << "-MEAS" << std::endl;
}
#endif
// modified from testClockParams() to test chi^2 computation
// std::random_device					randoDev;
// std::mt19937						randoGen(randoDev());
// std::normal_distribution<double>	rando(0, 8);

#if 0
void testOutlierDetection()
{
	GTime gtime;
	gtime++;
	KFState kfState;

	kfState.sigma_check		= false;
	kfState.w_test			= true;
	kfState.chi_square_test	= true;
	kfState.chi_square_mode	= E_ChiSqMode::INNOVATION;
	kfState.sigma_threshold	= 3;
	kfState.max_prefit_remv = 1;

	for (int i = 0; i < 100; i++)
	{
		KFMeasEntryList kfMeasEntryList0;

		for (int j = 0; j < 2; j++)
		{
			KFMeasEntry	codeMeas(&kfState);
			if (i%30 == 20 && j == 1)	codeMeas.setValue(i + rando(randoDev) + 60);	//inject an outlier of 60m
			else						codeMeas.setValue(i + rando(randoDev));
			codeMeas.setNoise(100);

			KFKey recClockKey		= {KF::REC_SYS_BIAS};
			KFKey recClockRateKey	= {KF::REC_SYS_BIAS_RATE};

			InitialState recClkInit		= {0,	SQR(30),	SQR(1)};
			InitialState recClkRateInit	= {0,	SQR(20),	SQR(0.01)};

			codeMeas.addDsgnEntry(recClockKey,						+1,	recClkInit);
			kfState.setKFTransRate(recClockKey, recClockRateKey,	+1,	recClkRateInit);

			kfMeasEntryList0.push_back(codeMeas);
		}

		//inject model failures
		{
			KFKey recClockKey	= {KF::REC_SYS_BIAS};
			if (i == 30)	kfState.setKFValue(recClockKey, 80);
			if (i == 90)	kfState.setKFValue(recClockKey, 50);
		}

		kfState.stateTransition(std::cout, gtime++);

// 		kfState.consolidateKFState();

		KFMeas combinedMeas = kfState.combineKFMeasList(kfMeasEntryList0);

		std::cout << std::endl << "Epoch #: " << i << std::endl;

		std::cout << std::endl << "Pre-fit:" << std::endl;
		kfState.outputStates(std::cout);
// 		outputMeas(std::cout, gtime, combinedMeas);

		kfState.filterKalman(std::cout, combinedMeas, false);

		std::cout << std::endl << "Post-fit:" << std::endl;
		kfState.outputStates(std::cout);
	}
//	exit(0);
}
#endif


#include "acsConfig.hpp"
#include "mongoWrite.hpp"

void rtsBump()
{
	KFState kfState;
	kfState.rts_basename = "therfe";
	kfState.rts_lag = -1;
	
	InitialState sinInit;
	sinInit.P = 10000;
	sinInit.Q = 1;
	
	InitialState ambInit;
	ambInit.P = 10000;
	
	GTime time;
	time += 60;
	
	for (int i = 0; i < 1000; i++)
	{
		double amb = 0;
		if (i < 10)			amb = 10;
		else 				amb = 0;
		
		double sine = sin(i/100.0);
		
		double cose = cos(i/100.0);
		
		KFMeasEntryList kfMeasEntryList;
	
		KFKey sinKey;
		KFKey ambKey;
		
		sinKey.type	= KF::TROP;
		ambKey.type	= KF::AMBIGUITY;
			
		{
			KFMeasEntry measEntry(&kfState, {0,{},"two"});
			
			measEntry.addDsgnEntry(sinKey,	1, sinInit);
			measEntry.addDsgnEntry(ambKey,	1, ambInit);
			
			measEntry.setValue(amb + sine);
			measEntry.setNoise(1);
			
			kfMeasEntryList.push_back(measEntry);
		}
		if (i > 100)
		{
			KFMeasEntry measEntry(&kfState, {0,{},"100"});
			
			measEntry.addDsgnEntry(sinKey,	1, sinInit);
			
			measEntry.setValue(sine);
			measEntry.setNoise(400);
			
			kfMeasEntryList.push_back(measEntry);
		}
		
// 		if (i > 100)
		{
			KFMeasEntry measEntry(&kfState, {0,{},"sin"});
			sinKey.num = 1;
			measEntry.addDsgnEntry(sinKey,	1, sinInit);
			
			measEntry.setValue(sine);
			measEntry.setNoise(400);
			
			kfMeasEntryList.push_back(measEntry);
		}
		
		kfState.output_residuals = true;
		
		kfState.stateTransition(std::cout, time);
		
		kfState.outputStates(std::cout);
	
		KFMeas combinedMeas = kfState.combineKFMeasList(kfMeasEntryList, time);
		
		kfState.filterKalman(std::cout, combinedMeas);
		
		kfState.outputStates(std::cout);

		if (acsConfig.output_mongo_states)
		{
			mongoStates(kfState);
		}
		
		time++;
	}	
		
	RTS_Process(kfState);
}

void dualFilters()
{
	vector<double> positions;
	
	KFState kfState;
	kfState.rts_basename = "therfe";
	kfState.rts_lag = -1;
	
	InitialState posInit;
	posInit.P = 100000000;
	
	InitialState velInit;
	velInit.P = 100000000;
	velInit.Q = 0.1;
	GTime time;
	time += 60;
	
	for (int i = 0; i < 20; i++)
	{
		double position = 0;
		if (i < 10)			position = i * 1.0 + 5;
		else 				position = i * 0.3 + 6;
		
		positions.push_back(position);
		
		KFMeasEntryList kfMeasEntryList;
		
		for (int j = 0; j < 3; j++)
		{
			KFKey posKey;
			KFKey velKey;
			
			posKey.type	= KF::REC_POS;
			velKey.type	= KF::REC_POS_RATE;
			
			posKey.num	= j;
			velKey.num	= j;
			
			KFMeasEntry measEntry(&kfState);
			
			measEntry.addDsgnEntry(posKey,			1, posInit);
			
			kfState.setKFTransRate(posKey, velKey,	1, velInit);
			
			if (j == 1 && i >= 10)	continue;
			if (j == 2 && i <  10)	continue;
			
			measEntry.setValue(position);
			measEntry.setNoise(4);
			
			kfMeasEntryList.push_back(measEntry);
		}
		
		kfState.output_residuals = true;
		
		kfState.stateTransition(std::cout, time);
		
		kfState.outputStates(std::cout);
	
		KFMeas combinedMeas = kfState.combineKFMeasList(kfMeasEntryList, time);
		
		kfState.filterKalman(std::cout, combinedMeas);
		
		kfState.outputStates(std::cout);

		if (acsConfig.output_mongo_states)
		{
			mongoStates(kfState);
		}
		
		time++;
	}	
		
	RTS_Process(kfState);
}



#include "streamSerial.hpp"
#include "streamFile.hpp"
#include "streamRtcm.hpp"
#include "streamObs.hpp"
#include "streamUbx.hpp"

#endif

#include "mongoWrite.hpp"


//#include "orbitProp.hpp"
//extern map<string, OrbitPropagator>	orbitPropagatorMap;

/** Compare the orbital states created by pseudo-linear state transitions with the original values.
 * The pseudo-linear state transition in the filter (STM + adjustment) is mathematically equivalent to setting a state value directly, 
 * but numerical precision in a computer does not allow 100% correspondence - this checks its mostly working
// */
//void checkOrbits(
//	Trace&				trace,
//	KFState&			kfState)
//{
//
//
//	//get current inertial states from the kfState
//	for (auto& [kfKey, index] : kfState.kfIndexMap)
//	{
//		if	( kfKey.type	!= KF::ORBIT
//			||kfKey.num		!= 0)
//		{
//			continue;
//		}
//
//		auto id = kfKey.str;
//
//		auto& orbitPropagator = orbitPropagatorMap[id];
//
//		KFState subState = getOrbitFromState(trace, id, kfState);
//
//		Vector6d inertialState;
//		inertialState << orbitPropagator.states.pos, orbitPropagator.states.vel;
//
//		Vector6d deltaState	= inertialState
//							- subState.x.head(6);
//
//		trace << "\norbitPropagator.base.inertialState"		<< inertialState		.transpose().format(HeavyFmt);
//		trace << "\nsubState.x.head(6)                "		<< subState.x.head(6)	.transpose().format(HeavyFmt);
//		trace << "\ndeltaState                        "		<< deltaState			.transpose().format(HeavyFmt);
//
// 		MatrixXd transition = orbitPropagator.states.posVelSTM;
// 
// 		//Convert the absolute transition matrix to an identity matrix (already populated elsewhere) and stm-per-time matrix
// 		transition -= MatrixXd::Identity(transition.rows(), transition.cols());
// 
// 		MatrixXd thingy = transition * subState.P * transition.transpose();
// 
// 	// 				std::cout << "\ntransition\n" << transition << "\n";
// 	// 				std::cout << "\nthingy\n" << thingy << "\n";
// // 		if (0)
// 		for (auto& [key1, index1] : subState.kfIndexMap)
// 		for (auto& [key2, index2] : subState.kfIndexMap)
// 		{
// 			int index1a = kfState.kfIndexMap[key1];
// 			int index2a = kfState.kfIndexMap[key2];
// 			
// 			kfState.P(index1a, index2a) += thingy(index1, index2);
// 	// 				kfState.addKFState(key, init);
// 		}
//	}
	
	
// 	MatrixXd transition = orbitPropagator.states.posVelSTM;
// 
// 	//Convert the absolute transition matrix to an identity matrix (already populated elsewhere) and stm-per-time matrix
// 	transition -= MatrixXd::Identity(transition.rows(), transition.cols());
// 	transition /= 900;
// 
// 	MatrixXd thingy = transition * subState.P * transition.transpose();
// 
// 		std::cout << "\ntransition\n" << transition << "\n";
// 		std::cout << "\nthingy\n" << thingy << "\n";
// 	for (auto& [key1, index1] : subState.kfIndexMap)
// 	for (auto& [key2, index2] : subState.kfIndexMap)
// 	{
// 		int index1a = kfState.kfIndexMap[key1];
// 		int index2a = kfState.kfIndexMap[key2];
// 		
// 		kfState.P(index1a, index2a) += thingy(index1, index2);
// // 				kfState.addKFState(key, init);
// 	}
//
//}

// 			//apply required state based accelerations to the matrices before the state transitions
// 			if (0)
// 			for (auto& [accKey, subIndex] : subState.kfIndexMap)
// 			{
// 				Vector3d acceleration;
// 				
// 				switch (accKey.type)
// 				{
// 					default:
// 					{
// 						//nothing to be done except for state types specified below
// 						continue;
// 					}
// 					case KF::SRP_SCALE:		{	acceleration = outputsMap["accSrp"];	break;	}	//this should be whatever was used in the propagator above, for this srp component only
// 				}
// 				
// 				std::cout << std::endl << "add acceleration: " << acceleration.transpose() << std::endl;
// 				//add state transition elements for each component
// 				for (int i = 0; i < 3; i++)
// 				{
// 					KFKey posKey	= kfKey;
// 					posKey.num		= i;
// 					
// 					KFKey velKey	= posKey;
// 					velKey.type		= KF::ORBIT;
// 					velKey.num		= i + 3;
// 					
// 					kfState	.setAccelerator(posKey, velKey, accKey, acceleration(i));
// 					subState.setAccelerator(posKey, velKey, accKey, acceleration(i));
// 				}
// 			}


void timecheck()
{
	PTime	now		= timeGet();
	GTime	gNow	= now;
	UtcTime uNow	= gNow;
	
	std::cout << "now:   " << now.	bigTime << std::endl;
	std::cout << "gNow:  " << gNow.	bigTime << std::endl;
	std::cout << "uNow:  " << uNow.	bigTime << std::endl;
}

void debugTime()
{
	auto	utcNow	= boost::posix_time::microsec_clock::universal_time();
	GTime	gTime	= timeGet();

	std::cout << std::setprecision(1) << std::fixed << std::endl;
	std::cout << "universal_time():   " << utcNow << std::endl;
	std::cout << "timeGet():          " << gTime.bigTime << " " << gTime.to_string(1) << std::endl;

	PTime	pTime	= gTime;
	auto	bTime	= boost::posix_time::from_time_t((time_t)pTime.bigTime);
	UtcTime utcTime	= gTime;
	GEpoch	gEpoch	= gTime;
	UYds	uYds	= gTime;

	GWeek	gWeek	= gTime;
	GTow	gTow	= gTime;

	BWeek	bWeek	= gTime;
	BTow	bTow	= gTime;

	RTod	rTod	= gTime;

	std::cout << std::setfill('0') << std::endl;
	std::cout << "GTime to anything:"   << std::endl;
	std::cout << "GTime to PTime:     " << pTime  .bigTime << " " << bTime                << std::endl;
	std::cout << "GTime to UtcTime:   " << utcTime.bigTime << " " << utcTime.to_string(1) << std::endl;
	std::cout << "GTime to GEpoch:    " << (int)gEpoch.year << "-" << std::setw(2) << (int)gEpoch.month << "-" << std::setw(2) << (int)gEpoch.day << " " << std::setw(2) << (int)gEpoch.hour << ":" << std::setw(2) << (int)gEpoch.min << ":" << std::setw(4) << gEpoch.sec << std::endl;
	std::cout << "GTime to UYds:      " << (int)uYds.year << " " << (int)uYds.doy << " " << uYds.sod << std::endl;
	std::cout << "GTime to GWeek:     " << gWeek.val << std::endl;
	std::cout << "GTime to GTow:      " << gTow. val << std::endl;
	std::cout << "GTime to BWeek:     " << bWeek.val << std::endl;
	std::cout << "GTime to BTow:      " << bTow. val << std::endl;
	std::cout << "GTime to RTod:      " << rTod. val << std::endl;


	GTime	gTimeP	= pTime;
	GTime	gTimeU	= utcTime;
	GTime	gTimeE	= gEpoch;
	GTime	gTimeY	= uYds;

	std::cout << std::endl;
	std::cout << "anything to GTime:"   << std::endl;
	std::cout << "PTime   to GTime:   " << gTimeP.bigTime << " " << gTimeP.to_string(1) << std::endl;
	std::cout << "UtcTime to GTime:   " << gTimeU.bigTime << " " << gTimeU.to_string(1) << std::endl;
	std::cout << "GEpoch  to GTime:   " << gTimeE.bigTime << " " << gTimeE.to_string(1) << std::endl;
	std::cout << "UYds    to GTime:   " << gTimeY.bigTime << " " << gTimeY.to_string(1) << std::endl;

	GTime	gTimeG	= GTime(gWeek, gTow);
	GTime	gTimeB	= GTime(bWeek, bTow);

	std::cout << "GTime(GWeek, GTow): " << gTimeG.bigTime << " " << gTimeG.to_string(1) << std::endl;
	std::cout << "GTime(BWeek, BTow): " << gTimeB.bigTime << " " << gTimeB.to_string(1) << std::endl;

	GTime	nearTime= timeGet();
			gTimeG	= GTime(gTow, nearTime);
			gTimeB	= GTime(bTow, nearTime);
	GTime	gTimeR	= GTime(rTod, nearTime);

	std::cout << "GTime(GTow, GTime): " << gTimeG.bigTime << " " << gTimeG.to_string(1) << std::endl;
	std::cout << "GTime(BTow, GTime): " << gTimeB.bigTime << " " << gTimeB.to_string(1) << std::endl;
	std::cout << "GTime(RTod, GTime): " << gTimeR.bigTime << " " << gTimeR.to_string(1) << std::endl;


	double ep[6];

	std::cout << std::endl;
	std::cout << "GTime to epoch to GTime:"   << std::endl;
	time2epoch(gTime, ep, E_TimeSys::GPST);						std::cout << "GTime to GPST     epoch: " << (int)ep[0] << "-" << std::setw(2) << (int)ep[1] << "-" << std::setw(2) << (int)ep[2] << " " << std::setw(2) << (int)ep[3] << ":" << std::setw(2) << (int)ep[4] << ":" << std::setw(4) << ep[5] << std::endl;
	GTime gTimeEpochGPS	= epoch2time(ep, E_TimeSys::GPST);		std::cout << "GPST     epoch to GTime: " << gTimeEpochGPS.to_string(1) << std::endl;
	time2epoch(gTime, ep, E_TimeSys::GLONASST);					std::cout << "GTime to GLONASST epoch: " << (int)ep[0] << "-" << std::setw(2) << (int)ep[1] << "-" << std::setw(2) << (int)ep[2] << " " << std::setw(2) << (int)ep[3] << ":" << std::setw(2) << (int)ep[4] << ":" << std::setw(4) << ep[5] << std::endl;
	GTime gTimeEpochGLO	= epoch2time(ep, E_TimeSys::GLONASST);	std::cout << "GLONASST epoch to GTime: " << gTimeEpochGLO.to_string(1) << std::endl;
	time2epoch(gTime, ep, E_TimeSys::GST);						std::cout << "GTime to GST      epoch: " << (int)ep[0] << "-" << std::setw(2) << (int)ep[1] << "-" << std::setw(2) << (int)ep[2] << " " << std::setw(2) << (int)ep[3] << ":" << std::setw(2) << (int)ep[4] << ":" << std::setw(4) << ep[5] << std::endl;
	GTime gTimeEpochGST	= epoch2time(ep, E_TimeSys::GST);		std::cout << "GST      epoch to GTime: " << gTimeEpochGST.to_string(1) << std::endl;
	time2epoch(gTime, ep, E_TimeSys::BDT);						std::cout << "GTime to BDT      epoch: " << (int)ep[0] << "-" << std::setw(2) << (int)ep[1] << "-" << std::setw(2) << (int)ep[2] << " " << std::setw(2) << (int)ep[3] << ":" << std::setw(2) << (int)ep[4] << ":" << std::setw(4) << ep[5] << std::endl;
	GTime gTimeEpochBDT	= epoch2time(ep, E_TimeSys::BDT);		std::cout << "BDT      epoch to GTime: " << gTimeEpochBDT.to_string(1) << std::endl;
	time2epoch(gTime, ep, E_TimeSys::QZSST);					std::cout << "GTime to QZSST    epoch: " << (int)ep[0] << "-" << std::setw(2) << (int)ep[1] << "-" << std::setw(2) << (int)ep[2] << " " << std::setw(2) << (int)ep[3] << ":" << std::setw(2) << (int)ep[4] << ":" << std::setw(4) << ep[5] << std::endl;
	GTime gTimeEpochQZS	= epoch2time(ep, E_TimeSys::QZSST);		std::cout << "QZSST    epoch to GTime: " << gTimeEpochQZS.to_string(1) << std::endl;
	time2epoch(gTime, ep, E_TimeSys::TAI);						std::cout << "GTime to TAI      epoch: " << (int)ep[0] << "-" << std::setw(2) << (int)ep[1] << "-" << std::setw(2) << (int)ep[2] << " " << std::setw(2) << (int)ep[3] << ":" << std::setw(2) << (int)ep[4] << ":" << std::setw(4) << ep[5] << std::endl;
	GTime gTimeEpochTAI	= epoch2time(ep, E_TimeSys::TAI);		std::cout << "TAI      epoch to GTime: " << gTimeEpochTAI.to_string(1) << std::endl;
	time2epoch(gTime, ep, E_TimeSys::UTC);						std::cout << "GTime to UTC      epoch: " << (int)ep[0] << "-" << std::setw(2) << (int)ep[1] << "-" << std::setw(2) << (int)ep[2] << " " << std::setw(2) << (int)ep[3] << ":" << std::setw(2) << (int)ep[4] << ":" << std::setw(4) << ep[5] << std::endl;
	GTime gTimeEpochUTC	= epoch2time(ep, E_TimeSys::UTC);		std::cout << "UTC      epoch to GTime: " << gTimeEpochUTC.to_string(1) << std::endl;


	double yds[6];

	std::cout << std::endl;
	std::cout << "GTime to yds to GTime:"   << std::endl;
	time2yds(gTime, yds, E_TimeSys::GPST);						std::cout << "GTime to GPST     yds: " << (int)yds[0] << " " << (int)yds[1] << " " << yds[2] << std::endl;
	GTime gTimeYdsGPS	= yds2time(yds, E_TimeSys::GPST);		std::cout << "GPST     yds to GTime: " << gTimeYdsGPS.to_string(1) << std::endl;
	time2yds(gTime, yds, E_TimeSys::GLONASST);					std::cout << "GTime to GLONASST yds: " << (int)yds[0] << " " << (int)yds[1] << " " << yds[2] << std::endl;
	GTime gTimeYdsGLO	= yds2time(yds, E_TimeSys::GLONASST);	std::cout << "GLONASST yds to GTime: " << gTimeYdsGLO.to_string(1) << std::endl;
	time2yds(gTime, yds, E_TimeSys::GST);						std::cout << "GTime to GST      yds: " << (int)yds[0] << " " << (int)yds[1] << " " << yds[2] << std::endl;
	GTime gTimeYdsGST	= yds2time(yds, E_TimeSys::GST);		std::cout << "GST      yds to GTime: " << gTimeYdsGST.to_string(1) << std::endl;
	time2yds(gTime, yds, E_TimeSys::BDT);						std::cout << "GTime to BDT      yds: " << (int)yds[0] << " " << (int)yds[1] << " " << yds[2] << std::endl;
	GTime gTimeYdsBDT	= yds2time(yds, E_TimeSys::BDT);		std::cout << "BDT      yds to GTime: " << gTimeYdsBDT.to_string(1) << std::endl;
	time2yds(gTime, yds, E_TimeSys::QZSST);						std::cout << "GTime to QZSST    yds: " << (int)yds[0] << " " << (int)yds[1] << " " << yds[2] << std::endl;
	GTime gTimeYdsQZS	= yds2time(yds, E_TimeSys::QZSST);		std::cout << "QZSST    yds to GTime: " << gTimeYdsQZS.to_string(1) << std::endl;
	time2yds(gTime, yds, E_TimeSys::TAI);						std::cout << "GTime to TAI      yds: " << (int)yds[0] << " " << (int)yds[1] << " " << yds[2] << std::endl;
	GTime gTimeYdsTAI	= yds2time(yds, E_TimeSys::TAI);		std::cout << "TAI      yds to GTime: " << gTimeYdsTAI.to_string(1) << std::endl;
	time2yds(gTime, yds, E_TimeSys::UTC);						std::cout << "GTime to UTC      yds: " << (int)yds[0] << " " << (int)yds[1] << " " << yds[2] << std::endl;
	GTime gTimeYdsUTC	= yds2time(yds, E_TimeSys::UTC);		std::cout << "UTC      yds to GTime: " << gTimeYdsUTC.to_string(1) << std::endl;


	GTow	nearGTow= 1;
			nearTime= GTime(gWeek, nearGTow);
			gTow	= 604800 - 1.1;
			gTimeG	= GTime(gTow, nearTime);

	std::cout << std::setfill(' ') << std::endl;
	std::cout << "Week/Day roll-over:" << std::endl;
	std::cout << "nearGTow: " << std::setw(8) << nearGTow.val << "\tnearTime:              " << nearTime.to_string(1) << std::endl;
	std::cout << "gTow:     " << std::setw(8) << gTow.    val << "\tGTime(gTow, nearTime): " << gTimeG.  to_string(1) << std::endl;

			nearGTow= 604800 - 1;
			nearTime= GTime(gWeek, nearGTow);
			gTow	= 1.1;
			gTimeG	= GTime(gTow, nearTime);

	std::cout << "nearGTow: " << std::setw(8) << nearGTow.val << "\tnearTime:              " << nearTime.to_string(1) << std::endl;
	std::cout << "gTow:     " << std::setw(8) << gTow.    val << "\tGTime(gTow, nearTime): " << gTimeG.  to_string(1) << std::endl;

	BTow	nearBTow= 1;
			nearTime= GTime(bWeek, nearBTow);
			bTow	= 604800 - 1.1;
			gTimeB	= GTime(bTow, nearTime);

	std::cout << "nearBTow: " << std::setw(8) << nearBTow.val << "\tnearTime:              " << nearTime.to_string(1) << std::endl;
	std::cout << "bTow:     " << std::setw(8) << bTow.    val << "\tGTime(bTow, nearTime): " << gTimeB.  to_string(1) << std::endl;

			nearBTow= 604800 - 1;
			nearTime= GTime(bWeek, nearBTow);
			bTow	= 1.1;
			gTimeB	= GTime(bTow, nearTime);

	std::cout << "nearBTow: " << std::setw(8) << nearBTow.val << "\tnearTime:              " << nearTime.to_string(1) << std::endl;
	std::cout << "bTow:     " << std::setw(8) << bTow.    val << "\tGTime(bTow, nearTime): " << gTimeB.  to_string(1) << std::endl;

	UYds	nearYds	= uYds;
			nearYds.sod	= 86400 - 10800 + 1;
			nearTime= nearYds;
	RTod	nearTod	= nearTime;
			rTod	= 86400 - 1.1;
			gTimeR	= GTime(rTod,  nearTime);

	std::cout << "nearSod:  " << std::setw(8) << nearYds. sod << "\tnearTime:              " << nearTime.to_string(1) << "\tnearTod:  " << nearTod. val << std::endl;
	std::cout << "rTod:     " << std::setw(8) << rTod.    val << "\tGTime(rTod, nearTime): " << gTimeR.  to_string(1) << std::endl;

			nearYds.sod	= 86400 - 10800 - 1;
			nearTime= nearYds;
			nearTod	= nearTime;
			rTod	= 1.1;
			gTimeR	= GTime(rTod,  nearTime);

	std::cout << "nearSod:  " << std::setw(8) << nearYds. sod << "\tnearTime:              " << nearTime.to_string(1) << "\tnearTod:  " << nearTod. val << std::endl;
	std::cout << "rTod:     " << std::setw(8) << rTod.    val << "\tGTime(rTod, nearTime): " << gTimeR.  to_string(1) << std::endl;

			gEpoch	= {2017, 1, 1, 0, 0, 17.9};
			gTimeE	= gEpoch;
			utcTime	= gTimeE;
			gTimeU	= utcTime;

	std::cout << std::endl;
	std::cout << "Leap second roll-over:" << std::endl;
	std::cout << "GTime:              " << gTimeE. bigTime << " " << gTimeE .to_string(1) << std::endl;
	std::cout << "GTime to UtcTime:   " << utcTime.bigTime << " " << utcTime.to_string(1) << std::endl;
	std::cout << "UtcTime to GTime:   " << gTimeU. bigTime << " " << gTimeU .to_string(1) << std::endl;
}

#include "coordinates.hpp"
#include "iers2010.hpp"

const GTime j2000TT		= GEpoch{2000, E_Month::JAN, 1,		11,	58,	55.816	+ GPS_SUB_UTC_2000};	// defined in utc 11:58:55.816
void rotationTest()
{
	GTime time = GEpoch{2019, E_Month::FEB, 6,		0,	0,	0};
	
	Vector3d nowPosI = Vector3d::Zero();
	nowPosI(0) = 20000000;
	Vector3d lastPosE = nowPosI;
	
	MjDateUt1	lastmjDate;
	
// 	change to inherit long double, then require to_double() for conversions, deny otherwise.
	
	//do a day's worth of 20 second increment tests
	for (; time < GEpoch{2019, E_Month::FEB, 9,		0,	0,	0}; time += 10)
	{
		Matrix3d i2tMatrix	= Matrix3d::Identity();
	
		//convert to terrestrial
		ERPValues erpv = getErp(nav.erp, time);
		eci2ecef(time, erpv, i2tMatrix);
		
		MjDateUt1	mjDate	(time, erpv.ut1Utc);
		
		Vector3d rSatEcef	= i2tMatrix * nowPosI;
		Vector3d deltaP		= rSatEcef - lastPosE;
		
		long double deltamjdtt = mjDate.val - lastmjDate.val;
		
		printf("\n%s - {%15.6f} [%20.32e %30.23e %30.23e] %30.23e",
			   time.to_string().c_str(),
			   rSatEcef.dot(deltaP),
			   (double)mjDate.val,
			   (double)deltamjdtt,
			   (double)(mjDate.val),
			   rSatEcef.dot(lastPosE)
  			);
		
		lastPosE		= rSatEcef; 
		
// 		std::cout << rSatEcef.dot(lastPosE);
		lastmjDate	= mjDate;	  
	}
}

void longDoubleTest()
{
	long double a;
	std::cout << std::endl << "This system uses " << sizeof(a) * 8 << " bits for long doubles. (Hopefully that number is 128...)" << std::endl;
}

#include "rtcmEncoder.hpp"
#include "ssr.hpp"
/*
void debugSSR(GTime t0, GTime targetTime, E_Sys sys, SsrOutMap& ssrOutMap)
{
	int			iodPos;
	int			iodEph;
	int			iodClk;
	GTime		ephValidStart;
	GTime		ephValidStop;
	GTime		clkValidStart;
	GTime		clkValidStop;
	Vector3d	dPos[2];
	double		dClk[3] = {};
	double		refClk = 0;
	bool		refClkFound = false;
	bool		posDeltaPass[2];
	bool		clkDeltaPass[2];
	Vector3d	dPosDiff;
	double		dClkDiff[2];
	GTime		ephTime = t0;
	
	for (auto& Sat : getSysSats(sys))
	{
		GObs obs;
		obs.Sat = Sat;
		obs.satNav_ptr = &nav.satNavMap[Sat];
		
		dPos[0] = Vector3d::Zero();
		dPos[1] = Vector3d::Zero();
		
		posDeltaPass[0] = ssrPosDelta(t0, ephTime, obs, obs.satNav_ptr->transmittedSSR,	dPos[0], iodPos, iodEph,	ephValidStart, ephValidStop);
		clkDeltaPass[0] = ssrClkDelta(t0, ephTime, obs, obs.satNav_ptr->transmittedSSR,	dClk[0], iodClk,			clkValidStart, clkValidStop);
		posDeltaPass[1] = ssrPosDelta(t0, ephTime, obs, obs.satNav_ptr->receivedSSR,	dPos[1], iodPos, iodEph,	ephValidStart, ephValidStop);
		clkDeltaPass[1] = ssrClkDelta(t0, ephTime, obs, obs.satNav_ptr->receivedSSR,	dClk[1], iodClk,			clkValidStart, clkValidStop);
		
		if	( posDeltaPass[0] && clkDeltaPass[0]
			&&posDeltaPass[1] && clkDeltaPass[1])
		{
			if	( refClkFound == false
				&&abs(dClk[0]) < 1E-6)
			{
				refClk = dClk[1];
				refClkFound = true;
			}

			dClk[2]		= dClk[1] - refClk;
			
			dPosDiff	= dPos[0] - dPos[1];
			dClkDiff[0]	= dClk[0] - dClk[1];
			dClkDiff[1]	= dClk[0] - dClk[2];

			std::cout << std::setprecision(4) << std::fixed;
			std::cout	<< "Debugging ssr: "
						<< "\tnow time (GPST): "					<< timeGet().to_string(1)
						<< "\ttarget time (GPST): "					<< targetTime.to_string(1)
						<< "\tt0 (GPST): "							<< t0.to_string(1)
						<< "\tsat: "								<< Sat.id()
						<< "\tdecoded: "			<< std::setw(7)	<< dPos[1] .transpose()	<< "\t" << std::setw(8) << dClk[1]		<< "\t" << std::setw(8) << dClk[2]
						<< "\tencoded: "			<< std::setw(7)	<< dPos[0] .transpose()	<< "\t" << std::setw(8) << dClk[0]
						<< "\tencoded-decoded: "	<< std::setw(7)	<< dPosDiff.transpose()	<< "\t" << std::setw(8) << dClkDiff[1]
													<< std::endl;
		}
	}

	std::cout << std::setprecision(4) << std::fixed;
	for (auto& [sat, ssrOut] : ssrOutMap)
	{
		std::cout	<< "Straddle clks: "
					<< "\tnow time (GPST): "				<< timeGet().to_string(1)
					<< "\ttarget time (GPST): "				<< targetTime.to_string(1)
					<< "\tt0 (GPST): " 						<< t0.to_string(1)
					<< "\tsat: "							<< sat.id()
					<< "\ttime[0]: "						<< ssrOut.clkInput.vals[0].time.to_string(1)
					<< "\tiode[0]: "	<< std::setw( 3)	<< ssrOut.clkInput.vals[0].iode
					<< "\tbrdc[0]: "	<< std::setw(12)	<< ssrOut.clkInput.vals[0].brdcClk
					<< "\tprec[0]: "	<< std::setw(12)	<< ssrOut.clkInput.vals[0].precClk
					<< "\ttime[1]: "						<< ssrOut.clkInput.vals[1].time.to_string(1)
					<< "\tiode[1]: "	<< std::setw( 3)	<< ssrOut.clkInput.vals[1].iode
					<< "\tbrdc[1]: "	<< std::setw(12)	<< ssrOut.clkInput.vals[1].brdcClk
					<< "\tprec[1]: "	<< std::setw(12)	<< ssrOut.clkInput.vals[1].precClk
										<< std::endl;
	}
}*/

void reflector()
{
	Vector3d face[3];
	for (int i = 0; i < 3; i++)
	{
		face[i] = Vector3d::Zero();
		face[i](i) = 1;
	}	
	
	double absorbtion	[3] = {};
	double specularity	[3] = {};
	
	absorbtion	[0] = 1;
	specularity	[1] = 1;
// 	specularity	[2] = 0;
	
	for (int x : {0, 1})
	for (int y : {0, 1})
	for (int z : {0, 1})
	{
		Vector3d source;
		source(0) = x;
		source(1) = y;
		source(2) = z;
		
		source.normalize();
		
		Vector3d totalMomentum = Vector3d::Zero();
		
		double totalFrontalarea = 0;
		
		for (int i = 0; i < 3; i++)
		{
			Vector3d correctFace = face[i];
			
			if (correctFace.dot(source) < 0)
			{
				correctFace *= -1;
			}
			
			double frontalArea = 1 * source.dot(face[i]);
			totalFrontalarea += frontalArea;
			
			Vector3d incoming	= frontalArea * source;
			Vector3d reflected	= -frontalArea * (source - 2 * (source.dot(correctFace)) * correctFace) * specularity[i];
			Vector3d emissive	= frontalArea * correctFace * (1-specularity[i]) * 0.7;
			
			Vector3d outgoing	= (1 - absorbtion[i]) * (reflected + emissive);
			
			Vector3d momentum	= (incoming + outgoing);
			
			
// 			std::cout << incoming.transpose() << std::endl;
// 			std::cout << reflected.transpose() << std::endl;
// 			std::cout << emissive.transpose() << std::endl;
			totalMomentum += momentum;
		}
		totalMomentum /= totalFrontalarea;
		printf("%10.4f %10.4f %10.4f -> %10.4f %10.4f %10.4f \n", source(0), source(1), source(2), totalMomentum(0), totalMomentum(1), totalMomentum(2));
	}
	
}

// void Spawn()
// {
// 	int pid = fork();
// 	
// 	std::cout << pid << std::endl;
// 	if (pid)
// 	{
// 		return;
// 		std::cout << "returning\n";
// 	}
// 	while (pid < 10000)
// 		std::cout << pid++ << std::endl;
// 	
// }

#include "streamParser.hpp"
#include "streamFile.hpp"
#include "rinex.hpp"
#include "coordinates.hpp"
#include "erp.hpp"

#include "geomagField.hpp"

void debugIGRF()
{
	std::cout << "\nDebugging IGRF:" << std::endl;

	// // test 1 - file reading
	// std::cout << "  n  m         g         h" << std::endl;
	// for (auto& [year, igrfMF] : igrfMFMap)
	// {
	// 	std::cout << igrfMF.year << ":" << std::endl;
	// 	for (int i = 0; i <= igrfMF.maxDegree; i++)
	// 	for (int j = 0; j <= i; j++)
	// 	{
	// 		std::cout	<< std::setprecision(2) << std::fixed;
	// 		std::cout	<< " " << std::setw(2) << i
	// 					<< " " << std::setw(2) << j
	// 					<< " " << std::setw(9) << igrfMF.gnm(i, j)
	// 					<< " " << std::setw(9) << igrfMF.hnm(i, j)
	// 					<< std::endl;
	// 	}
	// }

	// {
	// 	std::cout << igrfSV.year << "-" << igrfSV.yearEnd << ":" << std::endl;
	// 	for (int i = 0; i <= igrfSV.maxDegree; i++)
	// 	for (int j = 0; j <= i; j++)
	// 	{
	// 		std::cout	<< std::setprecision(2) << std::fixed;
	// 		std::cout	<< " " << std::setw(2) << i
	// 					<< " " << std::setw(2) << j
	// 					<< " " << std::setw(9) << igrfSV.gnm(i, j)
	// 					<< " " << std::setw(9) << igrfSV.hnm(i, j)
	// 					<< std::endl;
	// 	}
	// }

	// // test 2 - get coefficients
	// {
	// 	std::cout << "                    ";
	// 	for (int i = 0; i <= 13; i++)
	// 	for (int j = 0; j <= i;  j++)
	// 	{
	// 		std::cout	<< "   g " << std::setw(2) << i << "," << std::setw(2) << j
	// 					<< "   h " << std::setw(2) << i << "," << std::setw(2) << j;
	// 	}
	// 	std::cout << std::endl;
	// }

	// for (int year = 1981; year <= 2026; year++)
	// {
	// 	GEpoch ep	= {year, 1, 1, 0, 0, 0.0};
	// 	GTime time	= ep;

	// 	GeomagMainField igrfMF;
	// 	bool pass = getSHCoef(time, igrfMF);

	// 	if (!pass)
	// 		return;
		
	// 	std::cout << time.to_string() << ":";
	// 	for (int i = 0; i <= igrfMF.maxDegree; i++)
	// 	for (int j = 0; j <= i; j++)
	// 	{
	// 		std::cout	<< std::setprecision(2) << std::fixed;
	// 		std::cout	<< " " << std::setw(9) << igrfMF.gnm(i, j)
	// 					<< " " << std::setw(9) << igrfMF.hnm(i, j);
	// 	}
	// 	std::cout << std::endl;
	// }

	// test 3.1 - time series
	{
		Vector3d r = {-4.05205271694605e+06, 4.21283598092691e+06, -2.54510460797403e+06};	// Cartesian - ALIC
		VectorPos pos = ecef2pos(r);
		pos[0] = asin(r.z()/r.norm());
		pos[1] = atan2(r.y(), r.x());
		pos[2] = r.norm();

		std::cout	<< std::setprecision(5) << std::fixed;
		std::cout	<< "\n\tGeocentric pos: " << pos[0]*R2D << " " << pos[1]*R2D << " " << pos[2]/1000 << std::endl;

		for (int year = 1981; year <= 2023; year++)
		{
			GEpoch ep = {year, 1, 1, 0, 0, 0.0};
			GTime time = ep;

			Vector3d intensity = getGeomagIntensity(time, pos);

			std::cout	<< std::setprecision(5) << std::fixed;
			std::cout	<< "\tyear: " << ep.year;
			std::cout	<< std::setprecision(1) << std::fixed;
			std::cout	<< "\tX: " << std::setw(8) << intensity.x()
						<< "\tY: " << std::setw(8) << intensity.y()
						<< "\tZ: " << std::setw(8) << intensity.z()
						<< std::endl;
		}
	}

	// test 3.2 - grid (including singularity)
	{
		GEpoch ep = {2019, 7, 18, 0, 0, 0.0};
		GTime time = ep;
		double year = decimalYear(time);

		std::cout	<< std::setprecision(5) << std::fixed;
		std::cout	<< "\n\tyear: " << year << std::endl;

		for (int lat =  -90; lat <=  90; lat += 10)
		for (int lon = -180; lon <= 180; lon += 20)
		{
			VectorPos	pos = Vector3d(lat*D2R, lon*D2R, 6371000);

			Vector3d intensity = getGeomagIntensity(time, pos);

			std::cout	<< std::setprecision(1) << std::fixed;
			std::cout	<< "\tGeocentric pos: " << std::setw(5) << pos[0]*R2D << " " << std::setw(6) << pos[1]*R2D << " " << std::setw(4) << pos[2]/1000;
			std::cout	<< std::setprecision(1) << std::fixed;
			std::cout	<< "\tX: " << std::setw(8) << intensity.x()
						<< "\tY: " << std::setw(8) << intensity.y()
						<< "\tZ: " << std::setw(8) << intensity.z()
						<< std::endl;
		}
	}
}

#include "attitude.hpp"
#include "planets.hpp"

void debugAttitude()
{
	// // GPS
	// SatSys Sat(E_Sys::GPS, 1);
	// GEpoch ep = {2019, 07, 18, 0, 0, 0};
	// int nEpoch = 288;
	// double interval = 300;

	// // GRACE C
	// SatSys Sat(E_Sys::LEO, 65);
	// // GEpoch ep = {2019, 02, 13, 0, 0, 0};
	// GEpoch ep = {2022, 01, 01, 0, 0, 0};
	// int nEpoch = 8640;
	// double interval = 10;

	// // GRACE D
	// SatSys Sat(E_Sys::LEO, 65);
	// GEpoch ep = {2022, 01, 01, 0, 0, 0};
	// int nEpoch = 8640;
	// double interval = 10;

	// // COSMIC2 - 1
	// SatSys Sat(E_Sys::LEO, 80);
	// GEpoch ep = {2022, 12, 31, 23, 39, 43};
	// int nEpoch = 2261;
	// double interval = 1;

	// SPIRE
	SatSys Sat(E_Sys::LEO, 99);
	GEpoch ep = {2023, 01, 01, 9, 59, 46};
	int nEpoch = 5853;
	double interval = 1;

	GObs obs;
	obs.Sat = Sat;
	obs.time = ep;

	Station rec;
	rec.id = Sat.id();

	auto& satOpts = acsConfig.getSatOpts(Sat);
	auto& recOpts = acsConfig.getRecOpts(rec.id);

	AttStatus attStatus = {};
	VectorEcef rSun;
	VectorEcef eSun;

	printf("\n");
	for (int i=0; i<nEpoch/interval+50; i++)
	{
		int	week = GWeek(obs.time);
		double tow = GTow(obs.time);

		// satAntAtt(obs, satOpts.antenna_boresight, satOpts.antenna_azimuth, acsConfig.model.sat_attitude.source, attStatus, true);

		recAtt(rec, obs.time, recOpts.rec_attitude.sources);
		attStatus = rec.attStatus;

		printf("%d %8.1f\t%9.6f %9.6f %9.6f\t%9.6f %9.6f %9.6f\t%9.6f %9.6f %9.6f\t%9.6f %9.6f %9.6f\t%9.6f %9.6f %9.6f\t%9.6f %9.6f %9.6f\t", 
				week, tow, 
				attStatus.eXBody.x(),	attStatus.eXBody.y(),	attStatus.eXBody.z(), 
				attStatus.eYBody.x(),	attStatus.eYBody.y(),	attStatus.eYBody.z(), 
				attStatus.eZBody.x(),	attStatus.eZBody.y(),	attStatus.eZBody.z(), 
				attStatus.eXAnt	.x(),	attStatus.eXAnt	.y(),	attStatus.eXAnt	.z(), 
				attStatus.eYAnt	.x(),	attStatus.eYAnt	.y(),	attStatus.eYAnt	.z(), 
				attStatus.eZAnt	.x(),	attStatus.eZAnt	.y(),	attStatus.eZAnt	.z());

		planetPosEcef(obs.time, E_ThirdBody::SUN, rSun);
		eSun = rSun.normalized();

		printf("%9.6f %9.6f %9.6f\n", 
				eSun.x(), eSun.y(), eSun.z());

		obs.time += interval;
	}
}


void debugErp()
{
	ERP	erp = nav.erp;

	// Output all ERP data
	std::cout << std::endl << "EOP reading:";
	for (auto& erpMap : erp.erpMaps)
	{
		std::cout << std::endl;
		for (auto& [time, erpv] : erpMap)
		{
			MjDateUtc	mjd = time;
			std::cout	<< std::setprecision( 6)	<< std::fixed
						<< "\t"						<< time.to_string(1)
						<< " "	<< std::setw( 8)	<< mjd.val;
			std::cout	<< std::setprecision( 6)	<< std::fixed
						<< " "	<< std::setw( 9)	<< erpv.xp / AS2R
						<< " "	<< std::setw( 9)	<< erpv.yp / AS2R;
			std::cout	<< std::setprecision( 7)	<< std::fixed
						<< " "	<< std::setw(10)	<< erpv.ut1Utc
						<< " "	<< std::setw(10)	<< erpv.lod
						<< " "						<< (erpv.isPredicted ? 'P' : ' ')
								<< std::endl;

			writeErp(acsConfig.erp_filename, erpv);
		}
	}

	MjDateUtc mjd;
	mjd.val = 60069;
	GTime time = mjd;

	std::cout << std::endl << "EOP interpolation/extrapolation:" << std::endl;
	for (int i = 0; i < 60; i++)
	{
		time += S_IN_DAY/4;
		ERPValues erpv = getErp(erp, time);

		MjDateUtc	mjd = time;
		std::cout	<< std::setprecision( 6)	<< std::fixed
					<< "\t"						<< time.to_string(1)
					<< " "	<< std::setw( 8)	<< mjd.val;
		std::cout	<< std::setprecision( 6)	<< std::fixed
					<< " "	<< std::setw( 9)	<< erpv.xp / AS2R
					<< " "	<< std::setw( 9)	<< erpv.yp / AS2R;
		std::cout	<< std::setprecision( 7)	<< std::fixed
					<< " "	<< std::setw(10)	<< erpv.ut1Utc
					<< " "	<< std::setw(10)	<< erpv.lod
					<< " "						<< (erpv.isPredicted ? 'P' : ' ')
							<< std::endl;
	}
}

void infiniteTest()
{
	KFState kfState;
	
	
	GTime time;
	time += 60;
	
// 	for (int i = 0; i < 1000; i++)
	{
		KFMeasEntryList kfMeasEntryList;
	
		KFKey ionoKey;
		KFKey ambKey;
		
		ionoKey.type = KF::IONO_STEC;
		ambKey.type = KF::AMBIGUITY;
		
		
		InitialState ionoInit;
		ionoInit.P = 100;
		ionoInit.Q = -1;
		
		InitialState ambInit;
		ambInit.P = 100;
		
		
		
		{
			KFKey obsKey;
			obsKey.num = 1;
			
			KFMeasEntry measEntry(&kfState);
			
			measEntry.addDsgnEntry(ionoKey,	1, ionoInit);
			
			measEntry.setInnov(5);
// 			measEntry.setNoise(1);
			
			measEntry.addNoiseEntry(obsKey, 1, 1);
			
			kfMeasEntryList.push_back(measEntry);
		}
		
		{
			KFKey obsKey;
			obsKey.num = 2;
			
			KFMeasEntry measEntry(&kfState);
			
			measEntry.addDsgnEntry(ionoKey,	1, ionoInit);
			measEntry.addDsgnEntry(ambKey,	1, ambInit);
			
			measEntry.setInnov(8);
// 			measEntry.setNoise(1);
			
			measEntry.addNoiseEntry(obsKey, 1, 1);
			
			kfMeasEntryList.push_back(measEntry);
		}
		
		kfState.output_residuals = true;
		
		kfState.stateTransition(std::cout, time);
		
		kfState.outputStates(std::cout);
	
		KFMeas combinedMeas = kfState.combineKFMeasList(kfMeasEntryList, time);
		
		kfState.filterKalman(std::cout, combinedMeas, true);
		
		kfState.outputStates(std::cout);

		
		time++;
	}	
}

void doDebugs()
{
	// debugErp();
	// exit(0);;

// 	infiniteTest();
// 	exit(0);
}


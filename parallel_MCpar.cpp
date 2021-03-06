// parallel_MCpar.cpp : This file contains the 'main' function. Program execution begins and ends there.
// Parallel version of the Monte Carlo particle simulation in a 1D space with position dependent anomalous exponent.
// Daniel Han
// 9 Oct 2018

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

#include "pch.h"
#include <iostream>
#include <math.h>
#include <random>
#include <chrono>
#include <fstream>
#include "ppl.h"

using namespace std;

#define PI 3.1415926535897932384626433832795028841971693993751058209

double unirandomgen(double minNum, double maxNum) {
	//Variable to obtain a seed for the random number engine
	std::random_device rd;
	//standard mersenne_twister_engine seeded with rd()
	std::mt19937 gen(rd());
	//define random number distribution
	std::uniform_real_distribution<> dist(minNum, maxNum);
	//return the uniformly distributed number
	return dist(gen);
}

double ratefunc(double mu, double t0, double residence_time) {
	//take in the residence time and calculate the rate at that instant for residence time
	return mu / (t0 + residence_time);
}

double MLrandgen(double mu, double t0) {
	//take two uniformly generated numbers
	double u = unirandomgen(0., 1.);
	double v = unirandomgen(0., 1.);
	//generate Mittag-Leffler numbers
	return -t0 * log(u)*pow(((sin(mu*PI)) / (tan(mu*PI*v))) - cos(mu*PI), 1. / mu);
}

template <typename twod>
void randompath_sim(double endtime, double mu[], int state, double t0[], int nout, double displaytimes[], twod& state_status, int nstates) {
	mutex cm;
	//intialize time for particle and the last time that just occured
	double t = 0.;
	double tbefore = 0.;
	//loop of simulation until endtime
	while (t <= endtime) {
		//draw random times for particle
		double dt = MLrandgen(mu[state], t0[state]);
		//update time
		t += dt;
		//store particle state at times in displaytimes
		for (int i = 0; i < nout; i++) {
			if (displaytimes[i] <= t && tbefore <= displaytimes[i]) {
				cm.lock();
				state_status[i][state] += 1;
				cm.unlock();
			}
		}
		//update the time when the previous event occurred
		tbefore += dt;
		//change the state of the event that just occured
		//uniform drift
		//            int ran_choice = rand() % 2;
		//biased drift
		double ran_choice = unirandomgen(0., 1.);
		double ri = 0.5 + (.5 / double(nstates)*(0.5 - double(state) / double(nstates)));
		if (ran_choice < ri) {
			//if(ran_choice < 0.5+epsilon){
			if (state == nstates - 1) {
				state = nstates - 1;
			}
			else {
				state += 1;
			}
		}
		else {
			if (state == 0) {
				state = 0;
			}
			else {
				state -= 1;
			}
		}
	}// end of while loop (time for one particle)
	cm.lock();
	state_status[nout][state] += 1;
	cm.unlock();
}

int main() {
	//INITIALIZATION
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//declare all the constant parameters
	//total number of particles
	int N = 10000;
	//Rate function parameters
	//specified number of states
	const int nstates = 50;
	//maximum value of mu
	double maxmu = .9;
	//minimum value of mu
	double minmu = .4;
	//container for mus of different states
	double mu[nstates] = {};
	double t0[nstates] = {};
	//assign different values
	for (int i = 0; i < nstates; i++) {
		mu[i] = minmu + (i / double(nstates - 1))*(maxmu - minmu);
		t0[i] = 1e-3 * exp((i / double(nstates - 1))*5);
		//cout << mu[i] << endl;
	}
	//initialize file for storing info
	ofstream data;
	data.open("test.txt");
	//Rate function: t0 parameter
	//double t0 = 1e-3;
	//time of entire simulation
	double endtime = 1e6;
	//number of output points desired
	const int nout = 10;
	double displaytimes[nout] = {};
	for (int i = 0; i < nout; i++) { displaytimes[i] = endtime * (i / double(nout)); }
	//container for storing how many particles inside states
	int init_state[nstates] = {};
	int state_status[nout + 1][nstates] = {};
	//initialize states with specific conditions
	//uniform initial conditions
	for(int i=0;i<nstates;i++){ init_state[i] = int(double(N)/double(nstates)); }
	//non-uniform initial conditions
	//for (int i = nstates / 2; i < nstates; i++) {
		//init_state[i] = int(double(N) / double(nstates / 2));
		//cout << init_state[i] << endl;
	//}

	//PARALLEL SIMULATION
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	auto pstart = chrono::high_resolution_clock::now();
	//loop of simulation for each particle
	concurrency::parallel_for(size_t(0), size_t(N), [&] (size_t pdex) {
		//print out every 10% simulation step completed for sanity
		if (pdex % int(N / 10) == 0) {
			cout << 100 * pdex / double(N) << endl;
		}
		//initialize state depending on initial conditions
		int state = 0;
		int countstate = init_state[0];
		while (pdex >= countstate) {
			state += 1;
			countstate += init_state[state];
		}
		randompath_sim(endtime, mu, state, t0, nout, displaytimes, state_status, nstates);
	});// end of for loop (number of particles)
	auto pfinish = chrono::high_resolution_clock::now();
	chrono::duration<double> pelapsed = pfinish - pstart;
	cout << "Parallel Elapsed time: " << pelapsed.count() << "s" << endl;


	//OUTPUT TO FILE
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	for (int i = 0; i < nout + 1; i++) {
		if (i == nout) {
			data << endtime;
		}
		else {
			data << displaytimes[i];
		}
		for (int j = 0; j < nstates; j++) {
			data << "," << state_status[i][j];
		}
		data << endl;
	}
	data.close();
	return 0;
}

#ifndef GLOBALS_H
#define GLOBALS_H

#include <chrono>
#include <cstddef>

// Declaration of global variables
extern double startTime;
extern double endTime;

extern double subStartTime;
extern double subEndTime;

extern double time_forward;
extern double forward_l62;
extern double forward_l159;
extern double forward_l469;
extern double forward_l5136;
extern double forward_l577;
extern double forward_l642;

extern double x_flipt;
extern double rnn1t;
extern double rnn2t;
extern double rnn3t;
extern double rnn4t;
extern double rnn5t;


// Function to measure time difference
double getTimeDifference();

double getSubTimeDifference();

#endif

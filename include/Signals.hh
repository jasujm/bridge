// This class contains common signal handling routines used by both bridge and
// card server applications

#ifndef SIGNALS_HH_
#define SIGNALS_HH_

void startHandlingSignals(void (*handler)(int));

void stopHandlingSignals();

#endif // SIGNALS_HH_

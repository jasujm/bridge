// This class contains common signal handling routines used by both bridge and
// card server applications

#ifndef SIGNALS_HH_
#define SIGNALS_HH_

namespace Bridge {

/** \brief Handle SIGTERM and SIGINT
 *
 * Set \p handler as signal handler for SIGTERM and SIGINT signals. This is
 * intended to be called at startup. Failing to set signal handler is fatal
 * error and leads to the termination of the program.
 *
 * \param handler the signal handler
 */
void startHandlingSignals(void (*handler)(int));

/** \brief Stop handling signals
 *
 * Reset signal handler earlier set using startHandlingSignals().
 */
void stopHandlingSignals();

}

#endif // SIGNALS_HH_

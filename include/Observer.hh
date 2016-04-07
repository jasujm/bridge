/** \file
 *
 * \brief Definition of an implementation of the observer pattern
 *
 * This file contains the observer pattern implementation used in the Bridge
 * framework. Observer pattern is described
 * e.g. https://en.wikipedia.org/wiki/Observer_pattern.
 *
 * The Observer and Observable classes implemented in this file are not meant
 * for inter‐thread communication. Their implementation is not thread safe.
 */

#ifndef OBSERVER_HH_
#define OBSERVER_HH_

#include <memory>
#include <list>

namespace Bridge {

/** \brief Observer part of the observer pattern implementation
 *
 * This class is meant to be inherited by a class that wishes to receive
 * notifications about some events of interest.
 *
 * \sa Observable
 */
template<typename... T>
class Observer {
public:

    virtual ~Observer() = default;

    /** \brief Notify observer
     *
     * \param args parameters from Observable::notifyAll
     */
    void notify(const T&... args);

private:

    /** \brief Handle for notifying the observer
     *
     * \sa notify()
     */
    virtual void handleNotify(const T&... args) = 0;
};

template<typename... T>
inline void Observer<T...>::notify(const T&... args)
{
    handleNotify(args...);
}

/** \brief Observable part of the observer pattern
 *
 * This class is meant to be used by or inherited by a class that wishes to
 * publish events to Observer objects.
 *
 * \note To prevent external callers from calling notifyAll() and to prevent
 * nonvirtual destruction of the object, the inheritance shoud be protected
 * and subscribe() should be exposed by using declaration if needed.
 *
 * \note The implementation of Observable is not meant for inter‐thread
 * communication and is not thread safe
 */
template<typename... T>
class Observable {
public:

    /** \brief Subscribe observer to this Observable
     *
     * Observers will receive notifications when notifyAll() is called. The
     * observer is allowed to go out of scope, in which case it is simply
     * removed from the list of observers.
     *
     * \note This method is protected by default to let the derived class to
     * control subscriptions. It can choose to publish the method with using
     * declaration.
     *
     * \param observer the observer to be registered
     */
    void subscribe(std::weak_ptr<Observer<T...>> observer);

    /** \brief Notify all observers
     *
     * \param args parameters to pass to Observer::notify
     */
    void notifyAll(const T&... args);

private:
    std::list<std::weak_ptr<Observer<T...>>> observers;
};

template<typename... T>
inline void Observable<T...>::subscribe(
    std::weak_ptr<Observer<T...>> observer)
{
    observers.emplace_back(observer);
}

template<typename... T>
inline void Observable<T...>::notifyAll(const T&... args)
{
    for (auto iter = observers.begin(); iter != observers.end(); ) {
        if (auto observer = iter->lock()) {
            observer->notify(args...);
            ++iter;
        } else {
            iter = observers.erase(iter);
        }
    }
}

}

#endif // OBSERVER_HH_

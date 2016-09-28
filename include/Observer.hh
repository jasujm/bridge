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

#include "FunctionQueue.hh"

#include <memory>
#include <list>
#include <tuple>
#include <utility>

namespace Bridge {

template<typename... T> class Observable;

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

    /** \brief Type of observable the observer can be subscribed to
     */
    using ObservableType = Observable<T...>;

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
void Observer<T...>::notify(const T&... args)
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

    /** \brief Type of observer that can be subscribed to the observable
     */
    using ObserverType = Observable<T...>;

    /** \brief Subscribe observer to this Observable
     *
     * Observers will receive notifications when notifyAll() is called. The
     * lifetime of the observer is allowed to end at any point, in which case
     * it is simply removed from the list of observers.
     *
     * The method can be called even if notification is already ongoing. In
     * that case the new observer will receive the current notification after
     * the existing subscribers.
     *
     * \param observer the observer to be registered
     */
    void subscribe(std::weak_ptr<Observer<T...>> observer);

    /** \brief Notify all observers
     *
     * This method call will call Observer::notify() for all observers that
     * have previously subscribed to notifications.
     *
     * The method can be called even if notification is already ongoing. In
     * that case the notification is queued and starts when all subscribers
     * have received the prior notification or notifications.
     *
     * \param args arguments for Observer::notify
     */
    template<typename... U>
    void notifyAll(U&&... args);

private:

    template<std::size_t... Ns>
    void internalNotifyAllHelper(
        const std::tuple<T...>& args, std::index_sequence<Ns...>);

    std::list<std::weak_ptr<Observer<T...>>> observers;
    FunctionQueue functionQueue;
};

template<typename... T>
void Observable<T...>::subscribe(
    std::weak_ptr<Observer<T...>> observer)
{
    observers.emplace_back(observer);
}

template<typename... T>
template<typename... U>
void Observable<T...>::notifyAll(U&&... args)
{
    functionQueue(
        [this, args = std::tuple<T...> {std::forward<U>(args)...}]()
        {
            internalNotifyAllHelper(args, std::index_sequence_for<T...> {});
        });
}

template<typename... T>
template<std::size_t... Ns>
void Observable<T...>::internalNotifyAllHelper(
    const std::tuple<T...>& args, std::index_sequence<Ns...>)
{
    for (auto iter = observers.begin(); iter != observers.end(); ) {
        if (auto observer = iter->lock()) {
            observer->notify(std::get<Ns>(args)...);
            ++iter;
        } else {
            iter = observers.erase(iter);
        }
    }
}

}

#endif // OBSERVER_HH_

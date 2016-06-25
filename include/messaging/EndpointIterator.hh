/** \file
 *
 * \brief Definition of Bridge::Messaging::EndpointIterator
 */

#ifndef MESSAGING_ENDPOINTITERATOR_HH_
#define MESSAGING_ENDPOINTITERATOR_HH_

#include <boost/iterator/iterator_facade.hpp>

#include <string>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Generator of ZeroMQ TCP endpoints
 *
 * The input of an EndpointIterator is a ZeroMQ TCP endpoint consisting of
 * address and base port. Advancing the iterator advances the base
 * port. Dereferencing the iterator returns the endpoint with the same address
 * and the shifter port. Thus EndpointIterator can be used to generate a
 * sequence of TCP endpoints.
 *
 * \code{.cc}
 * EndpointIterator iter {"127.0.0.1", 5555};
 * assert(*iter == "tcp://127.0.0.1:5555");
 * ++iter;
 * assert(*iter == "tcp://127.0.0.1:5556");
 * \endcode
 *
 * EndpointIterator is a random access iterator. Distance between two endpoint
 * iterators is measured by the difference in port number and address is not
 * taken into account. Although possible, measuring difference between
 * iterators enumerating different addresses or port ranges does not make
 * sense.
 */
class EndpointIterator : public boost::iterator_facade<
    EndpointIterator,
    std::string,
    boost::random_access_traversal_tag,
    std::string,
    int>
{
public:

    /** \brief Create endpoint iterator
     *
     * \param endpoint The base endpoint. The endpoint must be correctly
     * formated ZeroMQ TCP endpoint, i.e. it must consist of
     *   - prefix tcp://
     *   - address or interface name
     *   - colon (:)
     *   - port number (integer)
     *
     * \throw std::invalid_argument if \p endpoint has incorrect format
     */
    explicit EndpointIterator(const std::string& endpoint);

    /** \brief Create endpoint iterator
     *
     * \param address the address or interface name
     * \param port the base port
     */
    EndpointIterator(std::string address, int port);

private:

    EndpointIterator(std::pair<std::string, int> endpoint);

    reference dereference() const;
    bool equal(const EndpointIterator& other) const;
    void increment();
    void decrement();
    void advance(difference_type n);
    difference_type distance_to(const EndpointIterator& other) const;

    std::string address;
    int port;

    friend class boost::iterator_core_access;
};

}
}

#endif // MESSAGING_ENDPOINTITERATOR_HH_

#include "messaging/EndpointIterator.hh"

#include <boost/lexical_cast.hpp>

#include <cassert>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace Bridge {
namespace Messaging {

namespace {

const auto ENDPOINT_REGEX = std::regex {"tcp://(.+):(\\d+)"};

auto parseEndpoint(const std::string& endpoint)
{
    auto match = std::smatch {};
    const auto success = std::regex_match(
        endpoint.begin(), endpoint.end(),
        match, ENDPOINT_REGEX);
    if (!success) {
        throw std::invalid_argument {"Invalid endpoint format"};
    }
    assert(match.size() == 3);
    return std::make_pair(
        match[1],
        boost::lexical_cast<int>(match[2]));
}

}

EndpointIterator::EndpointIterator(const std::string& endpoint) :
    EndpointIterator {parseEndpoint(endpoint)}
{
}

EndpointIterator::EndpointIterator(std::string address, int port) :
    EndpointIterator {std::make_pair(std::move(address), port)}
{
}

EndpointIterator::EndpointIterator(std::pair<std::string, int> endpoint) :
    address {std::move(endpoint.first)},
    port {endpoint.second}
{
}

EndpointIterator::reference EndpointIterator::dereference() const
{
    std::ostringstream str;
    str << "tcp://" << address << ":" << port;
    return str.str();
}

void EndpointIterator::increment()
{
    ++port;
}

void EndpointIterator::decrement()
{
    --port;
}

void EndpointIterator::advance(difference_type n)
{
    port += n;
}

EndpointIterator::difference_type EndpointIterator::distance_to(
    const EndpointIterator& other) const
{
    return other.port - this->port;
}

bool EndpointIterator::equal(const EndpointIterator& other) const
{
    printf("%s %s\n", address.c_str(), other.address.c_str());
    return address == other.address && port == other.port;
}

}
}

#ifndef TESTSERIALIZATIONPOLICY_HH_
#define TESTSERIALIZATIONPOLICY_HH_

#include <boost/lexical_cast.hpp>

#include <string>

namespace Bridge {
namespace Messaging {

class TestSerializationPolicy {
public:
    template<typename T> std::string serialize(const T& t);
    template<typename T> T deserialize(const std::string& s);
};

template<typename T>
std::string TestSerializationPolicy::serialize(const T& t)
{
    return boost::lexical_cast<std::string>(t);
}

template<typename T>
T TestSerializationPolicy::deserialize(const std::string& s)
{
    return boost::lexical_cast<T>(s);
}

}
}

#endif // TESTSERIALIZATIONPOLICY_HH_

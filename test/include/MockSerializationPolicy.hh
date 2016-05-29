#ifndef MOCKSERIALIZATIONPOLICY_HH_
#define MOCKSERIALIZATIONPOLICY_HH_

#include <boost/lexical_cast.hpp>

#include <string>

namespace Bridge {
namespace Messaging {

class MockSerializationPolicy {
public:
    template<typename T> std::string serialize(const T& t);
    template<typename T> T deserialize(const std::string& s);
};

template<typename T>
std::string MockSerializationPolicy::serialize(const T& t)
{
    return boost::lexical_cast<std::string>(t);
}

template<typename T>
T MockSerializationPolicy::deserialize(const std::string& s)
{
    return boost::lexical_cast<T>(s);
}

}
}

#endif // MOCKSERIALIZATIONPOLICY_HH_

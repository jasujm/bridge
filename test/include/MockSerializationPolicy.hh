#ifndef MOCKSERIALIZATIONPOLICY_HH_
#define MOCKSERIALIZATIONPOLICY_HH_

#include "Blob.hh"

#include <boost/lexical_cast.hpp>

#include <string>

namespace Bridge {
namespace Messaging {

class MockSerializationPolicy {
public:
    template<typename T> std::string serialize(const T& t);
    template<typename T> T deserialize(ByteSpan b);
};

template<typename T>
std::string MockSerializationPolicy::serialize(const T& t)
{
    return boost::lexical_cast<std::string>(t);
}

template<typename T>
T MockSerializationPolicy::deserialize(ByteSpan s)
{
    return boost::lexical_cast<T>(blobToString(s));
}

}
}

#endif // MOCKSERIALIZATIONPOLICY_HH_

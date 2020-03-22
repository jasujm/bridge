#include "messaging/Replies.hh"

#include <algorithm>

namespace Bridge {
namespace Messaging {

using namespace BlobLiterals;

const ByteSpan REPLY_SUCCESS = "OK"_BS;
const ByteSpan REPLY_FAILURE = "ERR"_BS;

bool isSuccessful(const ByteSpan code)
{
    const auto first_to_differ = std::mismatch(
        code.begin(), code.end(),
        REPLY_SUCCESS.begin(), REPLY_SUCCESS.end()).second;
    return first_to_differ == REPLY_SUCCESS.end();
}

}
}

#ifndef MOCKMESSAGEHANDLER_HH_
#define MOCKMESSAGEHANDLER_HH_

#include "messaging/MessageHandler.hh"

#include <gmock/gmock.h>

#include <algorithm>

namespace Bridge {
namespace Messaging {

class MockMessageHandler : public MessageHandler {
public:
    MOCK_METHOD3(
        doHandle, bool(const std::string&, ParameterRange, OutputSink));

    // Create functor that writes to sink given as parameter to
    // doHandler. Intended to use with the Invoke action.
    template<typename Iterator>
    static auto writeToSink(Iterator first, Iterator last, bool success = true);
};

template<typename Iterator>
auto MockMessageHandler::writeToSink(
    Iterator first, Iterator last, bool success)
{
    return [first, last, success](
        const std::string&, ParameterRange, OutputSink sink)
    {
        std::for_each(first, last, sink);
        return success;
    };
}

}
}

#endif // MOCKMESSAGEHANDLER_HH_

#ifndef MOCKMESSAGEHANDLER_HH_
#define MOCKMESSAGEHANDLER_HH_

#include "messaging/MessageHandler.hh"
#include "Blob.hh"

#include <gmock/gmock.h>

namespace Bridge {
namespace Messaging {

class MockMessageHandler : public MessageHandler {
public:
    MOCK_METHOD4(
        doHandle,
        void(
            SynchronousExecutionPolicy& execution, const Identity&,
            const ParameterVector&, Response&));
};

class MockResponse : public Response {
public:
    MOCK_METHOD1(handleSetStatus, void(StatusCode));
    MOCK_METHOD1(handleAddFrame, void(ByteSpan));
};

template<typename... Args>
auto Respond(StatusCode status, const Args&... frames)
{
    using namespace Messaging;
    return ::testing::WithArg<3>(
        ::testing::Invoke(
            [status, frames...](Response& response)
            {
                response.setStatus(status);
                if constexpr (sizeof...(frames) > 0) {
                    const auto _frames = { asBytes(frames)... };
                    for (const auto& frame : _frames) {
                        response.addFrame(frame);
                    }
                }
            }));
}

}
}

#endif // MOCKMESSAGEHANDLER_HH_

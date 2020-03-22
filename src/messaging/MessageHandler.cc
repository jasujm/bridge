#include "messaging/MessageHandler.hh"

namespace Bridge {
namespace Messaging {

Response::~Response() = default;

void Response::setStatus(ByteSpan status)
{
    handleSetStatus(status);
}

void Response::addFrame(ByteSpan frame)
{
    handleAddFrame(frame);
}

}
}

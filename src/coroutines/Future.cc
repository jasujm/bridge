#include "coroutines/Future.hh"

namespace Bridge {
namespace Coroutines {

void Future::nullResolve()
{
}

Future::Future() : resolveCallback {&nullResolve}
{
}

void Future::resolve()
{
    resolveCallback();
}

}
}

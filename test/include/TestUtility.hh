#include <gtest/gtest.h>

#include <memory>

namespace Bridge {

MATCHER_P(
    WeaklyPointsTo,
    ptr,
    std::string {negation ? "doesn't point weakly to " : "weakly points to "} +
    ::testing::PrintToString(*ptr))
{
    const auto s_ptr = arg.lock();
    return s_ptr.get() == ::std::addressof(*ptr);
}

}

#include <gtest/gtest.h>

#include <memory>
#include <ranges>
#include <vector>

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

// Needed to work around some googlemock matchers not working with native ranges
// (https://github.com/google/googletest/issues/3564)
template<std::ranges::view View>
auto vectorize(View v)
{
    return std::vector(v.begin(), v.end());
}

}

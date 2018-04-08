#include "bridge/Random.hh"

namespace Bridge {

Rng& getRng()
{
    // Initialize with seed from OS random number source
    static Rng randomEngine {std::random_device()()};
    return randomEngine;
}

}

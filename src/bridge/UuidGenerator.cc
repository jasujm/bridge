#include "bridge/UuidGenerator.hh"

namespace Bridge {

UuidGenerator createUuidGenerator()
{
    return UuidGenerator {&getRng()};
}

}

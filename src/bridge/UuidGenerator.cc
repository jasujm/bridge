#include "bridge/UuidGenerator.hh"

namespace Bridge {

UuidGenerator& getUuidGenerator()
{
    static UuidGenerator uuidGenerator {&getRng()};
    return uuidGenerator;
}

Uuid generateUuid()
{
    auto& uuidGenerator = getUuidGenerator();
    return uuidGenerator();
}

}

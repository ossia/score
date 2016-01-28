#include "IdentifiedObjectAbstract.hpp"

IdentifiedObjectAbstract::~IdentifiedObjectAbstract()
{
    emit identified_object_destroyed(this);
}

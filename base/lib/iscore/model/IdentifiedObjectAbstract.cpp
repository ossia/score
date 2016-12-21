#include "IdentifiedObjectAbstract.hpp"
#include <iscore/tools/std/HashMap.hpp>
IdentifiedObjectAbstract::~IdentifiedObjectAbstract()
{
  static_assert(is_template<iscore::hash_map<int, float>>::value, "");
  emit identified_object_destroyed(this);
}

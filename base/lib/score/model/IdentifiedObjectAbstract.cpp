// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IdentifiedObjectAbstract.hpp"
#include <score/tools/std/HashMap.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(IdentifiedObjectAbstract)
IdentifiedObjectAbstract::~IdentifiedObjectAbstract()
{
  static_assert(is_template<score::hash_map<int, float>>::value, "");
  identified_object_destroyed(this);
}

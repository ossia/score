#pragma once
#include <score/model/IdentifiedObject.hpp>

namespace score
{
namespace IDocument
{

template<typename T>
void changeObjectId(IdentifiedObject<T>& obj, const Id<T>& new_id)
{
  obj.setId(new_id);
  const auto& cld = ((QObject&)obj).findChildren<IdentifiedObjectAbstract*>();
  for(auto child : cld)
    child->resetCache();
}

}
}

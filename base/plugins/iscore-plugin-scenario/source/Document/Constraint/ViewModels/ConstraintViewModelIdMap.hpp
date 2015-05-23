#pragma once
#include<QHash>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
class AbstractConstraintViewModel;
using ConstraintViewModelIdMap = QHash<ObjectPath, id_type<AbstractConstraintViewModel>>;


inline
uint qHash(const ObjectPath& obj, uint seed)
{
  return qHash(obj.toString(), seed);
}

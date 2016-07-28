#include "BaseProperty.hpp"


namespace Ossia
{
namespace LocalTree
{
BaseProperty::~BaseProperty()
{
    node.getParent()->removeChild(node);
}
}
}

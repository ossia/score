#include "MultiCursor.hpp"


namespace Example
{
void MultiCursorManager::operator()(halp::tick t)
{
  outputs.out.value = inputs.pos.value;
}
}

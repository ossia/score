#include "CurveProcessModel.hpp"

namespace Curve
{
CurveProcessModel::~CurveProcessModel()
{
  emit identified_object_destroying(this);
}
}

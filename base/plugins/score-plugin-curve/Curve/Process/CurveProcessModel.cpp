// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveProcessModel.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(Curve::CurveProcessModel)
namespace Curve
{
CurveProcessModel::~CurveProcessModel()
{
  identified_object_destroying(this);
}
}

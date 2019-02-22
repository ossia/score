#pragma once
#include <QMetaType>

#include <wobjectdefs.h>

enum ExpandMode : int8_t
{
  Scale,
  GrowShrink,
  ForceGrow,
  CannotExpand
};

enum LockMode : int8_t
{
  Free,
  Constrained
};

Q_DECLARE_METATYPE(ExpandMode)
Q_DECLARE_METATYPE(LockMode)

W_REGISTER_ARGTYPE(ExpandMode)
W_REGISTER_ARGTYPE(LockMode)

#pragma once
#include <QMetaType>

enum ExpandMode
{
  Scale,
  GrowShrink,
  ForceGrow,
  CannotExpand
};

enum LockMode
{
  Free,
  Constrained
};

Q_DECLARE_METATYPE(ExpandMode)
Q_DECLARE_METATYPE(LockMode)

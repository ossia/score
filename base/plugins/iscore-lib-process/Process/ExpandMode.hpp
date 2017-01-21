#pragma once
#include <QMetaType>

enum ExpandMode
{
  Scale,
  GrowShrink,
  ForceGrow,
  CannotExpand
};
Q_DECLARE_METATYPE(ExpandMode)

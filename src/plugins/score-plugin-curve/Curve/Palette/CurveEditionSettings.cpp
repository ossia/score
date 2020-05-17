// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveEditionSettings.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(Curve::EditionSettings)
namespace Curve
{

bool EditionSettings::lockBetweenPoints() const
{
  return m_lockBetweenPoints;
}

void EditionSettings::setLockBetweenPoints(bool lockBetweenPoints)
{
  if (m_lockBetweenPoints == lockBetweenPoints)
    return;

  m_lockBetweenPoints = lockBetweenPoints;
  lockBetweenPointsChanged(lockBetweenPoints);
}

bool EditionSettings::suppressOnOverlap() const
{
  return m_suppressOnOverlap;
}

bool EditionSettings::stretchBothBounds() const
{
  return m_stretchBothBounds;
}

AddPointBehaviour EditionSettings::addPointBehaviour() const
{
  return m_addPointBehaviour;
}

void EditionSettings::setSuppressOnOverlap(bool suppressOnOverlap)
{
  if (m_suppressOnOverlap == suppressOnOverlap)
    return;

  m_suppressOnOverlap = suppressOnOverlap;
  suppressOnOverlapChanged(suppressOnOverlap);
}

void EditionSettings::setStretchBothBounds(bool stretchBothBounds)
{
  if (m_stretchBothBounds == stretchBothBounds)
    return;

  m_stretchBothBounds = stretchBothBounds;
  stretchBothBoundsChanged(stretchBothBounds);
}

void EditionSettings::setAddPointBehaviour(Curve::AddPointBehaviour AddPointBehaviour)
{
  if (m_addPointBehaviour == AddPointBehaviour)
    return;

  m_addPointBehaviour = AddPointBehaviour;
  addPointBehaviourChanged(AddPointBehaviour);
}

void EditionSettings::setTool(Tool tool)
{
  if (m_tool == tool)
    return;

  m_tool = tool;
  toolChanged(tool);
}

RemovePointBehaviour EditionSettings::removePointBehaviour() const
{
  return m_removePointBehaviour;
}

void EditionSettings::setRemovePointBehaviour(RemovePointBehaviour removePointBehaviour)
{
  if (m_removePointBehaviour == removePointBehaviour)
    return;

  m_removePointBehaviour = removePointBehaviour;
  removePointBehaviourChanged(removePointBehaviour);
}

Tool EditionSettings::tool() const
{
  return m_tool;
}
}

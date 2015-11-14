#include "CurveEditionSettings.hpp"

namespace Curve {

bool EditionSettings::lockBetweenPoints() const
{
    return m_lockBetweenPoints;
}

void EditionSettings::setLockBetweenPoints(bool lockBetweenPoints)
{
    if (m_lockBetweenPoints == lockBetweenPoints)
        return;

    m_lockBetweenPoints = lockBetweenPoints;
    emit lockBetweenPointsChanged(lockBetweenPoints);
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
    emit suppressOnOverlapChanged(suppressOnOverlap);
}

void EditionSettings::setStretchBothBounds(bool stretchBothBounds)
{
    if (m_stretchBothBounds == stretchBothBounds)
        return;

    m_stretchBothBounds = stretchBothBounds;
    emit stretchBothBoundsChanged(stretchBothBounds);
}

void EditionSettings::setAddPointBehaviour(Curve::AddPointBehaviour AddPointBehaviour)
{
    if (m_addPointBehaviour == AddPointBehaviour)
        return;

    m_addPointBehaviour = AddPointBehaviour;
    emit addPointBehaviourChanged(AddPointBehaviour);
}

void EditionSettings::setTool(Tool tool)
{
    if (m_tool == tool)
        return;

    m_tool = tool;
    emit toolChanged(tool);
}

Tool EditionSettings::tool() const
{
    return m_tool;
}

}

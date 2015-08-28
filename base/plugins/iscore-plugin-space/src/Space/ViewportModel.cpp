#include "ViewportModel.hpp"

const Id<DimensionModel>& ViewportModel::xDim() const
{
    return m_xDim;
}

void ViewportModel::setXDim(const Id<DimensionModel>& xDim)
{
    m_xDim = xDim;
}

const Id<DimensionModel>& ViewportModel::yDim() const
{
    return m_yDim;
}

void ViewportModel::setYDim(const Id<DimensionModel>& yDim)
{
    m_yDim = yDim;
}

const QMap<Id<DimensionModel>, double>& ViewportModel::defaultValuesMap() const
{
    return m_defaultValuesMap;
}

void ViewportModel::setDefaultValuesMap(const QMap<Id<DimensionModel>, double>& defaultValuesMap)
{
    m_defaultValuesMap = defaultValuesMap;
}

double ViewportModel::zoomLevel() const
{
    return m_zoomLevel;
}

void ViewportModel::setZoomLevel(double zoomLevel)
{
    m_zoomLevel = zoomLevel;
}
double ViewportModel::rotation() const
{
    return m_rotation;
}

void ViewportModel::setRotation(double rotation)
{
    m_rotation = rotation;
}

const QPointF& ViewportModel::pos() const
{
    return m_pos;
}

void ViewportModel::setPos(const QPointF& pos)
{
    m_pos = pos;
}

const QString& ViewportModel::name() const
{
    return m_name;
}

void ViewportModel::setName(const QString& name)
{
    m_name = name;
}





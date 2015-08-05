#include "DimensionModel.hpp"


const QString &DimensionModel::name() const
{
    return m_name;
}

spacelib::minmax_symbol &DimensionModel::sym()
{
    return m_sym;
}

const spacelib::minmax_symbol &DimensionModel::sym() const
{
    return m_sym;
}

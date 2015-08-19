#include "DimensionModel.hpp"


const QString &DimensionModel::name() const
{
    return m_name;
}

const spacelib::minmax_symbol &DimensionModel::sym() const
{
    return m_sym;
}

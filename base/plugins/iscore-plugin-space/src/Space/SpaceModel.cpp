#include "SpaceModel.hpp"

SpaceModel::SpaceModel(
        std::vector<DimensionModel> &&sp,
        const id_type<SpaceModel> &id,
        QObject *parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_dimensions{std::move(sp)}
{
    rebuildSpace();
}

void SpaceModel::addDimension(const DimensionModel &dim)
{
    m_dimensions.push_back(dim);
    rebuildSpace();
}

void SpaceModel::removeDimension(const QString &name)
{
    ISCORE_TODO;
    rebuildSpace();
}

void SpaceModel::rebuildSpace()
{
    std::vector<spacelib::minmax_symbol> syms;
    for(const auto& elt : m_dimensions)
        syms.push_back(elt.sym());

    m_space = std::make_unique<spacelib::euclidean_space>(std::move(syms));
    emit spaceChanged();
}

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

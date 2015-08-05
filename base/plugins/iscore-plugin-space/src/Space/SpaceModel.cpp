#include "SpaceModel.hpp"
#include <boost/range/algorithm/find_if.hpp>
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
const DimensionModel& SpaceModel::dimension(const QString &name) const
{
    using namespace boost::range;
    auto it = find_if(m_dimensions, [&] (const auto& dim) { return dim.name() == name; });
    return *it;
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

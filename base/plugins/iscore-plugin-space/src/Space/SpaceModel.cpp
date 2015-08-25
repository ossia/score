#include "SpaceModel.hpp"
#include <algorithm>
SpaceModel::SpaceModel(
        const id_type<SpaceModel> &id,
        QObject *parent):
    IdentifiedObject{id, staticMetaObject.className(), parent}
{
    rebuildSpace();
}

void SpaceModel::addDimension(DimensionModel *dim)
{
    m_dimensions.insert(dim);
    rebuildSpace();
}

const DimensionModel& SpaceModel::dimension(const QString& name) const
{
    auto it = std::find_if(m_dimensions.begin(),
                           m_dimensions.end(),
                           [&] (const DimensionModel& dim) { return dim.name() == name; });

    ISCORE_ASSERT(it != m_dimensions.end());

    return *it;
}

const DimensionModel& SpaceModel::dimension(const id_type<DimensionModel> &id) const
{
    return m_dimensions.at(id);
}

void SpaceModel::removeDimension(const QString &name)
{
    ISCORE_TODO;
    rebuildSpace();
}


void SpaceModel::addViewport(ViewportModel* v)
{
    m_viewports.insert(v);
    if(!m_defaultViewport)
        m_defaultViewport = v->id();
    emit viewportAdded(*v);
}

void SpaceModel::removeViewport(const id_type<ViewportModel>& vm)
{
    ISCORE_TODO;

    if(m_defaultViewport == vm)
    {
        if(!m_viewports.empty())
        {
            m_defaultViewport = (*m_viewports.begin()).id();
        }
        else
        {
            m_defaultViewport = id_type<ViewportModel>{};
        }
    }
}

void SpaceModel::rebuildSpace()
{
    std::vector<spacelib::minmax_symbol> syms;
    for(const auto& elt : m_dimensions)
        syms.push_back(elt.sym());

    m_space = std::make_unique<spacelib::euclidean_space>(std::move(syms));
    emit spaceChanged();
}

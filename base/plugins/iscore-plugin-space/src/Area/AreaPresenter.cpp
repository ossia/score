#include "AreaPresenter.hpp"
#include "AreaModel.hpp"
#include "src/Space/SpaceModel.hpp"

#include <QGraphicsItem>
#include <iscore/widgets/GraphicsItem.hpp>

namespace Space
{
AreaPresenter::AreaPresenter(
        QGraphicsItem* view,
        const AreaModel& model,
        QObject *parent):
    NamedObject{"AreaPresenter", parent},
    m_model{model},
    m_view{view}
{
}

AreaPresenter::~AreaPresenter()
{
    deleteGraphicsItem(m_view);
}

const Id<AreaModel>& AreaPresenter::id() const
{
    return m_model.id();
}
}

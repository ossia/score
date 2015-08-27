#include "AreaPresenter.hpp"
#include "AreaModel.hpp"
#include "src/Space/SpaceModel.hpp"

#include <QGraphicsItem>

AreaPresenter::AreaPresenter(
        QGraphicsItem* view,
        const AreaModel& model,
        QObject *parent):
    NamedObject{"AreaPresenter", parent},
    m_model{model},
    m_view{view}
{
}

const id_type<AreaModel>& AreaPresenter::id() const
{
    return m_model.id();
}

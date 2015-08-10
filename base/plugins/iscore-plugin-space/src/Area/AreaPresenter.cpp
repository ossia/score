#include "AreaPresenter.hpp"
#include "AreaModel.hpp"
#include "AreaView.hpp"
#include <QGraphicsItem>
#include "src/Space/SpaceModel.hpp"

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

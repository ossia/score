#include "AreaPresenter.hpp"
#include "AreaModel.hpp"
#include "AreaView.hpp"
#include "src/Space/SpaceModel.hpp"

#include <Space/square_renderer.hpp>
AreaPresenter::AreaPresenter(const AreaModel& model,
                             QGraphicsItem *parentview,
                             QObject *parent):
    NamedObject{"AreaPresenter", parent},
    m_model{model},
    m_view{new AreaView{parentview}}
{
    qDebug() << Q_FUNC_INFO;
    connect(&m_model, &AreaModel::areaChanged,
            this, &AreaPresenter::on_areaChanged);

    m_view->setPos(0, 0);
    on_areaChanged();
}

const id_type<AreaModel>& AreaPresenter::id() const
{
    return m_model.id();
}

void AreaPresenter::update()
{
    m_view->updateRect(m_view->parentItem()->boundingRect());
}

// Il vaut mieux faire comme dans les courbes ou le curvepresenter s'occupe des segments....
void AreaPresenter::on_areaChanged()
{
    qDebug() << Q_FUNC_INFO;
    spacelib::square_renderer<QPointF, RectDevice> renderer;
    renderer.size = {800, 600};
    renderer.render(m_model.area(), m_model.space().space());

    m_view->rects = renderer.render_device.rects;
    m_view->update();
}

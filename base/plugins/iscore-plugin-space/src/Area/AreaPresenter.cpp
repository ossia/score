#include "AreaPresenter.hpp"
#include "AreaModel.hpp"
#include "AreaView.hpp"
#include "src/Space/SpaceModel.hpp"

#include <Space/square_renderer.hpp>
AreaPresenter::AreaPresenter(
        AreaView* view,
        const AreaModel& model,
        QObject *parent):
    NamedObject{"AreaPresenter", parent},
    m_model{model},
    m_view{view}
{
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
    spacelib::square_renderer<QPointF, RectDevice> renderer;
    renderer.size = {800, 600};

    // Convert our dynamic space to a static one for rendering
    renderer.render(m_model.valuedArea(), spacelib::toStaticSpace<2>(m_model.space().space()));

    m_view->rects = renderer.render_device.rects;
    m_view->update();
}

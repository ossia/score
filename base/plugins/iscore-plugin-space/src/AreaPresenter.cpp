#include "AreaPresenter.hpp"
#include "AreaModel.hpp"
#include "AreaView.hpp"
#include "SpaceModel.hpp"

#include <Space/square_renderer.hpp>
AreaPresenter::AreaPresenter(const AreaModel& model,
                             QGraphicsItem *parentview,
                             QObject *parent):
    NamedObject{"AreaPresenter", parent},
    m_model{model},
    m_view{new AreaView{parentview}}
{
    connect(&m_model, &AreaModel::areaChanged,
            this, &AreaPresenter::on_areaChanged);
}

// Il vaut mieux faire comme dans les courbes ou le curvepresenter s'occupe des segments....
void AreaPresenter::on_areaChanged()
{
    spacelib::square_renderer<QPointF, PainterPathDevice> renderer;
    renderer.size = {800, 600};
    renderer.render(m_model.area(), m_model.space().space());

    m_view->rects = renderer.render_device.rects;
    m_view->update();

}

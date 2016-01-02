#include "GenericAreaPresenter.hpp"
#include "GenericAreaView.hpp"
#include "GenericAreaModel.hpp"

#include "src/Space/SpaceModel.hpp"
#include <Space/square_renderer.hpp>

namespace Space
{
GenericAreaPresenter::GenericAreaPresenter(
        GenericAreaView* view,
        const GenericAreaModel& model,
        QObject* parent):
    AreaPresenter{view, model, parent}
{
    this->view(this).setPos(0, 0);
}

void GenericAreaPresenter::update()
{
    view(this).updateRect(view(this).parentItem()->boundingRect());
}

// Il vaut mieux faire comme dans les courbes ou le curvepresenter s'occupe des segments....
void GenericAreaPresenter::on_areaChanged(ValMap map)
{
    spacelib::square_renderer<QPointF, RectDevice> renderer;
    renderer.size = {800, 600};

    // Convert our dynamic space to a static one for rendering
    ISCORE_TODO; //renderer.render(model(this).valuedArea(map), spacelib::toStaticSpace<2>(model(this).space().space()));

    view(this).rects = renderer.render_device.rects;
    view(this).update();
}
}

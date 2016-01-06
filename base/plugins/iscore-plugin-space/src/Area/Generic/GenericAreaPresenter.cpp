#include "GenericAreaPresenter.hpp"
#include "GenericAreaView.hpp"
#include "GenericAreaModel.hpp"

#include "src/Space/SpaceModel.hpp"

namespace Space
{
GenericAreaPresenter::GenericAreaPresenter(
        GenericAreaView* v,
        const GenericAreaModel& model,
        QObject* parent):
    AreaPresenter{v, model, parent}
{
    this->view(this).setPos(0, 0);

    m_cp = new AreaComputer{model.formula()};
    connect(m_cp, &AreaComputer::ready,
            this, [&] (auto vec) {
        view(this).rects = vec;
        view(this).update();
    }, Qt::QueuedConnection);

    connect(this, &GenericAreaPresenter::startCompute,
            m_cp, &AreaComputer::computeArea,
            Qt::QueuedConnection);
}

GenericAreaPresenter::~GenericAreaPresenter()
{
    m_cp->deleteLater();
}

void GenericAreaPresenter::update()
{
    view(this).updateRect(view(this).parentItem()->boundingRect());
}

// Il vaut mieux faire comme dans les courbes ou le curvepresenter s'occupe des segments....
void GenericAreaPresenter::on_areaChanged(ValMap map)
{
    emit startCompute(
                model(this).spaceMapping(),
                map);

}
}

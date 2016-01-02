#include "PointerAreaPresenter.hpp"
#include "PointerAreaModel.hpp"
#include "PointerAreaView.hpp"


namespace Space
{
PointerAreaPresenter::PointerAreaPresenter(
        PointerAreaView *view,
        const PointerAreaModel &model,
        QObject *parent):
    AreaPresenter{view, model, parent}
{
}

void PointerAreaPresenter::on_areaChanged(ValMap mapping)
{
    view(this).update(
                mapping.at("x0"),
                mapping.at("y0"));
}


void PointerAreaPresenter::update()
{
    ((QGraphicsItem&)view(this)).update();
}
}

#include "CircleAreaPresenter.hpp"
#include "CircleAreaModel.hpp"
#include "CircleAreaView.hpp"


namespace Space
{
CircleAreaPresenter::CircleAreaPresenter(
        CircleAreaView *view,
        const CircleAreaModel &model,
        QObject *parent):
    AreaPresenter{view, model, parent}
{
}

void CircleAreaPresenter::on_areaChanged(ValMap mapping)
{
    view(this).update(
                mapping.at("x0"),
                mapping.at("y0"),
                mapping.at("r"));
}


void CircleAreaPresenter::update()
{
    ((QGraphicsItem&)view(this)).update();
}
}

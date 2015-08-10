#include "CircleAreaPresenter.hpp"
#include "CircleAreaModel.hpp"
#include "CircleAreaView.hpp"

CircleAreaPresenter::CircleAreaPresenter(CircleAreaView *view, const CircleAreaModel &model, QObject *parent):
    AreaPresenter{view, model, parent}
{

}

void CircleAreaPresenter::on_areaChanged()
{
    const AreaModel::ParameterMap& pm = model(this).parameterMapping();
    auto x0 = pm["x0"].second.value.val.toDouble();
    auto y0 = pm["y0"].second.value.val.toDouble();
    auto r = pm["r"].second.value.val.toDouble();

    view(this).update(x0, y0, r);
}


void CircleAreaPresenter::update()
{
    ((QGraphicsItem&)view(this)).update();
}

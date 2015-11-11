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
    auto x0 = iscore::convert::value<double>(pm["x0"].second.value);
    auto y0 = iscore::convert::value<double>(pm["y0"].second.value);
    auto r = iscore::convert::value<double>(pm["r"].second.value);

    view(this).update(x0, y0, r);
}


void CircleAreaPresenter::update()
{
    ((QGraphicsItem&)view(this)).update();
}

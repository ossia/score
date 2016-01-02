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

void CircleAreaPresenter::on_areaChanged(GiNaC::exmap mapping)
{
    const CircleAreaModel& m = model(this);
    const auto& pm = m.parameterMapping();
    view(this).update(
                GiNaC::ex_to<GiNaC::numeric>(mapping.at(pm["x0"].first)).to_double(),
            GiNaC::ex_to<GiNaC::numeric>(mapping.at(pm["y0"].first)).to_double(),
            GiNaC::ex_to<GiNaC::numeric>(mapping.at(pm["r"].first)).to_double());
}


void CircleAreaPresenter::update()
{
    ((QGraphicsItem&)view(this)).update();
}
}

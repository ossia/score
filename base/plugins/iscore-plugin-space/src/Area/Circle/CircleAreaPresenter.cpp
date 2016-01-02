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
    auto x0_it = pm.find("x0");
    auto y0_it = pm.find("y0");
    auto r_it = pm.find("r");

    if(x0_it != pm.end()
    && y0_it != pm.end()
    && r_it != pm.end())
    {
        view(this).update(
                GiNaC::ex_to<GiNaC::numeric>(mapping.at((*x0_it).first)).to_double(),
                GiNaC::ex_to<GiNaC::numeric>(mapping.at((*y0_it).first)).to_double(),
                GiNaC::ex_to<GiNaC::numeric>(mapping.at((*r_it).first)).to_double());
    }
}


void CircleAreaPresenter::update()
{
    ((QGraphicsItem&)view(this)).update();
}
}

#include "PointerAreaPresenter.hpp"
#include "PointerAreaModel.hpp"
#include "PointerAreaView.hpp"

PointerAreaPresenter::PointerAreaPresenter(
        PointerAreaView *view,
        const PointerAreaModel &model,
        QObject *parent):
    AreaPresenter{view, model, parent}
{
}

void PointerAreaPresenter::on_areaChanged(GiNaC::exmap mapping)
{
    const PointerAreaModel& m = model(this);
    const auto& pm = m.parameterMapping();
    view(this).update(
            GiNaC::ex_to<GiNaC::numeric>(mapping.at(pm["x0"].first)).to_double(),
            GiNaC::ex_to<GiNaC::numeric>(mapping.at(pm["y0"].first)).to_double());
}


void PointerAreaPresenter::update()
{
    ((QGraphicsItem&)view(this)).update();
}

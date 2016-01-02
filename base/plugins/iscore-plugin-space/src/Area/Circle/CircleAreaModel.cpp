#include "CircleAreaModel.hpp"
#include "CircleAreaPresenter.hpp"
#include "CircleAreaView.hpp"
#include "src/Area/AreaParser.hpp"
#include "src/Space/SpaceModel.hpp"

namespace Space
{

const AreaFactoryKey&CircleAreaModel::static_factoryKey()
{
    static const AreaFactoryKey name{"Circle"};
    return name;
}

const AreaFactoryKey&CircleAreaModel::factoryKey() const
{
    return static_factoryKey();
}

QString CircleAreaModel::prettyName() const
{
    return tr("Circle");
}

QStringList CircleAreaModel::formula()
{
    return {"(xv-x0)^2 + (yv-y0)^2 <= r^2"};
}

CircleAreaModel::CircleAreaModel(
        const Space::AreaContext &space,
        const Id<AreaModel> &id,
        QObject *parent):
    AreaModel{AreaParser{formula()}.result(), space, id, parent}
{
    /*
    const auto& space_vars = this->space().space().variables();
    const auto& syms = area().symbols();
    GiNaC::symbol xv, x0, yv, y0, r;
    for(decltype(syms.size()) i = 0; i < syms.size(); i++)
    {
             if(syms[i].get_name() == "xv") xv = syms[i];
        else if(syms[i].get_name() == "yv") yv = syms[i];
        else if(syms[i].get_name() == "x0") x0 = syms[i];
        else if(syms[i].get_name() == "y0") y0 = syms[i];
        else if(syms[i].get_name() == "r") r = syms[i];
        else ISCORE_ABORT;
    }
    setSpaceMapping({{xv, space_vars[0].symbol()}, // xv -> x
                     {yv, space_vars[1].symbol()}}); // yv -> y

    Device::FullAddressSettings x0_val, y0_val, r_val;
    x0_val.value = State::Value::fromVariant(200);
    y0_val.value = State::Value::fromVariant(200);
    r_val.value = State::Value::fromVariant(100);
    setParameterMapping({
                    {x0.get_name().c_str(), {x0, x0_val}},
                    {y0.get_name().c_str(), {y0, y0_val}},
                    {r.get_name().c_str(), {r, r_val}},
                });
                */
}

AreaPresenter *CircleAreaModel::makePresenter(QGraphicsItem * parentitem, QObject * obj) const
{
    auto pres = new CircleAreaPresenter{new CircleAreaView{parentitem}, *this, obj};
    return pres;
}

}

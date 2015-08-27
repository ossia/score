#include "SpaceProcess.hpp"
#include "SpaceLayerModel.hpp"
#include "Area/AreaParser.hpp"
#include "Space/SpaceModel.hpp"
#include "Area/Circle/CircleAreaModel.hpp"

SpaceProcess::SpaceProcess(const id_type<Process> &id, QObject *parent):
    Process{id, "SpaceProcessModel", parent}
{
    using namespace GiNaC;
    using namespace spacelib;

    m_space = new SpaceModel{
            id_type<SpaceModel>(0),
            this};

    auto x_dim = new DimensionModel{"x", id_type<DimensionModel>{0}, m_space};
    auto y_dim = new DimensionModel{"y", id_type<DimensionModel>{1}, m_space};

    m_space->addDimension(x_dim);
    m_space->addDimension(y_dim);

    auto vp = new ViewportModel{id_type<ViewportModel>{0}, m_space};
    m_space->addViewport(vp);

    /*
    const auto& space_vars = m_space->space().variables();
    {
        AreaParser circleParser("(xv-x0)^2 + (yv-y0)^2 <= r^2");

        auto ar1 = new AreaModel(circleParser.result(),
                                 *m_space, id_type<AreaModel>(0), this);
        const auto& syms = ar1->area().symbols();

        ar1->setSpaceMapping({{syms[0], space_vars[0].symbol()},
                              {syms[2], space_vars[1].symbol()}});

        iscore::FullAddressSettings x0, y0, r;
        x0.value = iscore::Value::fromVariant(200);
        y0.value = iscore::Value::fromVariant(200);
        r.value = iscore::Value::fromVariant(100);
        ar1->setParameterMapping({
                        {syms[1].get_name().c_str(), {syms[1], x0}},
                        {syms[3].get_name().c_str(), {syms[3], y0}},
                        {syms[4].get_name().c_str(), {syms[4], r}},
                    });

        addArea(ar1);
    }

    {
        AreaParser parser("xv + yv >= c");

        auto ar2 = new AreaModel(parser.result(), *m_space, id_type<AreaModel>(1), this);
        const auto& syms = ar2->area().symbols();

        ar2->setSpaceMapping({{syms[0], space_vars[0].symbol()},
                              {syms[1], space_vars[1].symbol()}});


        iscore::FullAddressSettings c;
        c.value = iscore::Value::fromVariant(300);

        ar2->setParameterMapping({
                        {syms[2].get_name().c_str(), {syms[2], c}}
                    });

        addArea(ar2);
    }

    {
        addArea(new CircleAreaModel(*m_space,id_type<AreaModel>(2), this));
    }
    */
}

Process *SpaceProcess::clone(const id_type<Process> &newId, QObject *newParent) const
{
    return new SpaceProcess{newId, newParent};
}

QString SpaceProcess::processName() const
{
    return "Space";
}

void SpaceProcess::setDurationAndScale(const TimeValue &newDuration)
{
    ISCORE_TODO;
}

void SpaceProcess::setDurationAndGrow(const TimeValue &newDuration)
{
    ISCORE_TODO;
}

void SpaceProcess::setDurationAndShrink(const TimeValue &newDuration)
{
    ISCORE_TODO;
}

void SpaceProcess::reset()
{
    ISCORE_TODO;
}

DynamicStateDataInterface *SpaceProcess::startState() const
{
    ISCORE_TODO;
    return nullptr;
}

DynamicStateDataInterface *SpaceProcess::endState() const
{
    ISCORE_TODO;
    return nullptr;
}

Selection SpaceProcess::selectableChildren() const
{
    ISCORE_TODO;
    return {};
}

Selection SpaceProcess::selectedChildren() const
{
    ISCORE_TODO;
    return {};
}

void SpaceProcess::setSelection(const Selection &s) const
{
    ISCORE_TODO;
}

void SpaceProcess::serialize(const VisitorVariant &vis) const
{
    ISCORE_TODO;
}

void SpaceProcess::addArea(AreaModel* a)
{
    m_areas.insert(a);

    emit areaAdded(*a);
}

void SpaceProcess::removeArea(const id_type<AreaModel> &id)
{
    ISCORE_TODO;

}

LayerModel *SpaceProcess::makeLayer_impl(const id_type<LayerModel> &viewModelId, const QByteArray &constructionData, QObject *parent)
{
    return new SpaceLayerModel{viewModelId, *this, parent};
}

LayerModel *SpaceProcess::loadLayer_impl(const VisitorVariant &, QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}

LayerModel *SpaceProcess::cloneLayer_impl(const id_type<LayerModel> &newId, const LayerModel &source, QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}


void SpaceProcess::addComputation(ComputationModel * c)
{
    m_computations.insert(c);

    emit computationAdded(*c);
}

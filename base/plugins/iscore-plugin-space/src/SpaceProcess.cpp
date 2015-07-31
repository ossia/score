#include "SpaceProcess.hpp"
#include "SpaceLayerModel.hpp"

#include "Space/SpaceModel.hpp"


SpaceProcess::SpaceProcess(const id_type<Process> &id, QObject *parent):
    Process{id, "SpaceProcessModel", parent}
{
    using namespace GiNaC;
    using namespace spacelib;

    m_space = new SpaceModel(
                std::make_unique<spacelib::space<2>>(symbol("x"), symbol("y")),
                id_type<SpaceModel>(),
                this);

    symbol xv("xv");
    symbol yv("yv");
    symbol x0("x0");
    symbol y0("y0");
    symbol r("r");
    auto ar1 = new AreaModel(std::make_unique<spacelib::area>(
                                 pow((xv - x0),2) + pow((yv - y0),2) <= pow(r,2),
                                 std::vector<GiNaC::symbol>{xv, yv, x0, y0, r}/*,
                                 GiNaC::exmap{{x0, numeric(400)}, {y0, numeric(400)}, {r, 100}}*/),
                             *m_space, id_type<AreaModel>(0), this);

    ar1->setSpaceMapping({{xv, m_space->space().variables()[0]},
                          {yv, m_space->space().variables()[1]}});
    ar1->mapValueToParameter("x0", iscore::Value::fromVariant(200));
    ar1->mapValueToParameter("y0", iscore::Value::fromVariant(200));
    ar1->mapValueToParameter("r", iscore::Value::fromVariant(100));

    addArea(ar1);

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

ProcessStateDataInterface *SpaceProcess::startState() const
{
    ISCORE_TODO;
    return nullptr;
}

ProcessStateDataInterface *SpaceProcess::endState() const
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

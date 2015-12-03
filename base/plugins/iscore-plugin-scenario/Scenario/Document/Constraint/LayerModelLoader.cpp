#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <boost/optional/optional.hpp>
#include <QJsonObject>
#include <algorithm>

#include "LayerModelLoader.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

template <typename VisitorType> class Visitor;

template<>
LayerModel* createLayerModel(Deserializer<DataStream>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent)
{
    Id<Process> sharedProcessId;
    deserializer.m_stream >> sharedProcessId;

    auto& process = constraint.processes.at(sharedProcessId);
    auto viewmodel = process.loadLayer(deserializer.toVariant(),
                                       parent);

    deserializer.checkDelimiter();

    return viewmodel;
}


template<>
LayerModel* createLayerModel(
        Deserializer<JSONObject>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent)
{
    auto& process = constraint.processes.at(
                fromJsonValue<Id<Process>>(deserializer.m_obj["SharedProcessId"]));
    auto viewmodel = process.loadLayer(deserializer.toVariant(),
                                            parent);

    return viewmodel;
}

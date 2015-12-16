#pragma once
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/selection/Selection.hpp>
#include <QVector>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class ConstraintModel;
class DataStream;
class JSONObject;
class QObject;
class TimeNodeModel;
#include <iscore/tools/SettableIdentifier.hpp>

class BaseScenario final : public IdentifiedObject<BaseScenario>, public BaseScenarioContainer
{
        ISCORE_SERIALIZE_FRIENDS(BaseScenario, DataStream)
        ISCORE_SERIALIZE_FRIENDS(BaseScenario, JSONObject)

    public:
        iscore::ElementPluginModelList pluginModelList;

        BaseScenario(const Id<BaseScenario>& id, QObject* parentObject);

        template<typename DeserializerVisitor,
                 enable_if_deserializer<DeserializerVisitor>* = nullptr>
        BaseScenario(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject{vis, parent},
            BaseScenarioContainer{this}
        {
            vis.writeTo(*this);
        }

        Selection selectedChildren() const;

        using BaseScenarioContainer::event;
        using QObject::event;
};

const QVector<Id<ConstraintModel>> constraintsBeforeTimeNode(
        const BaseScenario&,
        const Id<TimeNodeModel>& timeNodeId);


#pragma once
#include <iscore/tools/Metadata.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/selection/Selection.hpp>
#include <QVector>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class DataStream;
class JSONObject;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
namespace Scenario
{
class ConstraintModel;
class TimeNodeModel;
class BaseScenario final :
        public IdentifiedObject<BaseScenario>,
        public BaseScenarioContainer
{
        ISCORE_SERIALIZE_FRIENDS(Scenario::BaseScenario, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Scenario::BaseScenario, JSONObject)

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
}

DEFAULT_MODEL_METADATA(Scenario::BaseScenario, "Base Scenario")
UNDO_NAME_METADATA(EMPTY_MACRO, Scenario::BaseScenario, "Base Scenario")

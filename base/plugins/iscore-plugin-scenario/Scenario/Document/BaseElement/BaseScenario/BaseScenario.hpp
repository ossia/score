#pragma once
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>

class BaseScenario final : public IdentifiedObject<BaseScenario>, public BaseScenarioContainer
{
        ISCORE_SERIALIZE_FRIENDS(BaseScenario, DataStream)
        ISCORE_SERIALIZE_FRIENDS(BaseScenario, JSONObject)

    public:
        iscore::ElementPluginModelList pluginModelList;

        BaseScenario(const Id<BaseScenario>& id, QObject* parent);

        template<typename DeserializerVisitor,
                 enable_if_deserializer<DeserializerVisitor>* = nullptr>
        BaseScenario(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject{vis, parent},
            BaseScenarioContainer{this}
        {
            vis.writeTo(*this);
        }

        Selection selectedChildren() const;
        static QString prettyName()
        {
            // For use in undo-redo menu
            return "BaseScenario";
        }
};

#pragma once
#include <QVector>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/Metadata.hpp>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/IdentifiedObject.hpp>

class DataStream;
class JSONObject;
class QObject;
#include <iscore/model/Identifier.hpp>
namespace Scenario
{
class ConstraintModel;
class TimeNodeModel;
class BaseScenario final : public IdentifiedObject<BaseScenario>,
                           public BaseScenarioContainer
{
  ISCORE_SERIALIZE_FRIENDS(Scenario::BaseScenario, DataStream)
  ISCORE_SERIALIZE_FRIENDS(Scenario::BaseScenario, JSONObject)

public:
  BaseScenario(const Id<BaseScenario>& id, QObject* parentObject);

  template <
      typename DeserializerVisitor,
      enable_if_deserializer<DeserializerVisitor>* = nullptr>
  BaseScenario(DeserializerVisitor&& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
      , BaseScenarioContainer{BaseScenarioContainer::no_init{}, this}
  {
    vis.writeTo(*this);
  }

  ~BaseScenario();

  Selection selectedChildren() const;
  bool focused() const;

  using BaseScenarioContainer::event;
  using QObject::event;
};

const QVector<Id<ConstraintModel>> constraintsBeforeTimeNode(
    const BaseScenario&, const Id<TimeNodeModel>& timeNodeId);
}

DEFAULT_MODEL_METADATA(Scenario::BaseScenario, "Base Scenario")
UNDO_NAME_METADATA(EMPTY_MACRO, Scenario::BaseScenario, "Base Scenario")

#pragma once
#include <QVector>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/Metadata.hpp>

#include <score/serialization/VisitorInterface.hpp>
#include <score/model/IdentifiedObject.hpp>

class DataStream;
class JSONObject;
class QObject;
#include <score/model/Identifier.hpp>
namespace Scenario
{
class IntervalModel;
class TimeSyncModel;
class BaseScenario final : public IdentifiedObject<BaseScenario>,
                           public BaseScenarioContainer
{
  Q_OBJECT
  SCORE_SERIALIZE_FRIENDS

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

const QVector<Id<IntervalModel>> intervalsBeforeTimeSync(
    const BaseScenario&, const Id<TimeSyncModel>& timeSyncId);
}

DEFAULT_MODEL_METADATA(Scenario::BaseScenario, "Base Scenario")
UNDO_NAME_METADATA(EMPTY_MACRO, Scenario::BaseScenario, "Base Scenario")

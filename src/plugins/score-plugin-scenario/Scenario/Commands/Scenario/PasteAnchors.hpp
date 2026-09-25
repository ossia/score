#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/ObjectPath.hpp>

#include <score/serialization/JSONVisitor.hpp>

#include <score_plugin_scenario_export.h>

#include <utility>
#include <vector>

class QObject;
namespace score
{
struct DocumentContext;
}

namespace Scenario
{
//! Paths of the copied objects and of their copies
struct CopiedPaths
{
  std::vector<std::pair<ObjectPath, ObjectPath>> moved;
  bool sameDocument{};
};

//! Compares the copy's "OriginDocument" key with score::IDocument::copyOrigin()
SCORE_PLUGIN_SCENARIO_EXPORT
bool copiedFromHere(const rapidjson::Value& copy, const score::DocumentContext& ctx);

SCORE_PLUGIN_SCENARIO_EXPORT
bool hasAnchors(const rapidjson::Value& copy);

//! Anchors to a copied object move to its copy; others are kept only when
//! pasting in the same document.
SCORE_PLUGIN_SCENARIO_EXPORT
void remapCopiedAnchors(QObject& root, const CopiedPaths& paths);

//! The copies are published under their own names
SCORE_PLUGIN_SCENARIO_EXPORT
void followPasted(const score::DocumentContext& ctx, const std::vector<QObject*>& pasted);

namespace Command
{
//! Goes last in a paste macro, once all the pasted objects exist
class SCORE_PLUGIN_SCENARIO_EXPORT FollowPasted final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), FollowPasted, "Follow pasted objects")
public:
  explicit FollowPasted(std::vector<ObjectPath> pasted);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  std::vector<ObjectPath> m_pasted;
};
}
}

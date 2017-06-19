#pragma once
/*
#include <QMap>
#include <QString>
#include <iscore/command/Command.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace PluginSettings
{
class BlacklistCommand : public iscore::Command
{
  // QUndoCommand interface
public:
  BlacklistCommand(QString name, bool value);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;
  // bool mergeWith(const Command* other) override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

  QMap<QString, bool> m_blacklistedState;

  // Command interface
public:
  const CommandParentFactoryKey& parentKey() const noexcept override;
  const CommandFactoryKey& key() const noexcept override;
  QString description() const override;
};
}
*/

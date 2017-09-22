#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariant>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

namespace score
{
/**
 * @brief The PropertyCommand class
 *
 * This generic command allows for a very simple operation when
 * changing a property specified with Q_PROPERTY.
 *
 * It will save the current state and switch between the current and new
 * state upon undo / redo.
 */
class SCORE_LIB_BASE_EXPORT PropertyCommand : public Command
{
public:
  using Command::Command;
  PropertyCommand() = default;

  template <typename T, typename... Args>
  PropertyCommand(const T& obj, QString property, QVariant newval)
      : m_path{Path<T>(obj).unsafePath()}
      , m_property{std::move(property)}
      , m_old{obj.property(m_property.toUtf8().constData())}
      , m_new{std::move(newval)}
  {
  }

  virtual ~PropertyCommand();

  void undo(const score::DocumentContext& ctx) const final override;
  void redo(const score::DocumentContext& ctx) const final override;

  template <typename Path_T>
  void update(const Path_T&, QVariant newval)
  {
    m_new = std::move(newval);
  }

protected:
  void serializeImpl(DataStreamInput&) const final override;
  void deserializeImpl(DataStreamOutput&) final override;

private:
  ObjectPath m_path;
  QString m_property;
  QVariant m_old, m_new;
};
}

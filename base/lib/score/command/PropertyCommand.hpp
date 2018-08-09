#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariant>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

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

  ~PropertyCommand() override;

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

template <typename T>
class PropertyCommand_T : public score::Command
{
public:
  using model_t = typename T::model_type;
  using param_t = typename T::param_type;

  template<typename U>
  struct command;

  using score::Command::Command;
  PropertyCommand_T() = default;

  template<typename U>
  PropertyCommand_T(const model_t& obj, U&& newval)
      : m_path{obj}
      , m_old{(obj.*T::get())()}
      , m_new{std::forward<U>(newval)}
  {
  }

  ~PropertyCommand_T() override
  {

  }

  template <typename Path_T, typename U>
  void update(const Path_T&, U&& newval)
  {
    m_new = std::forward<U>(newval);
  }

  void undo(const score::DocumentContext& ctx) const final override
  {
    (m_path.find(ctx).*T::set())(m_old);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    (m_path.find(ctx).*T::set())(m_new);
  }

private:
  void serializeImpl(DataStreamInput& s) const final override
  {
    s << m_path << m_old << m_new;
  }

  void deserializeImpl(DataStreamOutput& s) final override
  {
    s >> m_path >> m_old >> m_new;
  }

  Path<model_t> m_path;
  param_t m_old, m_new;
};
}

#define PROPERTY_COMMAND_T(NS, Name, Property, Description)    \
namespace NS {                                                 \
class Name final :                                             \
    public score::PropertyCommand_T<Property>                  \
{                                                              \
  SCORE_COMMAND_DECL(NS::CommandFactoryName(), Name, Description)  \
public:                                                        \
  using PropertyCommand_T::PropertyCommand_T;                  \
};                                                             \
}                                                              \
                                                               \
namespace score {                                              \
template<>                                                     \
template<>                                                     \
struct score::PropertyCommand_T<NS::Property>::command<void> {     \
  using type = NS::Name;                                           \
};                                                             \
}




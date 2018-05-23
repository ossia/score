#pragma once
#include <boost/call_traits.hpp>
#include <score/command/Command.hpp>
namespace score
{
/**
 * @brief Base class for commands to be used with the Settings system.
 *
 * It should not be necessary to use these classes in user code.
 * Instead, use the macro \ref SCORE_SETTINGS_COMMAND or \ref
 * SCORE_SETTINGS_DEFERRED_COMMAND
 */
class SCORE_LIB_BASE_EXPORT SettingsCommandBase
{
public:
  virtual ~SettingsCommandBase();
  virtual void undo() const = 0;
  virtual void redo() const = 0;
};

template <typename T>
/**
 * @brief A Command class that modifies a parameter given its trait class.
 *
 * This is used to have a very fast application of many settings.
 * @see score::SettingsParameterMetadata
 */
class SettingsCommand : public SettingsCommandBase
{
public:
  using model_t = typename T::model_type;
  using parameter_t = T;
  using parameter_pass_t =
      typename boost::call_traits<typename T::param_type>::param_type;
  SettingsCommand() = default;
  SettingsCommand(model_t& obj, parameter_pass_t newval)
      : m_model{obj}, m_new{newval}
  {
    m_old = (m_model.*T::get())();
  }

  virtual ~SettingsCommand() = default;

  void undo() const final override
  {
    (m_model.*T::set())(m_old);
  }

  void redo() const final override
  {
    (m_model.*T::set())(m_new);
  }

  void update(model_t&, parameter_pass_t newval)
  {
    m_new = newval;
  }

private:
  model_t& m_model;
  typename T::param_type m_old, m_new;
};
}

/**
 * \macro SCORE_SETTINGS_COMMAND_DECL
 * \brief Content of a Settings command.
 */
#define SCORE_SETTINGS_COMMAND_DECL(name)                     \
public:                                                       \
  using score::SettingsCommand<parameter_t>::SettingsCommand; \
  name() = default;                                           \
  static const CommandKey& static_key()                       \
  {                                                           \
    static const CommandKey var{#name};                       \
    return var;                                               \
  }                                                           \
                                                              \
private:

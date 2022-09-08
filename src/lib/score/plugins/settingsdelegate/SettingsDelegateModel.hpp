#pragma once
#include <score/command/SettingsCommand.hpp>

#include <ossia/detail/for_each_in_tuple.hpp>

#include <QObject>
#include <QSettings>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT SettingsDelegateModel : public QObject
{
public:
  using QObject::QObject;
  virtual ~SettingsDelegateModel();
};

template <typename Parameter>
struct SettingsParameterMetadata
{
public:
  using parameter_type = Parameter;
  using model_type = typename Parameter::model_type;
  using data_type = typename Parameter::param_type;
  using argument_type = typename boost::call_traits<data_type>::param_type;
  QString key;
  data_type def;
};

template <typename T>
using sp = SettingsParameterMetadata<T>;

#if !defined(__EMSCRIPTEN__)
template <typename T, typename Model>
void setupDefaultSettings(QSettings& set, const T& tuple, Model& model)
{
  ossia::for_each_in_tuple(tuple, [&](auto& e) {
    using type = std::remove_reference_t<decltype(e)>;
    using data_type = typename type::data_type;
    using param_type = typename type::parameter_type;

    // If we cannot find the key, it means that it's a new setting.
    // Hence we set the default value both in the QSettings and in the model.
    if(!set.contains(e.key))
    {
      set.setValue(e.key, QVariant::fromValue(e.def));
      (model.*param_type::init)(e.def);
    }
    else
    {
      // We fetch the value from the settings.
      auto val = set.value(e.key).template value<data_type>();
      (model.*param_type::init)(val);
    }
  });
}
#else
template <typename T, typename Model>
void setupDefaultSettings(QSettings&, const T& tuple, Model& model)
{
  ossia::for_each_in_tuple(tuple, [&](auto& e) {
    using type = std::remove_reference_t<decltype(e)>;
    using data_type = typename type::data_type;
    using param_type = typename type::parameter_type;
    (model.*param_type::set)(e.def);
  });
}
#endif
}

#define SETTINGS_PARAMETER_IMPL(Name) const score::sp<Model::p_##Name> Name

#define SCORE_SETTINGS_COMMAND(ModelType, Name)            \
  struct Set##ModelType##Name final                        \
      : public score::SettingsCommand<ModelType::p_##Name> \
  {                                                        \
    static constexpr const bool is_deferred = false;       \
    SCORE_SETTINGS_COMMAND_DECL(Set##ModelType##Name)      \
  };

#define SCORE_SETTINGS_PARAMETER(ModelType, Name) SCORE_SETTINGS_COMMAND(ModelType, Name)

#define SCORE_SETTINGS_DEFERRED_COMMAND(ModelType, Name)   \
  struct Set##ModelType##Name final                        \
      : public score::SettingsCommand<ModelType::p_##Name> \
  {                                                        \
    static constexpr const bool is_deferred = true;        \
    SCORE_SETTINGS_COMMAND_DECL(Set##ModelType##Name)      \
  };

#define SCORE_SETTINGS_DEFERRED_PARAMETER(ModelType, Name) \
  SCORE_SETTINGS_DEFERRED_COMMAND(ModelType, Name)

#define SCORE_SETTINGS_PROPERTY(Type, Name)                                 \
  W_PROPERTY(                                                               \
      Type, Name, &W_ThisType::get##Name, &W_ThisType::set##Name, W_Notify, \
      &W_ThisType::Name##Changed)                                           \
  struct p_##Name                                                           \
  {                                                                         \
    using param_type = Type;                                                \
    using model_type = W_ThisType;                                          \
    static const constexpr auto name = #Name;                               \
    static const constexpr auto get = &W_ThisType::get##Name;               \
    static const constexpr auto set = &W_ThisType::set##Name;               \
    static const constexpr auto notify = &W_ThisType::Name##Changed;        \
    static const constexpr auto init = &W_ThisType::init##Name;             \
  };

#define SCORE_SETTINGS_PARAMETER_HPP(Export, Type, Name)                         \
public:                                                                          \
  Type get##Name() const;                                                        \
  void init##Name(Type);                                                         \
  void set##Name(Type);                                                          \
  void Name##Changed(Type arg) E_SIGNAL2(Export##S, Export, Name##Changed, arg); \
  SCORE_SETTINGS_PROPERTY(Type, Name)                                            \
private:

#if !defined(__EMSCRIPTEN__)
#define SCORE_SETTINGS_PARAMETER_CPP(Type, ModelType, Name)          \
  Type ModelType::get##Name() const                                  \
  {                                                                  \
    return m_##Name;                                                 \
  }                                                                  \
                                                                     \
  void ModelType::init##Name(Type val)                               \
  {                                                                  \
    m_##Name = val;                                                  \
                                                                     \
    Name##Changed(val);                                              \
  }                                                                  \
  void ModelType::set##Name(Type val)                                \
  {                                                                  \
    if(val == m_##Name)                                              \
      return;                                                        \
                                                                     \
    m_##Name = val;                                                  \
                                                                     \
    QSettings s;                                                     \
    s.setValue(Parameters::Name.key, QVariant::fromValue(m_##Name)); \
    Name##Changed(val);                                              \
  }
#else
#define SCORE_SETTINGS_PARAMETER_CPP(Type, ModelType, Name) \
  Type ModelType::get##Name() const                         \
  {                                                         \
    return m_##Name;                                        \
  }                                                         \
                                                            \
  void ModelType::init##Name(Type val)                      \
  {                                                         \
    m_##Name = val;                                         \
    Name##Changed(val);                                     \
  }                                                         \
  void ModelType::set##Name(Type val)                       \
  {                                                         \
    if(val == m_##Name)                                     \
      return;                                               \
                                                            \
    init##Name(std::move(val));                             \
  }
#endif

#define SCORE_PROJECTSETTINGS_PARAMETER_CPP(Type, ModelType, Name) \
  Type ModelType::get##Name() const                                \
  {                                                                \
    return m_##Name;                                               \
  }                                                                \
                                                                   \
  void ModelType::init##Name(Type val)                             \
  {                                                                \
    m_##Name = val;                                                \
    Name##Changed(val);                                            \
  }                                                                \
  void ModelType::set##Name(Type val)                              \
  {                                                                \
    if(val == m_##Name)                                            \
      return;                                                      \
                                                                   \
    init##Name(std::move(val));                                    \
  }

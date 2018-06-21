#pragma once
#include <QJsonObject>
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score/tools/Version.hpp>
#include <score_lib_base_export.h>

namespace score
{
struct Plugin
{
};

using PluginKey = UuidKey<Plugin>;
class SCORE_LIB_BASE_EXPORT Plugin_QtInterface
{
public:
  virtual ~Plugin_QtInterface();

  virtual std::vector<PluginKey> required() const
  {
    return {};
  }

  virtual Version version() const
  {
    return Version{0};
  }

  virtual UuidKey<Plugin> key() const = 0;

  virtual void updateSaveFile(
      QJsonObject& obj, Version obj_version, Version current_version)
  {
  }
};
}

/**
 * \macro SCORE_PLUGIN_METADATA
 * \brief Macro for easy declaration of the key of a plug-in.
 */
#define SCORE_PLUGIN_METADATA(Ver, Uuid)                        \
public:                                                         \
  static Q_DECL_RELAXED_CONSTEXPR score::PluginKey static_key() \
  {                                                             \
    return_uuid(Uuid);                                          \
  }                                                             \
                                                                \
  score::PluginKey key() const final override                   \
  {                                                             \
    return static_key();                                        \
  }                                                             \
                                                                \
  score::Version version() const override                       \
  {                                                             \
    return score::Version{Ver};                                 \
  }                                                             \
                                                                \
private:

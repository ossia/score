#pragma once
#include <iscore/command/CommandFactoryKey.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
class Command;

/**
 * @brief Utility class for the serialization and deserialization of commands
 */
struct ISCORE_LIB_BASE_EXPORT CommandData
{
  CommandData() = default;
  explicit CommandData(const iscore::Command& cmd);

  CommandParentFactoryKey parentKey;
  CommandFactoryKey commandKey;
  QByteArray data;
};
}

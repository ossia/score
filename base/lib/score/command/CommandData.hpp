#pragma once
#include <score/command/CommandFactoryKey.hpp>
#include <score_lib_base_export.h>
namespace score
{
class Command;

/**
 * @brief Utility class for the serialization and deserialization of commands
 */
struct SCORE_LIB_BASE_EXPORT CommandData
{
  CommandData() = default;
  explicit CommandData(const score::Command& cmd);

  CommandGroupKey parentKey;
  CommandKey commandKey;
  QByteArray data;
};
}

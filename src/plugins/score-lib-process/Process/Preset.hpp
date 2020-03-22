#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QJsonObject>
#include <optional>
namespace Process
{
class ProcessFactoryList;

struct ProcessIdentifier
{
  UuidKey<Process::ProcessModel> key;
  QString effect;
  friend bool operator==(const ProcessIdentifier& lhs, const ProcessIdentifier& rhs) noexcept
  {
    return lhs.key == rhs.key && lhs.effect == rhs.effect;
  }
  friend bool operator!=(const ProcessIdentifier& lhs, const ProcessIdentifier& rhs) noexcept
  {
    return !(lhs == rhs);
  }
  friend bool operator<(const ProcessIdentifier& lhs, const ProcessIdentifier& rhs) noexcept
  {
    return lhs.key < rhs.key ||
        (lhs.key == rhs.key && lhs.effect < rhs.effect);
  }
};

struct SCORE_LIB_PROCESS_EXPORT Preset
{
  QString name;
  ProcessIdentifier key;
  QJsonObject data;

  static std::optional<Process::Preset> fromJson(
      const Process::ProcessFactoryList& procs,
      const QJsonObject& obj) noexcept;

  QJsonObject toJson() const noexcept;

  friend bool operator==(const Preset& lhs, const Preset& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.key == rhs.key && lhs.data == rhs.data;
  }
  friend bool operator!=(const Preset& lhs, const Preset& rhs) noexcept
  {
    return !(lhs == rhs);
  }
  friend bool operator<(const Preset& lhs, const Preset& rhs) noexcept
  {
    return lhs.key < rhs.key;
  }
};
}

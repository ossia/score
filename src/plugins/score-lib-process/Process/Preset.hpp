#pragma once
#include <Process/ProcessMetadata.hpp>

#include <optional>
#include <verdigris>
namespace Process
{
class ProcessFactoryList;

struct ProcessIdentifier
{
  UuidKey<Process::ProcessModel> key;
  QString effect;
  friend bool
  operator==(const ProcessIdentifier& lhs, const ProcessIdentifier& rhs) noexcept
  {
    return lhs.key == rhs.key && lhs.effect == rhs.effect;
  }
  friend bool
  operator!=(const ProcessIdentifier& lhs, const ProcessIdentifier& rhs) noexcept
  {
    return !(lhs == rhs);
  }
  friend bool
  operator<(const ProcessIdentifier& lhs, const ProcessIdentifier& rhs) noexcept
  {
    return lhs.key < rhs.key || (lhs.key == rhs.key && lhs.effect < rhs.effect);
  }
};

struct SCORE_LIB_PROCESS_EXPORT Preset
{
  QString name;
  ProcessIdentifier key;
  QByteArray data;

  // Optional hierarchical category used to group presets into submenus.
  // Empty means "root"; "Foo/Bar/Baz" nests the preset under Foo > Bar > Baz.
  QString category;

  static std::shared_ptr<Process::Preset>
  fromJson(const Process::ProcessFactoryList& procs, const QByteArray& obj) noexcept;

  QByteArray toJson() const noexcept;

  friend bool operator==(const Preset& lhs, const Preset& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.key == rhs.key && lhs.data == rhs.data
           && lhs.category == rhs.category;
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

Q_DECLARE_METATYPE(Process::Preset)
W_REGISTER_ARGTYPE(Process::Preset)
Q_DECLARE_METATYPE(std::optional<Process::Preset>)
W_REGISTER_ARGTYPE(std::optional<Process::Preset>)

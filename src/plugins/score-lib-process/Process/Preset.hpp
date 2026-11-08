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

  // Optional text shown as the preset's tooltip.
  QString description;

  // The file the library read it from or saved it to; empty for a preset in
  // no file (a plug-in's own, one being dragged). Not part of its contents:
  // neither saved nor compared.
  QString path;

  static std::shared_ptr<Process::Preset>
  fromJson(const Process::ProcessFactoryList& procs, const QByteArray& obj) noexcept;

  //! The preset stored in this file (.scp), with `path` set to it and its
  //! <PRESET>: paths resolved against its folder: what every preset read from
  //! disk goes through.
  static std::shared_ptr<Process::Preset>
  fromFile(const Process::ProcessFactoryList& procs, const QString& file) noexcept;

  QByteArray toJson() const noexcept;

  friend bool operator==(const Preset& lhs, const Preset& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.key == rhs.key && lhs.data == rhs.data
           && lhs.category == rhs.category && lhs.description == rhs.description;
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

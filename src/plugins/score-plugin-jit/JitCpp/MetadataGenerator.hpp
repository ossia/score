#pragma once
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringBuilder>

#include <vector>

namespace Jit
{

struct AddonData
{
  QJsonObject addon_info;
  std::string unity_cpp;
  std::vector<std::pair<QString, QString>> files;
};

//! Combines all the source files of an addon into a single unity file to make
//! build faster
static AddonData loadAddon(const QString& addon)
{
  AddonData data;
  if (QFile f(addon + "/addon.json"); f.open(QIODevice::ReadOnly))
    data.addon_info = QJsonDocument::fromJson(f.readAll()).object();

  std::string cpp_files;
  std::vector<std::pair<QString, QString>> files;
  QDirIterator it{addon,
                  {"*.cpp", "*.hpp"},
                  QDir::Filter::Files | QDir::Filter::NoDotAndDotDot,
                  QDirIterator::Subdirectories};

  while (it.hasNext())
  {
    if (QFile f(it.next()); f.open(QIODevice::ReadOnly))
    {
      QFileInfo fi{f};
      if (fi.suffix() == "cpp")
      {
        data.unity_cpp.append(
            "#include \"" + it.filePath().toStdString() + "\"\n");
      }

      data.files.push_back({fi.filePath(), f.readAll()});
    }
  }
  return data;
}

//! Generates the score_myaddon_commands.hpp and score_myaddon_command_list.hpp
//! files
static void generateCommandFiles(
    const QString& output,
    const QString& addon_path,
    const std::vector<std::pair<QString, QString>>& files)
{
  QRegularExpression decl(
      "SCORE_COMMAND_DECL\\([A-Za-z_0-9,:<>\r\n\t "
      "]*\\(\\)[A-Za-z_0-9,\"':<>\r\n\t ]*\\)");
  QRegularExpression decl_t(
      "SCORE_COMMAND_DECL_T\\([A-Za-z_0-9,:<>\r\n\t ]*\\)");

  QString includes;
  QString commands;
  for (const auto& f : files)
  {
    {
      auto res = decl.globalMatch(f.second);
      while (res.hasNext())
      {
        auto match = res.next();
        if (auto txt = match.capturedTexts(); !txt.empty())
        {
          if (auto split = txt[0].split(","); split.size() > 1)
          {
            auto filename = f.first;
            filename.remove(addon_path + "/");
            includes += "#include <" + filename + ">\n";
            commands += split[1] + ",\n";
          }
        }
      }
    }
  }
  commands.remove(commands.length() - 2, 2);
  commands.push_back("\n");
  QDir{}.mkpath(output);
  auto out_name = QFileInfo{addon_path}.fileName().replace("-", "_");
  {
    QFile cmd_f{output + "/" + out_name + "_commands_files.hpp"};
    cmd_f.open(QIODevice::WriteOnly);
    cmd_f.write(includes.toUtf8());
    cmd_f.close();
  }
  {
    QFile cmd_f{output + "/" + out_name + "_commands.hpp"};
    cmd_f.open(QIODevice::WriteOnly);
    cmd_f.write(commands.toUtf8());
    cmd_f.close();
  }
}

//! Generates a score_myaddon_export.h file suitable for a static build
static void generateExportFile(
    const QString& addon_files_path,
    const QString& addon_name,
    const QByteArray& addon_export)
{
  QFile export_file = QString{addon_files_path + "/" + addon_name + "_export.h"};
  export_file.open(QIODevice::WriteOnly);
  QByteArray export_data{
    "#ifndef " + addon_export + "_EXPORT_H\n"
    "#define " + addon_export + "_EXPORT_H\n"
        "#define " + addon_export + "_EXPORT __attribute__((visibility(\"default\")))\n"
        "#define " + addon_export + "_DEPRECATED [[deprecated]]\n"
    "#endif\n"
  };
  export_file.write(export_data);
  export_file.close();
}

//! Given an addon, generates all the files needed for the build of this addon
//! ; this is the same that CMake would generate into the addon's build dir.
static QString generateAddonFiles(
    QString addon_name,
    const QString& addon,
    const std::vector<std::pair<QString, QString>>& files)
{
  addon_name.replace("-", "_");
  QByteArray addon_export = addon_name.toUpper().toUtf8();

  QString addon_files_path
      = QDir::tempPath() + "/score-tmp-build/" + addon_name;
  QDir{}.mkpath(addon_files_path);
  generateExportFile(addon_files_path, addon_name, addon_export);
  generateCommandFiles(addon_files_path, addon, files);
  return addon_files_path;
}

}

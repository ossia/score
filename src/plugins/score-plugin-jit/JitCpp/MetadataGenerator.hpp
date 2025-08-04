#pragma once
#include <JitCpp/JitPlatform.hpp>

#include <score/tools/File.hpp>

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
  // Warning ! if ever changing that to QByteArray, look for mapFile usage as right now the file is read with mmap
  std::vector<std::pair<QString, QString>> files;
  std::vector<std::string> flags;
};

//! Combines all the source files of an addon into a single unity file to make
//! build faster
//!
static void loadBasicAddon(const QString& addon, AddonData& data)
{
  std::string cpp_files;
  std::vector<std::pair<QString, QString>> files;
  QDirIterator it{
      addon,
      {"*.cpp", "*.hpp"},
      QDir::Filter::Files | QDir::Filter::NoDotAndDotDot,
      QDirIterator::Subdirectories};

  while(it.hasNext())
  {
    if(QFile f(it.next()); f.open(QIODevice::ReadOnly))
    {
      QFileInfo fi{f};
      if(fi.suffix() == "cpp")
      {
        data.unity_cpp.append("#include \"" + it.filePath().toStdString() + "\"\n");
      }

      data.files.push_back({fi.filePath(), f.readAll()});
    }
  }
}

static void loadCMakeAddon(const QString& addon, AddonData& data, QString cm)
{
  static const QRegularExpression space{R"_(\s+)_"};
  static const QRegularExpression sourceFiles{
      R"_(add_library\(\s*[[:graph:]]+([a-zA-Z0-9_.\/\n ]*)\))_"};
  static const QRegularExpression definitions{
      R"_(target_compile_definitions\(\s*[[:graph:]]+\s*[[:graph:]]+([a-zA-Z0-9_"= ]+)\))_"};
  static const QRegularExpression includes{
      R"_(target_include_directories\(\s*[[:graph:]]+\s*[[:graph:]]+([a-zA-Z0-9_\/ ]+)\))_"};
  static const QRegularExpression avnd{
      R"_(avnd_score_plugin_add\(([a-zA-Z0-9_\/.\n ]+)\))_"};
  static const QRegularExpression avnd_finalize{
      R"_(avnd_score_plugin_finalize\(([a-zA-Z0-9_\/.\n" -]+)\))_"};

  auto avnds = avnd.globalMatch(cm);
  auto avnd_finalizes = avnd_finalize.globalMatch(cm);

  // Process classic CMake libraries
  auto files = sourceFiles.globalMatch(cm);
  auto defs = definitions.globalMatch(cm);
  auto incs = includes.globalMatch(cm);
  while(files.hasNext())
  {
    auto m = files.next();
    auto res = m.captured(1).replace('\n', ' ').split(space, Qt::SkipEmptyParts);
    for(const QString& file : res)
    {
      QString filename = QString{R"_(%1/%2)_"}.arg(addon).arg(file);

      QString path = QString{R"_(#include "%1/%2"
)_"}
                         .arg(addon)
                         .arg(file);
      data.unity_cpp.append(path.toStdString());

      QFile f{filename};
      f.open(QIODevice::ReadOnly);
      data.files.push_back({filename, score::readFileAsQString(f)});
    }
  }
  while(defs.hasNext())
  {
    auto m = defs.next();
    auto res = m.captured(1).replace('\n', ' ').split(space, Qt::SkipEmptyParts);
    for(const QString& define : res)
    {
      data.flags.push_back(QString{R"_(-D%1)_"}.arg(define).toStdString());
    }
  }

  while(incs.hasNext())
  {
    auto m = incs.next();
    auto res = m.captured(1).replace('\n', ' ').split(space, Qt::SkipEmptyParts);

    for(const QString& path : res)
    {
      data.flags.push_back(QString{R"_(-I%1/%2)_"}.arg(addon).arg(path).toStdString());
    }
  }

  // Process avendish wrappers
  struct avnd_plugin
  {
    QString base_target;
    QString main_class;
    QString target;
    QString avnd_namespace;
    struct source_file
    {
      QString path;
      QString filename;
      QString file;
    };
    std::vector<source_file> files;
  };

  struct avnd_plugin_group
  {
    QString base_target;
    QString version;
    QString uuid;
    std::vector<avnd_plugin> plugins;
  };

  std::vector<avnd_plugin_group> groups;
  qDebug() << "Found avnd finalize? " << avnd_finalizes.hasNext();
  while(avnd_finalizes.hasNext())
  {
    auto m = avnd_finalizes.next();
    auto res = m.captured(1).replace('\n', ' ').split(space, Qt::SkipEmptyParts);
    qDebug() << res;
    avnd_plugin_group plug;
    for(auto it = res.begin(); it != res.end(); ++it)
    {
      if(*it == "BASE_TARGET")
      {
        ++it;
        if(it != res.end())
          plug.base_target = *it;
      }
      else if(*it == "PLUGIN_VERSION")
      {
        ++it;
        if(it != res.end())
          plug.version = *it;
      }
      else if(*it == "PLUGIN_UUID")
      {
        ++it;
        if(it != res.end())
          plug.uuid = *it;
      }
    }

    if(!plug.base_target.isEmpty())
      groups.push_back(std::move(plug));
  }

  while(avnds.hasNext())
  {
    auto m = avnds.next();
    auto res = m.captured(1).replace('\n', ' ').split(space, Qt::SkipEmptyParts);
    avnd_plugin plug;
    for(auto it = res.begin(); it != res.end(); ++it)
    {
      if(*it == "BASE_TARGET")
      {
        ++it;
        if(it != res.end())
          plug.base_target = *it;
      }
      else if(*it == "SOURCES")
      {
        ++it;
        while(it != res.end() && QFile::exists(QString{"%1/%2"}.arg(addon).arg(*it)))
        {
          avnd_plugin::source_file sf;
          sf.filename = QString{"%1/%2"}.arg(addon).arg(*it);
          sf.path = QString{"#include \"%1/%2\"\n"}.arg(addon).arg(*it);

          QFile f{sf.filename};
          if(f.open(QIODevice::ReadOnly))
          {
            sf.file = score::readFileAsQString(f);
            data.files.push_back({sf.filename, sf.file});
            data.unity_cpp.append(sf.path.toStdString());
            plug.files.push_back(std::move(sf));
          }
          ++it;
        }

        --it;
      }
      else if(*it == "MAIN_CLASS")
      {
        ++it;
        if(it != res.end())
          plug.main_class = *it;
      }
      else if(*it == "TARGET")
      {
        ++it;
        if(it != res.end())
          plug.target = *it;
      }
      else if(*it == "NAMESPACE")
      {
        ++it;
        if(it != res.end())
          plug.avnd_namespace = *it;
      }
    }

    if(plug.files.empty())
    {
      qDebug() << "no files found";
      return;
    }

    for(auto& gp : groups)
    {
      if(plug.base_target == gp.base_target)
        gp.plugins.push_back(std::move(plug));
    }
  }

  auto sdk_location = locateSDKWithFallback();
  auto qsdk = QString::fromStdString(sdk_location.path);
  QString prototypes_folders;
  if(sdk_location.sdk_kind == located_sdk::official && sdk_location.deploying)
  {
    prototypes_folders = qsdk + "/lib/cmake/score";
  }
  else
  {
    prototypes_folders
        = QString::fromUtf8(SCORE_ROOT_SOURCE_DIR) + "/src/plugins/score-plugin-avnd/";
  }

  for(auto& gp : groups)
  {
    auto avnd_plugin_version = gp.version.toUtf8();
    auto avnd_plugin_uuid = gp.uuid.toUtf8();
    avnd_plugin_uuid.removeIf([](char c) { return c == '"'; });
    auto avnd_base_target = gp.base_target.toUtf8();

    QFile proto_file = QFile(prototypes_folders + "/prototype.cpp.in");
    proto_file.open(QIODevice::ReadOnly);
    auto proto = proto_file.readAll();

    QFile cpp_proto_file = QFile(prototypes_folders + "/plugin_prototype.cpp.in");
    cpp_proto_file.open(QIODevice::ReadOnly);
    auto cpp_proto = cpp_proto_file.readAll();

    QFile hpp_proto_file = QFile(prototypes_folders + "/plugin_prototype.hpp.in");
    hpp_proto_file.open(QIODevice::ReadOnly);
    auto hpp_proto = hpp_proto_file.readAll();

    QByteArray cpp_proto_replaced = cpp_proto;
    cpp_proto_replaced.replace(R"_(#include "@AVND_BASE_TARGET@.hpp")_", "");
    QByteArray hpp_proto_replaced = hpp_proto;

    QString avnd_additional_classes;
    QString avnd_custom_factories;
    QByteArray protos;
    for(auto& plug : gp.plugins)
    {
      QByteArray proto_replaced = proto;
      proto_replaced.replace(R"_(#cmakedefine AVND_REFLECTION_HELPERS)_", "");
      auto avnd_main_file = plug.files[0].filename.toUtf8();
      auto avnd_qualified = (plug.avnd_namespace + "::" + plug.main_class).toUtf8();
      proto_replaced.replace("@AVND_MAIN_FILE@", avnd_main_file);
      proto_replaced.replace("@AVND_QUALIFIED@", avnd_qualified);
      proto_replaced.replace("@AVND_BASE_TARGET@", avnd_base_target);
      if(!plug.avnd_namespace.isEmpty())
      {
        avnd_additional_classes.append(QString("namespace %1 { struct %2; }\n")
                                           .arg(plug.avnd_namespace)
                                           .arg(plug.main_class)
                                           .toUtf8());
        avnd_custom_factories.append(
            QString("::oscr::custom_factories<%1>(fx, ctx, key); \n")
                .arg(avnd_qualified)
                .toUtf8());
      }
      else
      {
        avnd_additional_classes.append(
            QString("struct %1; \n").arg(plug.main_class).toUtf8());
        avnd_custom_factories.append(
            QString("::oscr::custom_factories<%1>(fx, ctx, key); \n")
                .arg(plug.main_class)
                .toUtf8());
      }
      protos.append(proto_replaced);
    }
    for(QByteArray& f : {std::ref(cpp_proto_replaced), std::ref(hpp_proto_replaced)})
    {
      f.replace("@AVND_PLUGIN_VERSION@", avnd_plugin_version);
      f.replace("@AVND_PLUGIN_UUID@", avnd_plugin_uuid);
      f.replace("@AVND_BASE_TARGET@", avnd_base_target);
      f.replace("@AVND_ADDITIONAL_CLASSES@", avnd_additional_classes.toUtf8());
      f.replace("@AVND_CUSTOM_FACTORIES@", avnd_custom_factories.toUtf8());
    }

    data.unity_cpp.append(hpp_proto_replaced);
    data.unity_cpp.append(protos);
    data.unity_cpp.append(cpp_proto_replaced);

    qDebug().noquote().nospace() << "===============================================\n"
                                 << data.unity_cpp.data();
  }
}

static AddonData loadAddon(const QString& addon)
{
  AddonData data;
  if(QFile f(addon + QDir::separator() + "addon.json"); f.open(QIODevice::ReadOnly))
  {
    qDebug() << "Loading addon info from: " << f.fileName();
    f.setTextModeEnabled(true);
    data.addon_info = QJsonDocument::fromJson(f.readAll()).object();
  }

  if(QFile f(addon + QDir::separator() + "CMakeLists.txt"); f.open(QIODevice::ReadOnly))
  {
    qDebug() << "Loading CMake-based add-on: " << f.fileName();
    // Needed because regex uses \n
    f.setTextModeEnabled(true);
    loadCMakeAddon(addon, data, f.readAll());
  }
  else
  {
    qDebug() << "Loading non-CMake-based add-on";
    loadBasicAddon(addon, data);
  }

  return data;
}

//! Generates the score_myaddon_commands.hpp and score_myaddon_command_list.hpp
//! files
static void generateCommandFiles(
    const QString& output, const QString& addon_path,
    const std::vector<std::pair<QString, QString>>& files)
{
  QRegularExpression decl(
      "SCORE_COMMAND_DECL\\([A-Za-z_0-9,:<>\r\n\t "
      "]*\\(\\)[A-Za-z_0-9,\"':<>\r\n\t ]*\\)");
  QRegularExpression decl_t("SCORE_COMMAND_DECL_T\\([A-Za-z_0-9,:<>\r\n\t ]*\\)");

  QString includes;
  QString commands;
  for(const auto& f : files)
  {
    {
      auto res = decl.globalMatch(f.second);
      while(res.hasNext())
      {
        auto match = res.next();
        if(auto txt = match.capturedTexts(); !txt.empty())
        {
          if(auto split = txt[0].split(","); split.size() > 1)
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
    const QString& addon_files_path, const QString& addon_name,
    const QByteArray& addon_export)
{
  QFile export_file = QFile{addon_files_path + "/" + addon_name + "_export.h"};
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
    QString addon_name, const QString& addon,
    const std::vector<std::pair<QString, QString>>& files)
{
  addon_name.replace("-", "_");
  QByteArray addon_export = addon_name.toUpper().toUtf8();

  QString addon_files_path = QDir::tempPath() + "/score-tmp-build/" + addon_name;
  QDir{}.mkpath(addon_files_path);
  generateExportFile(addon_files_path, addon_name, addon_export);
  generateCommandFiles(addon_files_path, addon, files);
  return addon_files_path;
}

}

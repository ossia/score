#include <score/gfx/DisplayConfig.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>

namespace score::gfx
{
bool DisplaySettings::isEmpty() const noexcept
{
  return outputs.isEmpty() && device.isEmpty() && headless.isEmpty() && rotation == 0
         && !hideCursor && hardwareCursor && !verticalLayout
         && vulkanPhysicalDeviceIndex < 0 && vulkanDisplayIndex < 0
         && vulkanModeIndex < 0 && editorUi && platformOverride.isEmpty();
}

DisplayCapabilities displayCapabilities(const QString& platform)
{
  DisplayCapabilities c;

  // startsWith: the eglfs plug-in is selected as "eglfs", but a device
  // integration may be appended.
  if(platform.startsWith("eglfs"))
  {
    c.perOutputConfiguration = true;
    c.requiresRestart = true;
  }
  else if(platform == "vkkhrdisplay")
  {
    c.indexedDisplaySelection = true;
    c.requiresRestart = true;
  }
  else if(platform == "windows" || platform == "cocoa")
  {
    // The system owns the displays and can be asked to change them while
    // running -- ChangeDisplaySettingsEx, CGCompleteDisplayConfiguration.
    // Not implemented yet, which is why perOutputConfiguration stays false:
    // the dialog must not offer what nothing behind it will do.
    c.appliesToSystemDisplays = true;
  }

  return c;
}

QString resolvePlatform(const QString& current, const DisplaySettings& settings)
{
  if(settings.platformOverride.isEmpty())
    return current;

  if(!displayCapabilities(current).anyConfiguration())
    return current;

  return settings.platformOverride;
}

QVector<DisplayOutput> enumerateOutputs(const QString& drmRoot)
{
  QVector<DisplayOutput> res;

  // cardN-HDMI-A-1 -> HDMI-A-1. The card number is the graphics device and
  // changes with probe order, so it is not part of how an output is named.
  // Absent on Windows, macOS, and a Linux without DRM: entryList is empty
  // there and the walk simply yields nothing, falling through to Qt below.
  QDir root{drmRoot};
  for(const auto& entry : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
  {
    if(!entry.startsWith("card"))
      continue;

    const auto dash = entry.indexOf('-');
    if(dash < 0)
      continue;

    DisplayOutput out;
    out.name = entry.mid(dash + 1);

    QFile status{drmRoot + '/' + entry + "/status"};
    if(status.open(QIODevice::ReadOnly))
      out.connected = status.readAll().trimmed() == "connected";

    QFile modes{drmRoot + '/' + entry + "/modes"};
    if(modes.open(QIODevice::ReadOnly))
    {
      for(const auto& line : modes.readAll().split('\n'))
      {
        const auto mode = QString::fromUtf8(line.trimmed());
        // The list repeats a mode once per refresh rate it supports.
        if(!mode.isEmpty() && !out.modes.contains(mode))
          out.modes.push_back(mode);
      }
    }

    res.push_back(std::move(out));
  }

  // No DRM: Windows, macOS, or a Linux without it. Qt knows the screens, which
  // is less than the kernel would say -- one mode, the current one -- but a
  // real list beats an empty dialog. Only useful once there is a
  // QGuiApplication, which the settings UI has and startup does not.
  if(res.isEmpty() && qGuiApp)
  {
    for(auto* s : QGuiApplication::screens())
    {
      if(!s)
        continue;

      DisplayOutput out;
      out.name = s->name();
      out.connected = true;
      const auto sz = s->geometry().size();
      if(!sz.isEmpty())
        out.modes.push_back(
            QStringLiteral("%1x%2").arg(sz.width()).arg(sz.height()));
      res.push_back(std::move(out));
    }
  }

  return res;
}

QByteArray toKmsConfig(const DisplaySettings& settings)
{
  QJsonObject root;

  if(!settings.device.isEmpty())
    root["device"] = settings.device;
  if(!settings.hardwareCursor)
    root["hwcursor"] = false;
  if(settings.verticalLayout)
    root["virtualDesktopLayout"] = "vertical";
  if(!settings.headless.isEmpty())
    root["headless"] = settings.headless;

  QJsonArray outputs;
  for(const auto& o : settings.outputs)
  {
    if(o.name.isEmpty())
      continue;

    QJsonObject j;
    j["name"] = o.name;
    if(!o.mode.isEmpty())
      j["mode"] = o.mode;
    if(!o.format.isEmpty())
      j["format"] = o.format;
    if(o.primary)
      j["primary"] = true;
    if(o.hasPosition)
      j["virtualPos"] = QStringLiteral("%1, %2").arg(o.x).arg(o.y);
    if(o.physicalWidthMm > 0)
      j["physicalWidth"] = o.physicalWidthMm;
    if(o.physicalHeightMm > 0)
      j["physicalHeight"] = o.physicalHeightMm;
    if(!o.cloneOf.isEmpty())
      j["clones"] = o.cloneOf;

    outputs.push_back(j);
  }

  if(!outputs.isEmpty())
    root["outputs"] = outputs;

  return QJsonDocument{root}.toJson(QJsonDocument::Indented);
}

QString displayConfigPath()
{
  const auto dir
      = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  return dir.isEmpty() ? QString{} : dir + "/display.json";
}

DisplaySettings loadDisplaySettings(const QString& path)
{
  DisplaySettings s;

  QFile f{path};
  if(path.isEmpty() || !f.open(QIODevice::ReadOnly))
    return s;

  const auto doc = QJsonDocument::fromJson(f.readAll());
  if(!doc.isObject())
    return s;

  const auto root = doc.object();
  s.device = root["device"].toString();
  s.hardwareCursor = root["hwcursor"].toBool(true);
  s.verticalLayout = root["virtualDesktopLayout"].toString() == "vertical";
  s.headless = root["headless"].toString();
  s.rotation = root["rotation"].toInt();
  s.hideCursor = root["hideCursor"].toBool();
  s.editorUi = root["editorUi"].toBool(true);
  s.platformOverride = root["platformOverride"].toString();
  s.vulkanPhysicalDeviceIndex = root["vulkanPhysicalDeviceIndex"].toInt(-1);
  s.vulkanDisplayIndex = root["vulkanDisplayIndex"].toInt(-1);
  s.vulkanModeIndex = root["vulkanModeIndex"].toInt(-1);

  for(const auto& v : root["outputs"].toArray())
  {
    const auto j = v.toObject();
    DisplayOutputSettings o;
    o.name = j["name"].toString();
    o.mode = j["mode"].toString();
    o.format = j["format"].toString();
    o.primary = j["primary"].toBool();
    o.physicalWidthMm = j["physicalWidth"].toInt();
    o.physicalHeightMm = j["physicalHeight"].toInt();
    o.cloneOf = j["clones"].toString();

    if(const auto pos = j["virtualPos"].toString(); !pos.isEmpty())
    {
      const auto parts = pos.split(',');
      if(parts.size() == 2)
      {
        bool okX{}, okY{};
        const int x = parts[0].trimmed().toInt(&okX);
        const int y = parts[1].trimmed().toInt(&okY);
        if(okX && okY)
        {
          o.x = x;
          o.y = y;
          o.hasPosition = true;
        }
      }
    }

    s.outputs.push_back(std::move(o));
  }

  return s;
}

bool saveDisplaySettings(const DisplaySettings& settings, const QString& path)
{
  if(path.isEmpty())
    return false;

  QJsonObject root;
  if(!settings.device.isEmpty())
    root["device"] = settings.device;
  root["hwcursor"] = settings.hardwareCursor;
  if(settings.verticalLayout)
    root["virtualDesktopLayout"] = "vertical";
  if(!settings.headless.isEmpty())
    root["headless"] = settings.headless;
  if(settings.rotation != 0)
    root["rotation"] = settings.rotation;
  if(settings.hideCursor)
    root["hideCursor"] = true;
  if(!settings.editorUi)
    root["editorUi"] = false;
  if(!settings.platformOverride.isEmpty())
    root["platformOverride"] = settings.platformOverride;
  if(settings.vulkanPhysicalDeviceIndex >= 0)
    root["vulkanPhysicalDeviceIndex"] = settings.vulkanPhysicalDeviceIndex;
  if(settings.vulkanDisplayIndex >= 0)
    root["vulkanDisplayIndex"] = settings.vulkanDisplayIndex;
  if(settings.vulkanModeIndex >= 0)
    root["vulkanModeIndex"] = settings.vulkanModeIndex;

  QJsonArray outputs;
  for(const auto& o : settings.outputs)
  {
    QJsonObject j;
    j["name"] = o.name;
    if(!o.mode.isEmpty())
      j["mode"] = o.mode;
    if(!o.format.isEmpty())
      j["format"] = o.format;
    if(o.primary)
      j["primary"] = true;
    if(o.hasPosition)
      j["virtualPos"] = QStringLiteral("%1, %2").arg(o.x).arg(o.y);
    if(o.physicalWidthMm > 0)
      j["physicalWidth"] = o.physicalWidthMm;
    if(o.physicalHeightMm > 0)
      j["physicalHeight"] = o.physicalHeightMm;
    if(!o.cloneOf.isEmpty())
      j["clones"] = o.cloneOf;
    outputs.push_back(j);
  }
  root["outputs"] = outputs;

  QDir{}.mkpath(QFileInfo{path}.absolutePath());

  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;

  const auto data = QJsonDocument{root}.toJson(QJsonDocument::Indented);
  return f.write(data) == data.size();
}

bool editorUiRequested()
{
  return loadDisplaySettings(displayConfigPath()).editorUi;
}

bool oneWindowPerScreen() noexcept
{
  // main() asks before there is a QGuiApplication, so the environment is the
  // only answer available then; afterwards the platform itself is the truth,
  // since a -platform argument never reaches the environment.
  const auto p = qGuiApp ? QGuiApplication::platformName()
                         : QString::fromUtf8(qgetenv("QT_QPA_PLATFORM"));
  return p.startsWith("eglfs") || p == "vkkhrdisplay" || p == "linuxfb"
         || p == "minimalegl";
}

void restartIntoEditor()
{
  const auto path = displayConfigPath();
  auto settings = loadDisplaySettings(path);

  settings.editorUi = true;
  // vkkhrdisplay creates a window for a widget and then draws nothing into it,
  // so coming back to the editor there means coming back under eglfs.
  if(displayCapabilities(QGuiApplication::platformName()).indexedDisplaySelection)
    settings.platformOverride = QStringLiteral("eglfs");

  saveDisplaySettings(settings, path);

  QProcess::startDetached(
      QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
  QCoreApplication::quit();
}

void applyDisplayConfig()
{
#if defined(__linux__)
  const auto path = displayConfigPath();
  if(path.isEmpty() || !QFile::exists(path))
    return;

  const auto settings = loadDisplaySettings(path);
  if(settings.isEmpty())
    return;

  if(const auto chosen = resolvePlatform(
         QString::fromUtf8(qgetenv("QT_QPA_PLATFORM")), settings);
     !chosen.isEmpty())
    qputenv("QT_QPA_PLATFORM", chosen.toUtf8());

  // Only meaningful for a platform that reads it: where a window manager owns
  // the display, none of this applies and setting it would be a lie.
  const auto platform = QString::fromUtf8(qgetenv("QT_QPA_PLATFORM"));
  const auto caps = displayCapabilities(platform);

  if(caps.perOutputConfiguration)
  {
    const auto kms = toKmsConfig(settings);
    const auto kmsPath = QFileInfo{path}.absolutePath() + "/display-kms.json";
    QFile f{kmsPath};
    if(f.open(QIODevice::WriteOnly | QIODevice::Truncate)
       && f.write(kms) == kms.size())
    {
      f.close();
      qputenv("QT_QPA_EGLFS_KMS_CONFIG", kmsPath.toUtf8());
    }

    if(settings.rotation != 0)
      qputenv("QT_QPA_EGLFS_ROTATION", QByteArray::number(settings.rotation));
    if(settings.hideCursor)
      qputenv("QT_QPA_EGLFS_HIDECURSOR", "1");
  }
  else if(caps.indexedDisplaySelection)
  {
    // All this platform has. The connector names and the layout above have no
    // counterpart here and are deliberately not approximated.
    if(settings.vulkanPhysicalDeviceIndex >= 0)
      qputenv(
          "QT_VK_PHYSICAL_DEVICE_INDEX",
          QByteArray::number(settings.vulkanPhysicalDeviceIndex));
    if(settings.vulkanDisplayIndex >= 0)
      qputenv("QT_VK_DISPLAY_INDEX", QByteArray::number(settings.vulkanDisplayIndex));
    if(settings.vulkanModeIndex >= 0)
      qputenv("QT_VK_MODE_INDEX", QByteArray::number(settings.vulkanModeIndex));
  }
#endif
}
}

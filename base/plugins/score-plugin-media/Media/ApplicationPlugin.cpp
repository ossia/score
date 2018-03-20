#include "ApplicationPlugin.hpp"
#include <Media/Effect/Settings/Model.hpp>
#if defined(LILV_SHARED)
#include <Media/Effect/LV2/LV2Context.hpp>
#endif
#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Media/Effect/VST/VSTLoader.hpp>
#endif
#include <QFileInfo>
#include <QDirIterator>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Engine/Protocols/OSSIADevice.hpp>

#include <ossia/audio/audio_protocol.hpp>

#include <Engine/Protocols/Audio/AudioDevice.hpp>
/*
template<>
void JSONObjectReader::read<Media::ApplicationPlugin::vst_info>(const Media::ApplicationPlugin::vst_info& p)
{
  obj["Path"] = p.path;
  obj["PrettyName"] = p.prettyName;
  obj["UID"] = p.uniqueID;
  obj["Synth"] = p.isSynth;
}
template<>
void JSONObjectWriter::write<Media::ApplicationPlugin::vst_info>(Media::ApplicationPlugin::vst_info& p)
{
  p.path = obj["Path"].toString();
  p.prettyName = obj["PrettyName"].toString();
  p.uniqueID = obj["UID"].toInt();
  p.isSynth = obj["Synth"].toBool();
}
*/
template<>
void DataStreamReader::read<Media::ApplicationPlugin::vst_info>(const Media::ApplicationPlugin::vst_info& p)
{
  m_stream << p.path << p.prettyName << p.uniqueID << p.isSynth << p.isValid;
}
template<>
void DataStreamWriter::write<Media::ApplicationPlugin::vst_info>(Media::ApplicationPlugin::vst_info& p)
{
  m_stream >> p.path >> p.prettyName >> p.uniqueID >> p.isSynth >> p.isValid;
}

Q_DECLARE_METATYPE(Media::ApplicationPlugin::vst_info)
Q_DECLARE_METATYPE(std::vector<Media::ApplicationPlugin::vst_info>)
namespace Media
{

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& app):
    score::ApplicationPlugin{app}
  #if defined(LILV_SHARED)
  , lv2_context{std::make_unique<LV2::GlobalContext>(64, lv2_host_context)}
  , lv2_host_context{lv2_context.get(), nullptr, lv2_context->features(), lilv}
  #endif
{
  qRegisterMetaType<vst_info>();
  qRegisterMetaTypeStreamOperators<vst_info>();
  qRegisterMetaType<std::vector<vst_info>>();
  qRegisterMetaTypeStreamOperators<std::vector<vst_info>>();
#if defined(LILV_SHARED) // TODO instead add a proper preprocessor macro that also works in static case
    lv2_context->loadPlugins();
#endif

}
void ApplicationPlugin::initialize()
{

#if defined(HAS_VST2)
  // init with the database
  QSettings s;
  auto val = s.value("Effect/KnownVST2");
  if(val.canConvert<std::vector<vst_info>>())
  {
    vst_infos = val.value<std::vector<vst_info>>();
  }

  auto& set = context.settings<Media::Settings::Model>();
  con(set, &Media::Settings::Model::VstPathsChanged,
      this, &ApplicationPlugin::rescanVSTs);
  rescanVSTs(set.getVstPaths());
#endif
}

#if defined(HAS_VST2)
void ApplicationPlugin::rescanVSTs(const QStringList& paths)
{
  // 1. List all plug-ins in new paths
  QSet<QString> newPlugins;
  for(QString dir : paths)
  {
#if defined(__APPLE__)
    {
      QDirIterator it(dir, QStringList{"*.vst", "*.component"}, QDir::AllEntries, QDirIterator::Subdirectories);

      while (it.hasNext())
        newPlugins.insert(it.next());
    }
    {
      QDirIterator it(dir, QStringList{"*.dylib"}, QDir::Files, QDirIterator::Subdirectories);
      while (it.hasNext())
      {
        auto path = it.next();
        if(!path.contains(".vst") && !path.contains(".component"))
          newPlugins.insert(path);
      }
    }
#else
    QDirIterator it(dir, QStringList{Media::VST::default_filter}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
      newPlugins.insert(it.next());
#endif
  }


  // 2. Remove plug-ins not in these paths
  for(auto it = vst_infos.begin(); it != vst_infos.end(); )
  {
    auto new_it = newPlugins.find(it->path);
    if(new_it != newPlugins.end())
    {
      // plug-in is in both set, we ignore it
      newPlugins.erase(new_it);
      ++it;
    }
    else
    {
      it = vst_infos.erase(it);
    }
  }

  // 3. Add remaining plug-ins
  auto add_invalid = [=] (const QString& path)
  {
    vst_info i;
    i.path = path;
    i.prettyName = "invalid";
    i.uniqueID = -1;
    i.isSynth = false;
    i.isValid = false;
    vst_infos.push_back(i);
  };

  for(const QString& path : newPlugins)
  {
    SCORE_ASSERT(!path.isEmpty());
    bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
    if(!isFile)
    {
      qDebug() << "Invalid path: " << path;
      continue;
    }

    try {

      bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
      if(!isFile)
      {
        qDebug() << "Invalid path: " << path;
        continue;
      }

      auto plugin = new Media::VST::VSTModule{path.toStdString()};

      bool ok = false;
      if (auto m = plugin->getMain())
      {
        if (auto p = (AEffect*)m(Media::VST::vst_host_callback))
        {
          vst_info i;
          i.path = path;
          i.uniqueID = p->uniqueID;
          {
            /*
            char buf[256] = {0};
            p->dispatcher(p, effGetEffectName, 0, 0, buf, 0.f);
            QString s = buf;
            qDebug() << path;
            qDebug() << "effGetEffectName: " << s;

            p->dispatcher(p, effGetProductString, 0, 0, buf, 0.f);
            s = buf;
            qDebug() << "effGetProductString: " << s;

            p->dispatcher(p, effGetVstVersion, 0, 0, buf, 0.f);
            s = buf;
            qDebug() << "effGetProductString: " << s;

            p->dispatcher(p, effGetVendorString, 0, 0, buf, 0.f);
            s = buf;
            qDebug() << "effGetVendorString: " << s;

            p->dispatcher(p, effGetVendorVersion, 0, 0, buf, 0.f);
            s = buf;
            qDebug() << "effGetVendorVersion: " << s;
            if(!s.isEmpty())
              i.prettyName = s;
            else
            */
            // Only way to get a separation between Kontakt 5 / Kontakt 5 (8 out) / Kontakt 5 (16 out),  etc...
            i.prettyName = QFileInfo(path).baseName();
          }

          i.isSynth = p->flags & effFlagsIsSynth;
          i.isValid = true;
          vst_infos.push_back(std::move(i));

          vst_modules.insert({p->uniqueID, plugin});
          ok = true;
        }
      }

      if(!ok)
      {
        add_invalid(path);
        delete plugin;
      }


    } catch(const std::runtime_error& e) {
      add_invalid(path);
      qDebug() << e.what();
      continue;
    }

  }

  // write in the database
  QSettings{}.setValue("Effect/KnownVST2", QVariant::fromValue(vst_infos));
}
#endif
ApplicationPlugin::~ApplicationPlugin()
{
#if defined(HAS_VST2)
    for(auto& e : vst_modules)
    {
      delete e.second;
    }
#endif
}

GUIApplicationPlugin::GUIApplicationPlugin(const score::GUIApplicationContext& app):
  score::GUIApplicationPlugin{app}
{

}

void GUIApplicationPlugin::initialize()
{
}

}

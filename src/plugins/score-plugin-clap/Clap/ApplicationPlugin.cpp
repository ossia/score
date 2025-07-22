#include "ApplicationPlugin.hpp"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QThread>

#include <score/tools/Bind.hpp>

#include <clap/all.h>

#include <wobjectimpl.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

W_OBJECT_IMPL(Clap::ApplicationPlugin)

namespace Clap
{

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : score::GUIApplicationPlugin{app}
{
}

ApplicationPlugin::~ApplicationPlugin()
{
  if(m_scanThread.joinable())
    m_scanThread.join();
}

void ApplicationPlugin::initialize()
{
  rescanPlugins();
}

void ApplicationPlugin::rescanPlugins()
{
  // Launch plugin scanning in background thread
  if(m_scanThread.joinable())
    m_scanThread.join();

  m_scanThread = std::thread([this] { scanClapPlugins(); });
}

void ApplicationPlugin::scanClapPlugins()
{
  std::vector<PluginInfo> plugins;
  
  // Standard CLAP plugin directories
  QStringList searchPaths;
  
#if defined(__APPLE__)
  searchPaths << "/Library/Audio/Plug-Ins/CLAP"
              << "~/Library/Audio/Plug-Ins/CLAP";
#elif defined(_WIN32)
  searchPaths << "C:/Program Files/Common Files/CLAP"
              << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/CLAP";
#else
  searchPaths << "/usr/lib/clap"
              << "/usr/local/lib/clap"
              << "~/.clap"
              << QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.clap";
#endif

  for(const QString& searchPath : searchPaths)
  {
    QDir dir(searchPath);
    if(!dir.exists())
      continue;

    // Scan for .clap files
    QStringList filters;
#if defined(__APPLE__)
    filters << "*.clap";
#elif defined(_WIN32)
    filters << "*.clap";
#else
    filters << "*.clap";
#endif

    for(const QString& fileName : dir.entryList(filters, QDir::Files))
    {
      QString fullPath = dir.absoluteFilePath(fileName);
      
      // Load the plugin library
#if defined(_WIN32)
      HMODULE handle = LoadLibraryA(fullPath.toLocal8Bit().data());
      if(!handle)
        continue;
        
      auto entry_fn = (const clap_plugin_entry_t*)GetProcAddress(handle, "clap_entry");
#else
      void* handle = dlopen(fullPath.toLocal8Bit().data(), RTLD_LAZY);
      if(!handle)
        continue;
        
      auto entry_fn = (const clap_plugin_entry_t*)dlsym(handle, "clap_entry");
#endif

      if(!entry_fn || !entry_fn->init(fullPath.toLocal8Bit().data()))
      {
#if defined(_WIN32)
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        continue;
      }

      auto factory = (const clap_plugin_factory_t*)entry_fn->get_factory(CLAP_PLUGIN_FACTORY_ID);
      if(!factory)
      {
        entry_fn->deinit();
#if defined(_WIN32)
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        continue;
      }

      uint32_t plugin_count = factory->get_plugin_count(factory);
      for(uint32_t i = 0; i < plugin_count; ++i)
      {
        const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, i);
        if(!desc)
          continue;

        PluginInfo info;
        info.path = fullPath;
        info.id = QString::fromUtf8(desc->id);
        info.name = QString::fromUtf8(desc->name);
        info.vendor = QString::fromUtf8(desc->vendor ? desc->vendor : "");
        info.version = QString::fromUtf8(desc->version ? desc->version : "");
        info.url = QString::fromUtf8(desc->url ? desc->url : "");
        info.manual_url = QString::fromUtf8(desc->manual_url ? desc->manual_url : "");
        info.support_url = QString::fromUtf8(desc->support_url ? desc->support_url : "");
        info.description = QString::fromUtf8(desc->description ? desc->description : "");

        if(desc->features)
        {
          for(const char* const* feature = desc->features; *feature; ++feature)
          {
            info.features.push_back(QString::fromUtf8(*feature));
          }
        }
        
        plugins.push_back(std::move(info));
      }

      entry_fn->deinit();
#if defined(_WIN32)
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
    }
  }

  m_plugins = std::move(plugins);
  
  // Emit signal to update library
  QMetaObject::invokeMethod(this, [this] {
    pluginsChanged();
  }, Qt::QueuedConnection);
}

}

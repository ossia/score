#pragma once
#include <JitCpp/AddonCompiler.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QFileSystemWatcher>
#include <QSet>
#include <QThread>

namespace Jit
{

struct ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
  ApplicationPlugin(const score::GUIApplicationContext& ctx);

  bool setupAddon(const QString& addon);
  void registerAddon(score::Plugin_QtInterface*);
  void updateAddon(const QString& addon);

  bool setupNode(const QString& addon);
  void initialize() override;

  void rescanAddons();
  void rescanNodes();

  QFileSystemWatcher m_addonsWatch;
  QFileSystemWatcher m_nodesWatch;
  QSet<QString> m_addonsPaths;
  AddonCompiler m_compiler;
};
}

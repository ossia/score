#pragma once
#include <Process/Process.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <JS/JSProcessMetadata.hpp>
#include <JS/Qml/QmlObjects.hpp>

#include <QFileSystemWatcher>
#include <QQmlComponent>
#include <QQmlEngine>

#include <score_plugin_js_export.h>

#include <memory>
#include <verdigris>
namespace JS
{
class Script;
class ProcessModel;

void setupEngineImportPaths(QQmlEngine& engine) noexcept;

struct ComponentCache
{
public:
  JS::Script*
  get(const JS::ProcessModel& process, const QByteArray& str, bool isFile) noexcept;
  JS::Script* tryGet(const QByteArray& str, bool isFile) const noexcept;
  ComponentCache();
  ~ComponentCache();

private:
  struct Cache
  {
    QByteArray key;
    std::unique_ptr<QQmlComponent> component{};
    std::unique_ptr<JS::Script> object{};
  };
  std::vector<Cache> m_map;
};

class SCORE_PLUGIN_JS_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(JS::ProcessModel)
  W_OBJECT(ProcessModel)
public:
  static constexpr bool hasExternalUI() noexcept { return true; }

  explicit ProcessModel(
      const TimeVal& duration, const QString& data, const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  [[nodiscard]] Process::ScriptChangeResult setScript(const QString& script);
  const QString& script() const noexcept { return m_script; }

  const QByteArray& qmlData() const noexcept { return m_qmlData; }

  JS::Script* currentObject() const noexcept;
  bool isGpu() const noexcept;

  ~ProcessModel() override;

  bool validate(const QString& str) const noexcept;
  void errorMessage(int arg_1, const QString& arg_2) const
      W_SIGNAL(errorMessage, arg_1, arg_2);
  void scriptOk() W_SIGNAL(scriptOk);
  void scriptChanged(const QString& arg_1) W_SIGNAL(scriptChanged, arg_1);
  void programChanged() W_SIGNAL(programChanged);

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  QString effect() const noexcept override;
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  Process::ScriptChangeResult setQmlData(const QByteArray&, bool isFile);

  QString m_script;
  QByteArray m_qmlData;
  mutable ComponentCache m_cache;
  bool m_isFile{};
};
}

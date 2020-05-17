#pragma once
#include <JS/JSProcessMetadata.hpp>
#include <Process/Process.hpp>

#include <QFileSystemWatcher>
#include <QQmlComponent>
#include <QQmlEngine>

#include <score_plugin_js_export.h>
#include <verdigris>

#include <memory>
namespace JS
{
class Script;
class ProcessModel;
struct ComponentCache
{
public:
  JS::Script* get(JS::ProcessModel& process, const QByteArray& str, bool isFile) noexcept;
  JS::Script* tryGet(const QByteArray& str, bool isFile) const noexcept;
  ComponentCache();
  ~ComponentCache();
private:
  struct Cache {
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
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  void setScript(const QString& script);
  void setQmlData(const QByteArray&, bool isFile);
  const QString& script() const noexcept { return m_script; }
  const QByteArray& qmlData() const noexcept { return m_qmlData; }
  QQmlEngine& engine() noexcept { return m_dummyEngine; }

  JS::Script* currentObject() const noexcept;

  ~ProcessModel() override;

  void errorMessage(int arg_1, const QString& arg_2)
      W_SIGNAL(errorMessage, arg_1, arg_2);
  void scriptOk() W_SIGNAL(scriptOk);
  void scriptChanged(const QString& arg_1) W_SIGNAL(scriptChanged, arg_1);

  void qmlDataChanged(const QString& arg_1) W_SIGNAL(qmlDataChanged, arg_1);


  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  QString m_script;
  QByteArray m_qmlData;
  QQmlEngine m_dummyEngine;
  ComponentCache m_cache;
  std::unique_ptr<QFileSystemWatcher> m_watch;
  bool m_isFile{};
};
}

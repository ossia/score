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
class SCORE_PLUGIN_JS_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(JS::ProcessModel)
  W_OBJECT(ProcessModel)
public:
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
  const QString& script() const { return m_script; }
  const QString& qmlData() const { return m_qmlData; }

  ~ProcessModel() override;

  QObject* m_dummyObject{};

  void errorMessage(int arg_1, const QString& arg_2)
      W_SIGNAL(errorMessage, arg_1, arg_2);
  void scriptOk() W_SIGNAL(scriptOk);
  void scriptChanged(const QString& arg_1) W_SIGNAL(scriptChanged, arg_1);

  void qmlDataChanged(const QString& arg_1) W_SIGNAL(qmlDataChanged, arg_1);

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  QString m_script, m_qmlData;
  QQmlEngine m_dummyEngine;
  std::unique_ptr<QQmlComponent> m_dummyComponent;
  std::unique_ptr<QFileSystemWatcher> m_watch;
};
}

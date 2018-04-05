#pragma once
#include <JS/JSProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QFileSystemWatcher>
#include <memory>
namespace JS
{
class ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(JS::ProcessModel)
  Q_OBJECT
public:
  explicit ProcessModel(
      const TimeVal& duration,
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
  const QString& script() const
  {
    return m_script;
  }
  const QString& qmlData() const
  {
    return m_qmlData;
  }

  ~ProcessModel() override;

  QObject* m_dummyObject{};
Q_SIGNALS:
  void scriptError(int, const QString&);
  void scriptOk();
  void scriptChanged(const QString&);

  void qmlDataChanged(const QString&);

private:
  QString m_script, m_qmlData;
  QQmlEngine m_dummyEngine;
  std::unique_ptr<QQmlComponent> m_dummyComponent;
  std::unique_ptr<QFileSystemWatcher> m_watch;
};
}

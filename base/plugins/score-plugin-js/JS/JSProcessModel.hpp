#pragma once
#include <JS/JSProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <QByteArray>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QString>
#include <memory>

#include <Process/TimeValue.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/model/Identifier.hpp>
#include <QJSValue>
#include <QVariantMap>
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
  const QString& script() const
  {
    return m_script;
  }

  ~ProcessModel();

  Process::Inlets inlets() const override { return m_inlets; }
  Process::Outlets outlets() const override { return m_outlets; }

  QObject* m_dummyObject{};
signals:
  void scriptError(int, const QString&);
  void scriptOk();
  void scriptChanged(const QString&);

private:
  QString m_script;
  Process::Inlets m_inlets;
  Process::Outlets m_outlets;
  QQmlEngine m_dummyEngine;
  std::unique_ptr<QQmlComponent> m_dummyComponent;
};
}

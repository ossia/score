#pragma once
#include <JS/JSProcessMetadata.hpp>
#include <Process/StateProcess.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
namespace JS
{
class StateProcess : public Process::StateProcess
{
  Q_OBJECT
  ISCORE_SERIALIZE_FRIENDS
  MODEL_METADATA_IMPL(StateProcess)
public:
  explicit StateProcess(const Id<Process::StateProcess>& id, QObject* parent);

  explicit StateProcess(
      const StateProcess& source,
      const Id<Process::StateProcess>& id,
      QObject* parent)
      : Process::StateProcess{id, Metadata<ObjectKey_k, StateProcess>::get(),
                              parent}
      , m_script{source.m_script}
  {
  }

  template <typename Impl>
  explicit StateProcess(Impl& vis, QObject* parent)
      : Process::StateProcess{vis, parent}
  {
    vis.writeTo(*this);
  }

  void setScript(const QString& script);
  const QString& script() const
  {
    return m_script;
  }

  const std::vector<std::pair<QByteArray, QVariant>>& customProperties() const { return m_properties; }
signals:
  void scriptError(int, const QString&);
  void scriptOk();
  void scriptChanged(QString);

private:
  QString prettyName() const override
  {
    return Metadata<PrettyName_k, StateProcess>::get();
  }

  QString m_script;
  std::vector<std::pair<QByteArray, QVariant>> m_properties;
};
}

#pragma once
#include <JS/JSProcessMetadata.hpp>
#include <Process/StateProcess.hpp>
#include <score/serialization/VisitorCommon.hpp>
namespace JS
{
class StateProcess : public Process::StateProcess
{
  Q_OBJECT
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(StateProcess)
public:
  explicit StateProcess(const Id<Process::StateProcess>& id, QObject* parent);

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

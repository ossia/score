#pragma once
#include <JS/JSProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <QByteArray>
#include <QString>
#include <memory>

#include <Process/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class DataStream;
class JSONObject;
namespace Process
{
class LayerModel;
}

class QObject;

namespace JS
{
class ProcessModel final : public Process::ProcessModel
{
  ISCORE_SERIALIZE_FRIENDS(ProcessModel, DataStream)
  ISCORE_SERIALIZE_FRIENDS(ProcessModel, JSONObject)
  MODEL_METADATA_IMPL(JS::ProcessModel)
  Q_OBJECT
public:
  explicit ProcessModel(
      const TimeValue& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  explicit ProcessModel(
      const ProcessModel& source,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Deserializer<Impl>& vis, QObject* parent)
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
signals:
  void scriptChanged(QString);

private:
  QString m_script;
};
}

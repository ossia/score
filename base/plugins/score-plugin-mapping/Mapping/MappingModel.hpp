#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <wobjectdefs.h>
#include <Mapping/MappingProcessMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <QByteArray>
#include <QString>
#include <State/Address.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score_plugin_mapping_export.h>
namespace Process
{
class Inlet;
class Outlet;
}
namespace Mapping
{
class SCORE_PLUGIN_MAPPING_EXPORT ProcessModel final
    : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Mapping::ProcessModel)

  W_OBJECT(ProcessModel)

  
  
  

  
  
  
public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  ProcessModel(DataStream::Deserializer& vis, QObject* parent);
  ProcessModel(JSONObject::Deserializer& vis, QObject* parent);

  //// MappingModel specifics ////
  State::AddressAccessor sourceAddress() const;
  double sourceMin() const;
  double sourceMax() const;

  void setSourceAddress(const State::AddressAccessor& arg);
  void setSourceMin(double arg);
  void setSourceMax(double arg);

  State::AddressAccessor targetAddress() const;
  double targetMin() const;
  double targetMax() const;

  void setTargetAddress(const State::AddressAccessor& arg);
  void setTargetMin(double arg);
  void setTargetMax(double arg);

  QString prettyName() const override;

  ~ProcessModel() override;

  std::unique_ptr<Process::Inlet> inlet;
  std::unique_ptr<Process::Outlet> outlet;
public:
  void sourceAddressChanged(const State::AddressAccessor& arg) W_SIGNAL(sourceAddressChanged, arg);
  void sourceMinChanged(double arg) W_SIGNAL(sourceMinChanged, arg);
  void sourceMaxChanged(double arg) W_SIGNAL(sourceMaxChanged, arg);

  void targetAddressChanged(const State::AddressAccessor& arg) W_SIGNAL(targetAddressChanged, arg);
  void targetMinChanged(double arg) W_SIGNAL(targetMinChanged, arg);
  void targetMaxChanged(double arg) W_SIGNAL(targetMaxChanged, arg);

private:
  void init();
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  double m_sourceMin{};
  double m_sourceMax{};

  double m_targetMin{};
  double m_targetMax{};

W_PROPERTY(double, targetMax READ targetMax WRITE setTargetMax NOTIFY targetMaxChanged)

W_PROPERTY(double, targetMin READ targetMin WRITE setTargetMin NOTIFY targetMinChanged)

W_PROPERTY(State::AddressAccessor, targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)

W_PROPERTY(double, sourceMax READ sourceMax WRITE setSourceMax NOTIFY sourceMaxChanged)

W_PROPERTY(double, sourceMin READ sourceMin WRITE setSourceMin NOTIFY sourceMinChanged)

W_PROPERTY(State::AddressAccessor, sourceAddress READ sourceAddress WRITE setSourceAddress NOTIFY sourceAddressChanged)
};
}

#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <State/Address.hpp>

#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <QString>

#include <score_plugin_mapping_export.h>

#include <verdigris>
namespace Process
{
class Inlet;
class Outlet;
}
namespace Mapping
{
class SCORE_PLUGIN_MAPPING_EXPORT ProcessModel final : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Mapping::ProcessModel)

  W_OBJECT(ProcessModel)

public:
  ProcessModel(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  ProcessModel(DataStream::Deserializer& vis, QObject* parent);
  ProcessModel(JSONObject::Deserializer& vis, QObject* parent);

  //// MappingModel specifics ////
  State::AddressAccessor sourceAddress() const noexcept;
  double sourceMin() const noexcept;
  double sourceMax() const noexcept;

  void setSourceAddress(const State::AddressAccessor& arg);
  void setSourceMin(double arg);
  void setSourceMax(double arg);

  State::AddressAccessor targetAddress() const noexcept;
  double targetMin() const noexcept;
  double targetMax() const noexcept;

  void setTargetAddress(const State::AddressAccessor& arg);
  void setTargetMin(double arg);
  void setTargetMax(double arg);

  QString prettyName() const noexcept override;
  QString prettyValue(double x, double y) const noexcept override;

  ~ProcessModel() override;

  std::unique_ptr<Process::Inlet> inlet;
  std::unique_ptr<Process::Outlet> outlet;

public:
  void sourceAddressChanged(const State::AddressAccessor& arg)
      E_SIGNAL(SCORE_PLUGIN_MAPPING_EXPORT, sourceAddressChanged, arg)
  void sourceMinChanged(double arg) E_SIGNAL(SCORE_PLUGIN_MAPPING_EXPORT, sourceMinChanged, arg)
  void sourceMaxChanged(double arg) E_SIGNAL(SCORE_PLUGIN_MAPPING_EXPORT, sourceMaxChanged, arg)

  void targetAddressChanged(const State::AddressAccessor& arg)
      E_SIGNAL(SCORE_PLUGIN_MAPPING_EXPORT, targetAddressChanged, arg)
  void targetMinChanged(double arg) E_SIGNAL(SCORE_PLUGIN_MAPPING_EXPORT, targetMinChanged, arg)
  void targetMaxChanged(double arg) E_SIGNAL(SCORE_PLUGIN_MAPPING_EXPORT, targetMaxChanged, arg)

  PROPERTY(double, targetMax READ targetMax WRITE setTargetMax NOTIFY targetMaxChanged)
  PROPERTY(double, targetMin READ targetMin WRITE setTargetMin NOTIFY targetMinChanged)
  PROPERTY(
      State::AddressAccessor,
      targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
  PROPERTY(double, sourceMax READ sourceMax WRITE setSourceMax NOTIFY sourceMaxChanged)
  PROPERTY(double, sourceMin READ sourceMin WRITE setSourceMin NOTIFY sourceMinChanged)
  PROPERTY(
      State::AddressAccessor,
      sourceAddress READ sourceAddress WRITE setSourceAddress NOTIFY sourceAddressChanged)

private:
  void init();
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  double m_sourceMin{};
  double m_sourceMax{};

  double m_targetMin{};
  double m_targetMax{};
};
}

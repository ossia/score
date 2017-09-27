#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <QByteArray>
#include <QString>
#include <State/Address.hpp>

#include <Process/TimeValue.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score_plugin_mapping_export.h>
#include <score/model/Identifier.hpp>

namespace Mapping
{
class SCORE_PLUGIN_MAPPING_EXPORT ProcessModel final
    : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Mapping::ProcessModel)

  Q_OBJECT

  Q_PROPERTY(State::AddressAccessor sourceAddress READ sourceAddress WRITE
                 setSourceAddress NOTIFY sourceAddressChanged)
  Q_PROPERTY(double sourceMin READ sourceMin WRITE setSourceMin NOTIFY
                 sourceMinChanged)
  Q_PROPERTY(double sourceMax READ sourceMax WRITE setSourceMax NOTIFY
                 sourceMaxChanged)

  Q_PROPERTY(State::AddressAccessor targetAddress READ targetAddress WRITE
                 setTargetAddress NOTIFY targetAddressChanged)
  Q_PROPERTY(double targetMin READ targetMin WRITE setTargetMin NOTIFY
                 targetMinChanged)
  Q_PROPERTY(double targetMax READ targetMax WRITE setTargetMax NOTIFY
                 targetMaxChanged)
public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent)
      : CurveProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

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


  std::unique_ptr<Process::Port> inlet, outlet;
signals:
  void sourceAddressChanged(const State::AddressAccessor& arg);
  void sourceMinChanged(double arg);
  void sourceMaxChanged(double arg);

  void targetAddressChanged(const State::AddressAccessor& arg);
  void targetMinChanged(double arg);
  void targetMaxChanged(double arg);

private:
  void init();
  ProcessModel(
      const ProcessModel& source,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  //// ProcessModel ////
  std::vector<Process::Port*> inlets() const override;
  std::vector<Process::Port*> outlets() const override;
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  double m_sourceMin{};
  double m_sourceMax{};

  double m_targetMin{};
  double m_targetMax{};
};
}

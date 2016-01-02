#pragma once

#include <Curve/Process/CurveProcessModel.hpp>
#include <State/Address.hpp>
#include <QByteArray>
#include <QString>

#include <Process/ProcessFactoryKey.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class DataStream;
class JSONObject;
namespace Process {
class LayerModel;
}
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

class MappingModel : public Curve::CurveProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(MappingModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(MappingModel, JSONObject)

        Q_OBJECT

        Q_PROPERTY(State::Address sourceAddress READ sourceAddress WRITE setSourceAddress NOTIFY sourceAddressChanged)
        Q_PROPERTY(double sourceMin READ sourceMin WRITE setSourceMin NOTIFY sourceMinChanged)
        Q_PROPERTY(double sourceMax READ sourceMax WRITE setSourceMax NOTIFY sourceMaxChanged)

        Q_PROPERTY(State::Address targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
        Q_PROPERTY(double targetMin READ targetMin WRITE setTargetMin NOTIFY targetMinChanged)
        Q_PROPERTY(double targetMax READ targetMax WRITE setTargetMax NOTIFY targetMaxChanged)
    public:
        MappingModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent);

        template<typename Impl>
        MappingModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveProcessModel{vis, parent}
        {
            vis.writeTo(*this);
        }

        //// MappingModel specifics ////
        State::Address sourceAddress() const;
        double sourceMin() const;
        double sourceMax() const;

        void setSourceAddress(const State::Address& arg);
        void setSourceMin(double arg);
        void setSourceMax(double arg);

        State::Address targetAddress() const;
        double targetMin() const;
        double targetMax() const;

        void setTargetAddress(const State::Address& arg);
        void setTargetMin(double arg);
        void setTargetMax(double arg);

    signals:
        void sourceAddressChanged(const State::Address& arg);
        void sourceMinChanged(double arg);
        void sourceMaxChanged(double arg);

        void targetAddressChanged(const State::Address& arg);
        void targetMinChanged(double arg);
        void targetMaxChanged(double arg);

    private:
        MappingModel(const MappingModel& source,
                        const Id<Process::ProcessModel>& id,
                        QObject* parent);
        Process::LayerModel* cloneLayer_impl(
                const Id<Process::LayerModel>& newId,
                const Process::LayerModel& source,
                QObject* parent) override;

        Process::ProcessModel* clone(
                const Id<Process::ProcessModel>& newId,
                QObject* newParent) const override;

        //// ProcessModel ////
        const ProcessFactoryKey& key() const override;

        QString prettyName() const override;

        Process::LayerModel* makeLayer_impl(
                const Id<Process::LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;
        Process::LayerModel* loadLayer_impl(
                const VisitorVariant&,
                QObject* parent) override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        void serialize(const VisitorVariant& vis) const override;

        /// States
        ProcessStateDataInterface* startStateData() const override;
        ProcessStateDataInterface* endStateData() const override;



        State::Address m_sourceAddress;
        State::Address m_targetAddress;

        double m_sourceMin{};
        double m_sourceMax{};

        double m_targetMin{};
        double m_targetMax{};
};

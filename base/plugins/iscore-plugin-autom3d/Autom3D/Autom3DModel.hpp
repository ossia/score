#pragma once
#include <Process/Process.hpp>
#include <Autom3D/State/Autom3DState.hpp>
#include <State/Address.hpp>
#include <QByteArray>
#include <QString>

#include <Autom3D/Point.hpp>
#include <Process/ProcessFactoryKey.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class DataStream;
class JSONObject;
namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_autom3d_export.h>


namespace Autom3D
{

class ISCORE_PLUGIN_AUTOM3D_EXPORT ProcessModel final : public Process::ProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(ProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ProcessModel, JSONObject)

        Q_OBJECT
        Q_PROPERTY(::State::Address address READ address WRITE setAddress NOTIFY addressChanged)
        // Min and max to scale the curve with at execution
        Q_PROPERTY(Point min READ min WRITE setMin NOTIFY minChanged)
        Q_PROPERTY(Point max READ max WRITE setMax NOTIFY maxChanged)

    public:
        ProcessModel(const TimeValue& duration,
                     const Id<Process::ProcessModel>& id,
                     QObject* parent);
        Process::ProcessModel* clone(const Id<Process::ProcessModel>& newId,
                            QObject* newParent) const override;

        template<typename Impl>
        ProcessModel(Deserializer<Impl>& vis, QObject* parent) :
            Process::ProcessModel{vis, parent},
            m_startState{new ProcessState{*this, 0., this}},
            m_endState{new ProcessState{*this, 1., this}}
        {
            vis.writeTo(*this);
        }

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
        ProcessState* startStateData() const override;
        ProcessState* endStateData() const override;

        //// Autom3DModel specifics ////
        ::State::Address address() const;

        Point value(const TimeValue& time);

        Point min() const;
        Point max() const;

        void setAddress(const ::State::Address& arg);

        void setMin(Point arg);
        void setMax(Point arg);

    signals:
        void addressChanged(const ::State::Address& arg);

        void minChanged(Point arg);
        void maxChanged(Point arg);

    protected:
        ProcessModel(const ProcessModel& source,
                        const Id<Process::ProcessModel>& id,
                        QObject* parent);
        Process::LayerModel* cloneLayer_impl(
                const Id<Process::LayerModel>& newId,
                const Process::LayerModel& source,
                QObject* parent) override;

    private:
        void startExecution() override;
        void stopExecution() override;
        void reset() override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection& s) const override;

        ::State::Address m_address;

        Point m_min{};
        Point m_max{};

        ProcessState* m_startState{};
        ProcessState* m_endState{};
};
}
Q_DECLARE_METATYPE(Autom3D::Point)

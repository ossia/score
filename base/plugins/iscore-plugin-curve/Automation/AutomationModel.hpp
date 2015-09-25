#pragma once

#include <ProcessInterface/Process.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <State/AutomationState.hpp>
#include <State/Address.hpp>

/**
 * @brief The AutomationModel class
 *
 * Points are in relative coordinates :
 *	x is between  0 and 1,
 *  y is between -1 and 1.
 *
 * The duration is the time between x=0 and x=1.
 *
 */
class CurveModel;
class AutomationState;
class AutomationModel : public Process
{
        ISCORE_SERIALIZE_FRIENDS(AutomationModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(AutomationModel, JSONObject)

        Q_OBJECT
        Q_PROPERTY(iscore::Address address READ address WRITE setAddress NOTIFY addressChanged)
        // Min and max to scale the curve with at execution
        Q_PROPERTY(double min READ min WRITE setMin NOTIFY minChanged)
        Q_PROPERTY(double max READ max WRITE setMax NOTIFY maxChanged)

    public:
        AutomationModel(const TimeValue& duration,
                        const Id<Process>& id,
                        QObject* parent);
        Process* clone(const Id<Process>& newId,
                                           QObject* newParent) const override;

        template<typename Impl>
        AutomationModel(Deserializer<Impl>& vis, QObject* parent) :
            Process{vis, parent}
        {
            vis.writeTo(*this);
        }

        //// ProcessModel ////
        QString processName() const override;

        LayerModel* makeLayer_impl(
                const Id<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;
        LayerModel* loadLayer_impl(
                const VisitorVariant&,
                QObject* parent) override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        void startExecution() override;
        void stopExecution() override;
        void reset() override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection&) const override;

        void serialize(const VisitorVariant& vis) const override;

        /// States
        AutomationState* startState() const override;
        AutomationState* endState() const override;

        //// AutomationModel specifics ////
        iscore::Address address() const;

        CurveModel& curve() const
        { return *m_curve; }

        double value(const TimeValue& time);

        double min() const;
        double max() const;

    signals:
        void addressChanged(const iscore::Address& arg);
        void curveChanged();

        void minChanged(double arg);

        void maxChanged(double arg);

    public slots:
        void setAddress(const iscore::Address& arg);

        void setMin(double arg);
        void setMax(double arg);

    protected:
        AutomationModel(const AutomationModel& source,
                        const Id<Process>& id,
                        QObject* parent);
        LayerModel* cloneLayer_impl(
                const Id<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent) override;

    private:
        void setCurve(CurveModel* newCurve);
        iscore::Address m_address;
        CurveModel* m_curve{};

        double m_min{};
        double m_max{};

        AutomationState* m_startState{};
        AutomationState* m_endState{};
};

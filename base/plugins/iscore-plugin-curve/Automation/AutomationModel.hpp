#pragma once

#include <ProcessInterface/ProcessModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
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
class AutomationModel : public ProcessModel
{

        friend void Visitor<Writer<DataStream>>::writeTo<AutomationModel>(AutomationModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<AutomationModel>(AutomationModel& ev);

        Q_OBJECT
        Q_PROPERTY(iscore::Address address READ address WRITE setAddress NOTIFY addressChanged)
        // Min and max to scale the curve with at execution
        Q_PROPERTY(double min READ min WRITE setMin NOTIFY minChanged)
        Q_PROPERTY(double max READ max WRITE setMax NOTIFY maxChanged)

    public:
        AutomationModel(const TimeValue& duration,
                        const id_type<ProcessModel>& id,
                        QObject* parent);
        ProcessModel* clone(const id_type<ProcessModel>& newId,
                                           QObject* newParent) override;

        template<typename Impl>
        AutomationModel(Deserializer<Impl>& vis, QObject* parent) :
            ProcessModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        //// ProcessModel ////
        QString processName() const override;

        LayerModel* makeLayer_impl(
                const id_type<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;
        LayerModel* loadLayer_impl(
                const VisitorVariant&,
                QObject* parent) override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection&) const override;

        void serialize(const VisitorVariant& vis) const override;

        /// States
        ProcessStateDataInterface* startState() const override;
        ProcessStateDataInterface* endState() const override;

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
                        const id_type<ProcessModel>& id,
                        QObject* parent);
        LayerModel* cloneLayer_impl(
                const id_type<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent) override;

    private:
        void setCurve(CurveModel* newCurve);
        iscore::Address m_address;
        CurveModel* m_curve{};

        double m_min;
        double m_max;
};

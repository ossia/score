#pragma once

#include <ProcessInterface/ProcessModel.hpp>

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
class AutomationModel : public ProcessModel
{
        Q_OBJECT
        Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)

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

        ProcessViewModelInterface* makeViewModel_impl(
                const id_type<ProcessViewModelInterface>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;
        ProcessViewModelInterface* loadViewModel_impl(
                const VisitorVariant&,
                QObject* parent) override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        Selection selectableChildren() const override { return {}; }
        Selection selectedChildren() const override { return {}; }
        void setSelection(const Selection&) const override { }

        void serialize(const VisitorVariant& vis) const override;

        /// States
        ProcessStateDataInterface* startState() const override;
        ProcessStateDataInterface* endState() const override;

        //// AutomationModel specifics ////
        QString address() const
        {
            return m_address;
        }

        const QMap<double, double>& points() const
        {
            return m_points;
        }

        void setPoints(QMap<double, double>&& points)
        {
            m_points = std::move(points);
        }

        void addPoint(double x, double y);
        void removePoint(double x);
        void movePoint(double oldx, double newx, double newy);

        double value(const TimeValue& time);

    signals:
        void addressChanged(QString arg);
        void pointsChanged();

    public slots:
        void setAddress(const QString& arg)
        {
            if(m_address == arg)
            {
                return;
            }

            m_address = arg;
            emit addressChanged(arg);
        }

    protected:
        ProcessViewModelInterface* cloneViewModel_impl(
                const id_type<ProcessViewModelInterface>& newId,
                const ProcessViewModelInterface& source,
                QObject* parent) override;

    private:
        QString m_address;
        QMap<double, double> m_points;
};

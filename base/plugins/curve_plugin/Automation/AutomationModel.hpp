#pragma once

#include <ProcessInterface/ProcessSharedModelInterface.hpp>

// TODO Curvature
/**
 * @brief The AutomationModel class
 *
 * Points are in relative coordinates :
 *	x is between  0 and 1,
 *  y is between -1 and 1.
 *
 *
 *
 */
class AutomationModel : public ProcessSharedModelInterface
{
		Q_OBJECT
		Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)

	public:
		AutomationModel(id_type<ProcessSharedModelInterface> id, QObject* parent);
		virtual ProcessSharedModelInterface* clone(id_type<ProcessSharedModelInterface> newId,
												   QObject* newParent) override;

		template<typename Impl>
		AutomationModel(Deserializer<Impl>& vis, QObject* parent):
			ProcessSharedModelInterface{vis, parent}
		{
			vis.writeTo(*this);
		}

		virtual QString processName() const override;
		virtual void serialize(SerializationIdentifier identifier, void* data) const override;

		virtual ProcessViewModelInterface* makeViewModel(id_type<ProcessViewModelInterface> viewModelId,
														 QObject* parent) override;
		virtual ProcessViewModelInterface* makeViewModel(SerializationIdentifier identifier,
														 void* data,
														 QObject* parent) override;
		virtual ProcessViewModelInterface* makeViewModel(id_type<ProcessViewModelInterface> newId,
														 const ProcessViewModelInterface* source,
														 QObject* parent) override;


		QString address() const
		{ return m_address; }

		const QMap<double, double>& points() const
		{ return m_points; }

		void setPoints(QMap<double,double>&& points)
		{
			m_points = std::move(points);
		}

		void addPoint(double x, double y);
		void removePoint(double x);
		void movePoint(double oldx, double newx, double newy);

	signals:
		void addressChanged(QString arg);
		void pointsChanged();

	public slots:
		void setAddress(QString arg)
		{
			if (m_address == arg)
				return;

			m_address = arg;
			emit addressChanged(arg);
		}


	private:
		QString m_address;
		QMap<double, double> m_points;
};

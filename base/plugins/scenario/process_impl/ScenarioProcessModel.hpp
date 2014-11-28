#pragma once
#include <QObject>
#include <interface/process/ProcessSharedModelInterface.hpp>
#include <QDebug>

class ScenarioProcessModel : public iscore::ProcessSharedModelInterface
{
		Q_OBJECT
	public:
		ScenarioProcessModel(unsigned int id, QObject* parent);
		virtual ~ScenarioProcessModel();

		virtual QString processName() const override
		{
			return "Scenario_old";
		}

		virtual void serialize(QDataStream&) const override { }
		virtual void deserialize(QDataStream&) override { }

	public slots:
		void setText() { qDebug() << "Text set in process"; }
		void increment() { qDebug() << "ScenarioProcess::increment: " << ++m_counter; }
		void decrement() { qDebug() << "ScenarioProcess::decrement: " << --m_counter; }

	private:
		QString m_processText{"Text not set"};
		int m_counter{};

		// ProcessSharedModelInterface interface
	public:
		virtual iscore::ProcessViewModelInterface* makeViewModel(int id, QObject* parent);
};

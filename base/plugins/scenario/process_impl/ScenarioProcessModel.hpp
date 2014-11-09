#pragma once
#include <QObject>
#include <interface/process/ProcessModelInterface.hpp>
#include <QDebug>

class ScenarioProcessModel : public iscore::ProcessModelInterface
{
		Q_OBJECT
	public:
		ScenarioProcessModel(unsigned int id, QObject* parent);
		virtual ~ScenarioProcessModel();

	public slots:
		void setText() { qDebug() << "Text set in process"; }
		void increment() { qDebug() << "ScenarioProcess::increment: " << ++m_counter; }
		void decrement() { qDebug() << "ScenarioProcess::decrement: " << --m_counter; }

	private:
		QString m_processText{"Text not set"};
		int m_counter{};
};

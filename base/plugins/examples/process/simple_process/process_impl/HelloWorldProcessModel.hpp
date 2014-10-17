#pragma once
#include <QObject>
#include <interface/processes/Process.hpp>
#include <QDebug>

class HelloWorldProcessModel : public iscore::ProcessModel
{
		Q_OBJECT
	public:
		HelloWorldProcessModel();
		virtual ~HelloWorldProcessModel();

	public slots:
		void setText() { qDebug() << "Text set in process"; }
		void increment() { qDebug() << "HelloWorldProcess::increment: " << ++m_counter; }
		void decrement() { qDebug() << "HelloWorldProcess::decrement: " << --m_counter; }

	private:
		QString m_processText{"Text not set"};
		int m_counter{};
};

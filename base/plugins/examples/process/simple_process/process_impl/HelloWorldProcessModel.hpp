#pragma once
#include <QObject>
#include <interface/process/ProcessFactoryInterface.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>
#include <QDebug>

class HelloWorldProcessModel : public iscore::ProcessSharedModelInterface
{
		Q_OBJECT
	public:
		HelloWorldProcessModel(unsigned int id, QObject* parent);
		virtual ~HelloWorldProcessModel();

	public slots:
		void setText() { qDebug() << "Text set in process"; }
		void increment() { qDebug() << "HelloWorldProcess::increment: " << ++m_counter; }
		void decrement() { qDebug() << "HelloWorldProcess::decrement: " << --m_counter; }

	private:
		QString m_processText{"Text not set"};
		int m_counter{};
		
		// ProcessSharedModelInterface interface
	public:
		virtual ProcessViewModelInterface*makeViewModel(int id, QObject* parent);
};

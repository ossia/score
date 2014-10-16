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

	private:
		QString m_processText{"Text not set"};
};

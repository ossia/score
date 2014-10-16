#pragma once
#include <QObject>
class TimeProcess { };

namespace iscore
{
	class ProcessModel: public QObject
	{
		public:
			using QObject::QObject;
			virtual ~ProcessModel() = default;
		private:
			TimeProcess process;
	};

}

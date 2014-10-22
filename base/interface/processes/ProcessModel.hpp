#pragma once
#include <QObject>
class TimeProcess { };

namespace iscore
{
	class ProcessModel: public QObject
	{
		public:
			ProcessModel(unsigned int id, QObject* parent):
				QObject{parent},
				m_modelId{id}
			{
				
			}
			
			virtual ~ProcessModel() = default;
			
		private:
			TimeProcess process;
			const unsigned int m_modelId;
	};

}

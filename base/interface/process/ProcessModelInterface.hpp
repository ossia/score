#pragma once
#include <QObject>
class TimeProcess { };

namespace iscore
{
	class ProcessModelInterface: public QObject
	{
		public:
			ProcessModelInterface(unsigned int id, QObject* parent):
				QObject{parent},
				m_modelId{id}
			{
				
			}
			
			virtual ~ProcessModelInterface() = default;
			
		private:
			TimeProcess process;
			const unsigned int m_modelId;
	};

}

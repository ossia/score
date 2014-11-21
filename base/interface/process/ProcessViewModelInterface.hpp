#pragma once
#include <QObject>

namespace iscore
{
	/**
	 * @brief The ProcessViewModelInterface class
	 *
	 * Interface to implement to make a process view model.  
	 */
	class ProcessViewModelInterface: public QObject
	{
		public:
			ProcessViewModelInterface(int id, QObject* parent):
				QObject{parent},
				m_id{id}
			{
				
			}
			
			int id() const
			{ return m_id; }
			
			virtual ~ProcessViewModelInterface() = default;
			
		private:
			int m_id{};
	};

}

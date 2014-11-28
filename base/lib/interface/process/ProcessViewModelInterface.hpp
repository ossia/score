#pragma once
#include <QNamedObject>

namespace iscore
{
	/**
	 * @brief The ProcessViewModelInterface class
	 *
	 * Interface to implement to make a process view model.
	 */
	class ProcessViewModelInterface: public QIdentifiedObject
	{
		public:
			ProcessViewModelInterface(QObject* parent, QString name, int viewModelId, int sharedProcessId):
				QIdentifiedObject{parent, name, viewModelId},
				m_sharedProcessId{sharedProcessId}
			{

			}

			virtual ~ProcessViewModelInterface() = default;

			int sharedProcessId() const
			{ return m_sharedProcessId; }
			void setSharedProcessId(int id)
			{ m_sharedProcessId = id; }

			virtual void serialize(QDataStream&) const = 0;
			virtual void deserialize(QDataStream&) = 0;

		private:
			int m_sharedProcessId{};
	};

}


inline QDataStream& operator <<(QDataStream& s, const iscore::ProcessViewModelInterface& p)
{
	qDebug(Q_FUNC_INFO);
	s << p.id()
	  << p.objectName()
	  << p.sharedProcessId();

	p.serialize(s);
	return s;
}

inline QDataStream& operator >>(QDataStream& s, iscore::ProcessViewModelInterface& p)
{
	qDebug(Q_FUNC_INFO);
	int id;
	QString name;
	int sharedProcessId;
	s >> id
	  >> name
	  >> sharedProcessId;
	p.setId(id);
	p.setObjectName(name);
	p.setSharedProcessId(sharedProcessId);

	p.deserialize(s);
	return s;
}


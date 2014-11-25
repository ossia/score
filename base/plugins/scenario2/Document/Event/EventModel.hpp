#pragma once
#include <QNamedObject>

class EventModel : public QIdentifiedObject
{
	Q_OBJECT
	
	public:
		using QIdentifiedObject::QIdentifiedObject;
		virtual ~EventModel() = default;
		
	private:
	
};


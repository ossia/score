#pragma once
#include <QGraphicsObject>

namespace iscore
{
	class ProcessViewInterface : public QGraphicsObject
	{
		public:
			using QGraphicsObject::QGraphicsObject;
	};

	/* class ProcessSmallView { };
	class ProcessStandardView { };
	class ProcessFullView { }; */
}

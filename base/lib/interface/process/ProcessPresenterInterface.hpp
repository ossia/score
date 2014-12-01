#pragma once
#include <QNamedObject>

namespace iscore
{
	class ProcessPresenterInterface : public QNamedObject
	{
		public:
			using QNamedObject::QNamedObject;
			virtual ~ProcessPresenterInterface() = default;
	};
}

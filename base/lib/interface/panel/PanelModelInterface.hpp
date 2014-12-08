#pragma once
#include <tools/NamedObject.hpp>

namespace iscore
{
	class PanelPresenterInterface;
	class PanelModelInterface : public QNamedObject
	{
			Q_OBJECT
		public:
			using QNamedObject::QNamedObject;
			virtual ~PanelModelInterface() = default;
	};
}

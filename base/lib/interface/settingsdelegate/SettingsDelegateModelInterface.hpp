#pragma once
#include <QObject>

namespace iscore
{
	class SettingsDelegatePresenterInterface;
	class SettingsDelegateModelInterface : public QObject
	{
		public:
			using QObject::QObject;
			virtual ~SettingsDelegateModelInterface() = default;

			virtual void setFirstTimeSettings() = 0;
	};
}

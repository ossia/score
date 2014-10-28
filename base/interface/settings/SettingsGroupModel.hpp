#pragma once
#include <QObject>
namespace iscore
{
	class SettingsGroupPresenter;
	class SettingsGroupModel : public QObject
	{
		public:
			using QObject::QObject;
			virtual ~SettingsGroupModel() = default;

			virtual void setPresenter(SettingsGroupPresenter* presenter) = 0;
			virtual void setFirstTimeSettings() = 0;
	};
}

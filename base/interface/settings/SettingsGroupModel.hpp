#pragma once
#include <QObject>
namespace iscore
{
	class SettingsGroupPresenter;
	class SettingsGroupModel : public QObject
	{
		public:
			using QObject::QObject;
			virtual ~SettingsGroupModel()
			{
				this->setParent(nullptr);
			}

			virtual void setPresenter(SettingsGroupPresenter* presenter) = 0;
	};
}

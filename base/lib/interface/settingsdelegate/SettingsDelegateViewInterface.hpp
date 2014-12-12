#pragma once
#include <QWidget>

namespace iscore
{
	class SettingsDelegatePresenterInterface;

	class SettingsDelegateViewInterface : public QObject
	{
		public:
			using QObject::QObject;
			virtual ~SettingsDelegateViewInterface() = default;
			virtual void setPresenter(SettingsDelegatePresenterInterface* presenter)
			{
				m_presenter = presenter;
			}

			SettingsDelegatePresenterInterface* getPresenter()
			{
				return m_presenter;
			}

			virtual QWidget* getWidget() = 0; // QML? ownership transfer ? ? ? what about "this" case ?

		protected:
			SettingsDelegatePresenterInterface* m_presenter;
	};
}

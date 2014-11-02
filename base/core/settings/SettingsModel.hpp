#pragma once
#include <set>
#include <memory>
#include <interface/settings/SettingsGroup.hpp>
#include <interface/settings/SettingsGroupModel.hpp>
#include <QObject>
#include <QChildEvent>
#include <QDebug>
#include <QCoreApplication>
namespace iscore
{
	class SettingsModel : public QObject
	{
		public:
			using QObject::QObject;
			virtual ~SettingsModel()
			{
			}

			void addSettingsModel(SettingsGroupModel* model)
			{
				model->setParent(this); // TODO careful with double-deletion.
				m_pluginModels.insert(model);
			}

		protected:
			virtual void childEvent(QChildEvent* ev) override
			{/*
				if(ev->type() == QEvent::ChildAdded)
					qDebug() << "SettingsModel: Child added";
				*/
				QCoreApplication::sendEvent(parent(), ev);
			}

		private:
			std::set<SettingsGroupModel*> m_pluginModels;
	};
}

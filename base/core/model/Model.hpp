#pragma once
#include <QObject>
#include <set>

namespace iscore
{
	class PanelModel;
	class Model : public QObject
	{
		public:
			using QObject::QObject;
			void addPanel(PanelModel*);
			
		private:
			std::set<PanelModel*> m_panelsModels;
	};
}

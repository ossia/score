#include <core/document/DocumentModel.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <interface/panel/PanelModelInterface.hpp>
#include <exception>


using namespace iscore;

DocumentModel::DocumentModel(QObject* parent):
	NamedObject{"DocumentModel", parent}
{

}

void DocumentModel::reset()
{
	setModelDelegate(nullptr);
}

void DocumentModel::setModelDelegate(DocumentDelegateModelInterface* m)
{
	if(m_model)
	{
		delete m_model;
	}
	m_model = m;
}

PanelModelInterface* DocumentModel::panel(QString name) const
{
	using namespace std;
	auto it = find_if(begin(m_panelModels),
					  end(m_panelModels),
					  [&] (PanelModelInterface* pm)
	{ return pm->objectName() == name; });

	return it != end(m_panelModels) ? *it : nullptr;

}

namespace iscore
{
	DocumentModel* getDocumentFromObject(QObject* obj)
	{
		while(obj && obj->objectName() != QString{"DocumentModel"})
		{
			obj = obj->parent();
		}

		if(!obj)
			qDebug() << Q_FUNC_INFO << obj->objectName();
		else
			return static_cast<DocumentModel*>(obj);

		return nullptr;
	}
}

#include <core/document/DocumentModel.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
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

#include <core/document/DocumentModel.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
using namespace iscore;

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

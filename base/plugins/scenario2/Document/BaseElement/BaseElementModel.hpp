#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>

class BaseElementPresenter;
class IntervalModel;

class BaseElementModel : public iscore::DocumentDelegateModelInterface
{
	Q_OBJECT
	
	public:
		BaseElementModel(QObject* parent);
		virtual ~BaseElementModel() = default;
		
	private:
		IntervalModel* m_baseInterval{};
		
};


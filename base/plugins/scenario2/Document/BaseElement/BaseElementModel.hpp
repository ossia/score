#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>

class BaseElementModel : public iscore::DocumentDelegateModelInterface
{
	Q_OBJECT
	
	public:
	
		virtual ~BaseElementModel() = default;
		
	private:
	
};


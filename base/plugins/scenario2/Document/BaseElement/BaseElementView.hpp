#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>

class BaseElementView : public iscore::DocumentDelegateViewInterface
{
	Q_OBJECT
	
	public:
	
		virtual ~BaseElementView() = default;
		
	private:
	
};


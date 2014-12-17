#include "CurvePlugin.hpp"
#include <interface/customfactory/CustomFactoryInterface.hpp>

#include <base/plugins/scenario/source/ProcessInterface/ProcessFactoryInterface.hpp>
#include <base/plugins/scenario/source/ProcessInterface/ProcessViewInterface.hpp>
#include "plugincurve/include/plugincurve.hpp"
#include <source/ProcessInterface/ProcessPresenterInterface.hpp>
#include <source/ProcessInterface/ProcessSharedModelInterface.hpp>

class SmallCurve : public ProcessViewInterface
{
	public:
		virtual QRectF boundingRect() const
		{
			return {40, 40, 400, 400};
		}
		virtual void paint (QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
		{
		}
};
class pres : public ProcessPresenterInterface
{
		// ProcessPresenterInterface interface
	public:
		pres() : ProcessPresenterInterface ("CurvePresenter", nullptr) { }
		virtual int id() const
		{
			return 42;
		}
};

class mod : public ProcessSharedModelInterface
{
		// ProcessSharedModelInterface interface
	public:
		mod (int id) :
			ProcessSharedModelInterface (id, "CurveModel", nullptr)
		{

		}

		virtual QString processName() const
		{
			return "CurvePlugin";
		}

		virtual ProcessViewModelInterface* makeViewModel (int viewModelId,
		        int sharedProcessId,
		        QObject* parent)
		{
			return nullptr;
		}

		virtual ProcessViewModelInterface* makeViewModel (QDataStream& s,
		        QObject* parent)
		{
			return nullptr;
		}
};

class CurveProcessFactory: public ProcessFactoryInterface
{
	public:
		CurveProcessFactory()
		{
			curve = new PluginCurve (obj);
		}

		virtual QString name() const
		{
			return "CurvePlugin";
		}
		virtual QStringList availableViews()
		{
			return {};
		}

		virtual ProcessSharedModelInterface* makeModel (int id, QObject* parent)
		{
			return new mod (id);
		}

		virtual ProcessSharedModelInterface* makeModel (QDataStream&, QObject*)
		{
			return nullptr;
		}

		virtual ProcessViewInterface* makeView (QString view, QObject* parent)
		{

			return obj;
		}
		virtual ProcessPresenterInterface* makePresenter (ProcessViewModelInterface*, ProcessViewInterface*, QObject* parent)
		{
			return new pres;
		}

		PluginCurve* curve;
		SmallCurve* obj{new SmallCurve};
};


CurvePlugin::CurvePlugin() :
	QObject{}
{
}

QList<iscore::Autoconnect> CurvePlugin::autoconnect_list() const
{
	return
	{
	};
}


QVector<iscore::FactoryInterface*> CurvePlugin::factories_make (QString factoryName)
{
	if (factoryName == "Process")
	{
		return {new CurveProcessFactory};
	}
}


QStringList CurvePlugin::panel_list() const
{
	return {};
}

iscore::PanelFactoryInterface* CurvePlugin::panel_make (QString name)
{
	return nullptr;
}

QStringList CurvePlugin::control_list() const
{
	return {};
}

iscore::PluginControlInterface* CurvePlugin::control_make (QString name)
{
	return nullptr;
}



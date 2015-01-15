#include "DeckPresenter.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Commands/Constraint/Box/Deck/ResizeDeckVertically.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessPresenterInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "ProcessInterface/ProcessViewInterface.hpp"
#include "ProcessInterface/ProcessFactoryInterface.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/utilsCPP11.hpp>

#include <QGraphicsScene>

using namespace Scenario;

DeckPresenter::DeckPresenter(DeckModel* model,
								 DeckView* view,
								 QObject* parent):
	NamedObject{"DeckPresenter", parent},
	m_model{model},
	m_view{view}
{
	m_view->setHeight(m_model->height());

	for(ProcessViewModelInterface* proc_vm : m_model->processViewModels())
	{
		on_processViewModelCreated_impl(proc_vm);
	}

	connect(m_model, &DeckModel::processViewModelCreated,
			this,	 &DeckPresenter::on_processViewModelCreated);
	connect(m_model, &DeckModel::processViewModelRemoved,
			this,	 &DeckPresenter::on_processViewModelDeleted);
	connect(m_model, &DeckModel::heightChanged,
			this,	 &DeckPresenter::on_heightChanged);

	connect(m_view, &DeckView::bottomHandleChanged,
			this,	&DeckPresenter::on_bottomHandleChanged);
	connect(m_view, &DeckView::bottomHandleReleased,
			this,	&DeckPresenter::on_bottomHandleReleased);
	connect(m_view, &DeckView::bottomHandleSelected,
			this,	&DeckPresenter::on_bottomHandleSelected);
}

DeckPresenter::~DeckPresenter()
{
	if(m_view)
	{
		auto sc = m_view->scene();
		if(sc) sc->removeItem(m_view);
		m_view->deleteLater();
	}
}

int DeckPresenter::id() const
{
	return (SettableIdentifier::identifier_type) m_model->id();
}

int DeckPresenter::height() const
{
	return m_view->height();
}

int DeckPresenter::position() const
{
	return m_model->position();
}

void DeckPresenter::setVerticalPosition(int pos)
{
	auto view_pos = m_view->pos();
	if(view_pos.y() != pos)
	{
		m_view->setPos(view_pos.x(), pos);
		m_view->update();
	}
}

void DeckPresenter::on_processViewModelCreated(int processId)
{
	on_processViewModelCreated_impl(m_model->processViewModel(processId));
}

void DeckPresenter::on_processViewModelDeleted(int processId)
{
	vec_erase_remove_if(m_processes,
						[&processId] (ProcessPresenterInterface* pres)
						{
							bool to_delete = pres->viewModelId() == processId;
							if(to_delete) delete pres;
							return to_delete;
						} );


	emit askUpdate();
}

void DeckPresenter::on_heightChanged(int height)
{
	m_view->setHeight(height);
	emit askUpdate();
}

void DeckPresenter::on_bottomHandleSelected()
{

}

void DeckPresenter::on_bottomHandleChanged(int newHeight)
{
	on_heightChanged(newHeight);
}

void DeckPresenter::on_bottomHandleReleased()
{
	auto path = ObjectPath::pathFromObject("BaseConstraintModel", m_model);

	auto cmd = new Command::ResizeDeckVertically{std::move(path), m_view->height()};
	emit submitCommand(cmd);
}

void DeckPresenter::on_processViewModelCreated_impl(ProcessViewModelInterface* proc_vm)
{
	auto procname = proc_vm->sharedProcessModel()->processName();

	auto factory = ProcessList::getFactory(procname);

	auto proc_view = factory->makeView(factory->availableViews().first(), m_view);
	proc_view->setPos(5, 5);
	auto presenter = factory->makePresenter(proc_vm, proc_view, this);

	connect(presenter,	&ProcessPresenterInterface::submitCommand,
			this,		&DeckPresenter::submitCommand);
	connect(presenter,	&ProcessPresenterInterface::elementSelected,
			this,		&DeckPresenter::elementSelected);

	m_processes.push_back(presenter);
}

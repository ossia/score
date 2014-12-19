#include "StoreyPresenter.hpp"

#include "Document/Constraint/Box/Storey/StoreyModel.hpp"
#include "Document/Constraint/Box/Storey/StoreyView.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Commands/Constraint/Box/Deck/ResizeDeckVerticallyCommand.hpp"

#include "Control/ProcessList.hpp"
#include "ProcessInterface/ProcessPresenterInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "ProcessInterface/ProcessViewInterface.hpp"
#include "ProcessInterface/ProcessFactoryInterface.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/utilsCPP11.hpp>

#include <QGraphicsScene>

// @todo vérifier en créant un nouvel élément
// qu'il n'existe pas déjà dans un tableau.
StoreyPresenter::StoreyPresenter(StoreyModel* model,
								 StoreyView* view,
								 QObject* parent):
	NamedObject{"StoreyPresenter", parent},
	m_model{model},
	m_view{view}
{
	m_view->setHeight(m_model->height());

	for(ProcessViewModelInterface* proc_vm : m_model->processViewModels())
	{
		on_processViewModelCreated_impl(proc_vm);
	}

	connect(m_model, &StoreyModel::processViewModelCreated,
			this,	 &StoreyPresenter::on_processViewModelCreated);
	connect(m_model, &StoreyModel::processViewModelRemoved,
			this,	 &StoreyPresenter::on_processViewModelDeleted);
	connect(m_model, &StoreyModel::heightChanged,
			this,	 &StoreyPresenter::on_heightChanged);

	connect(m_view, &StoreyView::bottomHandleChanged,
			this,	&StoreyPresenter::on_bottomHandleChanged);
	connect(m_view, &StoreyView::bottomHandleReleased,
			this,	&StoreyPresenter::on_bottomHandleReleased);
	connect(m_view, &StoreyView::bottomHandleSelected,
			this,	&StoreyPresenter::on_bottomHandleSelected);
}

StoreyPresenter::~StoreyPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}

int StoreyPresenter::id() const
{
	return m_model->id();
}

int StoreyPresenter::height() const
{
	return m_view->height();
}

int StoreyPresenter::position() const
{
	return m_model->position();
}

void StoreyPresenter::setVerticalPosition(int pos)
{
	auto view_pos = m_view->pos();
	if(view_pos.y() != pos)
	{
		m_view->setPos(view_pos.x(), pos);
		m_view->update();
	}
}

void StoreyPresenter::on_processViewModelCreated(int processId)
{
	qDebug(Q_FUNC_INFO);
	on_processViewModelCreated_impl(m_model->processViewModel(processId));
}

void StoreyPresenter::on_processViewModelDeleted(int processId)
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

void StoreyPresenter::on_heightChanged(int height)
{
	m_view->setHeight(height);
	emit askUpdate();
}

void StoreyPresenter::on_bottomHandleSelected()
{

}

void StoreyPresenter::on_bottomHandleChanged(int newHeight)
{
	on_heightChanged(newHeight);
}

void StoreyPresenter::on_bottomHandleReleased()
{
	auto path = ObjectPath::pathFromObject("BaseConstraintModel", m_model);

	auto cmd = new ResizeDeckVerticallyCommand(std::move(path), m_view->height());
	emit submitCommand(cmd);
}

void StoreyPresenter::on_processViewModelCreated_impl(ProcessViewModelInterface* proc_vm)
{
	auto procname = m_model
						->parentConstraint()
						->process(proc_vm->sharedProcessId())
						->processName();

	auto factory = ProcessList::getFactory(procname);

	auto proc_view = factory->makeView(factory->availableViews().first(), m_view);
	proc_view->setPos(5, 5);
	auto presenter = factory->makePresenter(proc_vm, proc_view, this);

	connect(presenter,	&ProcessPresenterInterface::submitCommand,
			this,		&StoreyPresenter::submitCommand);
	connect(presenter,	&ProcessPresenterInterface::elementSelected,
			this,		&StoreyPresenter::elementSelected);

	m_processes.push_back(presenter);
}

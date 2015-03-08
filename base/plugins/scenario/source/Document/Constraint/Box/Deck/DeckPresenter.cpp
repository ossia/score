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

#include "public_interface/document/DocumentInterface.hpp"
#include <public_interface/command/OngoingCommandManager.hpp>
#include <public_interface/command/SerializableCommand.hpp>
#include <public_interface/tools/utilsCPP11.hpp>

#include <QGraphicsScene>

using namespace Scenario;

DeckPresenter::DeckPresenter(DeckModel* model,
                             DeckView* view,
                             QObject* parent) :
    NamedObject {"DeckPresenter", parent},
    m_model {model},
    m_view {view},
    m_commandDispatcher{new CommandDispatcher<>{this}}
{
    for(ProcessViewModelInterface* proc_vm : m_model->processViewModels())
    {
        on_processViewModelCreated_impl(proc_vm);
    }

    connect(m_model, &DeckModel::processViewModelCreated,
    this,	 &DeckPresenter::on_processViewModelCreated);
    connect(m_model, &DeckModel::processViewModelRemoved,
    this,	 &DeckPresenter::on_processViewModelDeleted);

    connect(m_model, &DeckModel::processViewModelSelected,
    this,	 &DeckPresenter::on_processViewModelSelected);

    connect(m_model, &DeckModel::heightChanged,
    this,	 &DeckPresenter::on_heightChanged);

    connect(m_view, &DeckView::bottomHandleChanged,
    this,	&DeckPresenter::on_bottomHandleChanged);
    connect(m_view, &DeckView::bottomHandleReleased,
    this,	&DeckPresenter::on_bottomHandleReleased);
    connect(m_view, &DeckView::bottomHandleSelected,
    this,	&DeckPresenter::on_bottomHandleSelected);

    m_view->setHeight(m_model->height());
}

DeckPresenter::~DeckPresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

id_type<DeckModel> DeckPresenter::id() const
{
    return m_model->id();
}

int DeckPresenter::height() const
{
    return m_view->height();
}

void DeckPresenter::setWidth(int w)
{
    m_view->setWidth(w);
    updateProcessesShape();
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


void DeckPresenter::on_processViewModelCreated(id_type<ProcessViewModelInterface> processId)
{
    on_processViewModelCreated_impl(m_model->processViewModel(processId));
}

void DeckPresenter::on_processViewModelDeleted(id_type<ProcessViewModelInterface> processId)
{
    vec_erase_remove_if(m_processes,
                        [&processId](ProcessPresenterInterface * pres)
    {
        bool to_delete = pres->viewModelId() == processId;

        if(to_delete)
        {
            delete pres;
        }

        return to_delete;
    });


    updateProcessesShape();
    emit askUpdate();
}

void DeckPresenter::on_processViewModelSelected(id_type<ProcessViewModelInterface> processId)
{
    // Put the selected one at z+1 and the others at z+2; set "disabled" graphics mode.
    for(auto& pvm : m_processes)
    {
        if(pvm->viewModelId() == processId)
        {
            pvm->putToFront();
        }
        else
        {
            pvm->putBehind();
        }
    }
}

void DeckPresenter::on_heightChanged(int height)
{
    m_view->setHeight(height);
    updateProcessesShape();

    emit askUpdate();
}

void DeckPresenter::on_parentGeometryChanged()
{
    updateProcessesShape();
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
    auto path = iscore::IDocument::path(m_model);

    auto cmd = new Command::ResizeDeckVertically {std::move(path),
               m_view->height()};
    emit m_commandDispatcher->submitCommand(cmd);
}

void DeckPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    for(ProcessPresenterInterface* proc : m_processes)
    {
        proc->on_zoomRatioChanged(m_zoomRatio);
    }
}

void DeckPresenter::on_processViewModelCreated_impl(ProcessViewModelInterface* proc_vm)
{
    auto procname = proc_vm->sharedProcessModel()->processName();

    auto factory = ProcessList::getFactory(procname);

    auto proc_view = factory->makeView(factory->availableViews().first(), m_view);
    proc_view->setPos(0, 0);
    auto presenter = factory->makePresenter(proc_vm, proc_view, this);

    presenter->on_zoomRatioChanged(m_zoomRatio);

    m_processes.push_back(presenter);
    updateProcessesShape();
}

void DeckPresenter::updateProcessesShape()
{
    for(ProcessPresenterInterface* proc : m_processes)
    {
        proc->setHeight(height() - DeckView::borderHeight());
        proc->setWidth(m_view->width());
        proc->parentGeometryChanged();
    }
}

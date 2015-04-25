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

#include "iscore/document/DocumentInterface.hpp"
#include <iscore/command/OngoingCommandManager.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/utilsCPP11.hpp>

#include "Document/Constraint/Box/BoxView.hpp"
#include <QGraphicsScene>

using namespace Scenario;

DeckPresenter::DeckPresenter(DeckModel* model,
                             BoxView *view,
                             QObject* parent) :
    NamedObject {"DeckPresenter", parent},
    m_model {model},
    m_view {new DeckView{*this, view}}
{
    m_view->setPos(0, 0);

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

const DeckModel &DeckPresenter::model() const
{ return *m_model; }

int DeckPresenter::height() const
{
    return m_view->height();
}

void DeckPresenter::setWidth(double w)
{
    m_view->setWidth(w);
    updateProcessesShape();
}

void DeckPresenter::setVerticalPosition(double pos)
{
    auto view_pos = m_view->pos();

    if(view_pos.y() != pos)
    {
        m_view->setPos(view_pos.x(), pos);
        m_view->update();
    }
}

void DeckPresenter::enable()
{
    m_view->enable();
    for(auto& pair : m_processes)
    {
        pair.first->parentGeometryChanged();
    }

    m_enabled = true;
}

void DeckPresenter::disable()
{
    m_view->disable();
    for(auto& pair : m_processes)
    {
        pair.first->parentGeometryChanged();
    }

    m_enabled = false;
}


void DeckPresenter::on_processViewModelCreated(id_type<ProcessViewModelInterface> processId)
{
    on_processViewModelCreated_impl(m_model->processViewModel(processId));
}

void DeckPresenter::on_processViewModelDeleted(id_type<ProcessViewModelInterface> processId)
{
    vec_erase_remove_if(m_processes,
                        [&processId](auto& pair)
    {
        bool to_delete = pair.first->viewModelId() == processId;

        if(to_delete)
        {
            delete pair.second;
            delete pair.first;
        }

        return to_delete;
    });


    updateProcessesShape();
    emit askUpdate();
}

void DeckPresenter::on_processViewModelSelected(id_type<ProcessViewModelInterface> processId)
{
    // Put the selected one at z+1 and the others at z+2; set "disabled" graphics mode.
    for(auto& pair : m_processes)
    {
        if(pair.first->viewModelId() == processId)
        {
            pair.first->putToFront();
        }
        else
        {
            pair.first->putBehind();
        }
    }
}

void DeckPresenter::on_heightChanged(double height)
{
    m_view->setHeight(height);
    updateProcessesShape();

    emit askUpdate();
}

void DeckPresenter::on_parentGeometryChanged()
{
    updateProcessesShape();
}

void DeckPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    for(auto& pair: m_processes)
    {
        pair.first->on_zoomRatioChanged(m_zoomRatio);
    }
}

void DeckPresenter::on_processViewModelCreated_impl(ProcessViewModelInterface* proc_vm)
{
    auto procname = proc_vm->sharedProcessModel()->processName();

    auto factory = ProcessList::getFactory(procname);

    auto proc_view = factory->makeView(proc_vm, m_view);
    auto proc_pres = factory->makePresenter(proc_vm, proc_view, this);

    proc_pres->on_zoomRatioChanged(m_zoomRatio);

    if(m_enabled)
        m_view->enable();
    else
        m_view->disable();

    m_processes.push_back({proc_pres, proc_view});
    updateProcessesShape();
}

void DeckPresenter::updateProcessesShape()
{
    for(auto& pair : m_processes)
    {
        pair.first->setHeight(height() - DeckView::handleHeight());
        pair.first->setWidth(m_view->width());
        pair.first->parentGeometryChanged();
    }
}

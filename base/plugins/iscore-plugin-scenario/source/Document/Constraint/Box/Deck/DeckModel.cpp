#include "DeckModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"

DeckModel::DeckModel(
        const id_type<DeckModel>& id,
        BoxModel* parent) :
    IdentifiedObject<DeckModel> {id, "DeckModel", parent}
{
}

DeckModel::DeckModel(
        std::function<void(const DeckModel&, DeckModel&)> pvmCopyMethod,
        const DeckModel& source,
        const id_type<DeckModel>& id,
        BoxModel *parent):
    IdentifiedObject<DeckModel> {id, "DeckModel", parent},
    m_editedProcessViewModelId {source.editedProcessViewModel() }, // Keep the same id.
    m_height {source.height() }
{
    pvmCopyMethod(source, *this);
}

void DeckModel::copyViewModelsInSameConstraint(
        const DeckModel &source,
        DeckModel &target)
{
    for(ProcessViewModel* pvm : source.processViewModels())
    {
        // We can safely reuse the same id since it's in a different deck.
        auto& proc = pvm->sharedProcessModel();
        target.addProcessViewModel(
                    proc.cloneViewModel(pvm->id(),
                                        *pvm,
                                        &target));
    }
}

void DeckModel::addProcessViewModel(ProcessViewModel* viewmodel)
{
    m_processViewModels.push_back(viewmodel);

    emit processViewModelCreated(viewmodel->id());

    selectForEdition(viewmodel->id());
}

void DeckModel::deleteProcessViewModel(
        const id_type<ProcessViewModel>& processViewId)
{
    auto& pvm = processViewModel(processViewId);

    vec_erase_remove_if(m_processViewModels,
                        [&processViewId](ProcessViewModel * model)
    {
        return model->id() == processViewId;
    });

    emit processViewModelRemoved(processViewId);


    if(!m_processViewModels.empty())
    {
        selectForEdition((*m_processViewModels.begin())->id());
    }
    else
    {
        m_editedProcessViewModelId.setVal({});
    }

    delete &pvm;
}

void DeckModel::selectForEdition(
        const id_type<ProcessViewModel>& processViewId)
{
    if(!processViewId.val())
        return;

    if(processViewId != m_editedProcessViewModelId)
    {
        m_editedProcessViewModelId = processViewId;
        emit processViewModelSelected(processViewId);
    }
}

const id_type<ProcessViewModel>& DeckModel::editedProcessViewModel() const
{
    return m_editedProcessViewModelId;
}

const std::vector<ProcessViewModel*>& DeckModel::processViewModels() const
{
    return m_processViewModels;
}

ProcessViewModel& DeckModel::processViewModel(
        const id_type<ProcessViewModel>& processViewModelId) const
{
    return *findById(m_processViewModels, processViewModelId);
}

void DeckModel::on_deleteSharedProcessModel(
        const id_type<ProcessModel>& sharedProcessId)
{
    using namespace std;
    auto it = find_if(begin(m_processViewModels),
                      end(m_processViewModels),
                      [&sharedProcessId](const ProcessViewModel * pvm)
    {
        return pvm->sharedProcessModel().id() == sharedProcessId;
    });

    if(it != end(m_processViewModels))
    {
        deleteProcessViewModel((*it)->id());
    }
}

void DeckModel::setHeight(int arg)
{
    if(m_height != arg)
    {
        m_height = arg;
        emit heightChanged(arg);
    }
}

void DeckModel::setFocus(bool arg)
{
    if (m_focus == arg)
        return;

    m_focus = arg;
    emit focusChanged(arg);
}

ConstraintModel& DeckModel::parentConstraint() const
{
    return static_cast<ConstraintModel&>(*parent()->parent());
}

int DeckModel::height() const
{
    return m_height;
}

bool DeckModel::focus() const
{
    return m_focus;
}


std::tuple<int, int, int> identifierOfProcessViewModelFromConstraint(ProcessViewModel* pvm)
{
    // TODO - this only works in a scenario.
    auto deckModel = static_cast<IdentifiedObjectAbstract*>(pvm->parent());
    auto boxModel = static_cast<IdentifiedObjectAbstract*>(deckModel->parent());
    return std::tuple<int, int, int>
    {
        boxModel->id_val(),
                deckModel->id_val(),
                pvm->id_val()
    };
}


QDataStream&operator<<(QDataStream& s, const std::tuple<int, int, int>& tuple)
{
    s << std::get<0> (tuple) << std::get<1> (tuple) << std::get<2> (tuple);
    return s;
}


QDataStream&operator>>(QDataStream& s, std::tuple<int, int, int>& tuple)
{
    s >> std::get<0> (tuple) >> std::get<1> (tuple) >> std::get<2> (tuple);
    return s;
}


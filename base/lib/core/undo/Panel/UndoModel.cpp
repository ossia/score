#include "UndoModel.hpp"
#include <core/document/DocumentModel.hpp>

UndoModel::UndoModel(iscore::DocumentModel* model) :
    iscore::PanelModelInterface{"UndoPanelModel", model}
{

}

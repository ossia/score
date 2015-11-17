#include "LoopInspectorWidget.hpp"
#include <Loop/LoopProcessModel.hpp>
#include <Inspector/InspectorSectionWidget.hpp>
#include "Loop/Commands/EditScript.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <iscore/widgets/SpinBoxes.hpp>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QApplication>

LoopInspectorWidget::LoopInspectorWidget(
        const LoopProcessModel& LoopModel,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {LoopModel, doc, parent},
    m_model {LoopModel}
{
    setObjectName("LoopInspectorWidget");
    setParent(parent);


    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), {});
}

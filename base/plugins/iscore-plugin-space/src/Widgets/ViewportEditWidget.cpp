#include "ViewportEditWidget.hpp"

#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

ViewportEditWidget::ViewportEditWidget(const SpaceModel& sp, const ViewportModel& vp, QWidget* parent):
    QWidget{parent},
    m_viewport{vp}
{
    auto lay = new MarginLess<QFormLayout>;

    m_name = new QLineEdit{m_viewport.name()};
    lay->addRow(tr("Name"), m_name);

    m_zoom = new QDoubleSpinBox;
    m_zoom->setMinimum(0.5);
    m_zoom->setMaximum(10);
    lay->addRow(tr("Zoom"), m_zoom);

    m_rotation = new QDoubleSpinBox;
    m_rotation->setMinimum(0);
    m_rotation->setMaximum(360);
    m_rotation->setWrapping(true);
    lay->addRow(tr("Rotation"), m_rotation);

    m_topleftX = new MaxRangeSpinBox<TemplatedSpinBox<double>>;
    lay->addRow(tr("Top Left (X)"), m_topleftX);
    m_topleftY = new MaxRangeSpinBox<TemplatedSpinBox<double>>;
    lay->addRow(tr("Top Left (Y)"), m_topleftY);

    // Add all the dimensions in our current space.
    m_dimX = new QComboBox;
    for(const auto& dim : sp.dimensions())
    {
        m_dimX->addItem(dim.name(), QString::number(*dim.id().val()));
    }
    lay->addRow("Dimension (x)", m_dimX);

    m_dimY = new QComboBox;
    for(const auto& dim : sp.dimensions())
    {
        m_dimY->addItem(dim.name(), QString::number(*dim.id().val()));
    }
    lay->addRow("Dimension (y)", m_dimY);

    // Default values for the remaining dimensions (gray the other out like in the area widget).
    ISCORE_TODO;
    m_remove = new QPushButton{tr("Remove")};
    lay->addRow(tr("Remove"), m_remove);

}

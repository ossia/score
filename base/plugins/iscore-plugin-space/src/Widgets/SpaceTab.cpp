#include "SpaceTab.hpp"
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include "src/Space/SpaceModel.hpp"
#include "src/Space/DimensionModel.hpp"
#include "ViewportEditWidget.hpp"
#include "DimensionEditWidget.hpp"

SpaceTab::SpaceTab(const SpaceModel& space, QWidget *parent):
    QWidget{parent},
    m_space{space}
{
    auto lay = new iscore::MarginLess<QGridLayout>;
    this->setLayout(lay);

    // Widgets relative to the space & dimension properties
    {
        auto dimBox = new QGroupBox{tr("Dimensions")};
        lay->addWidget(dimBox);
        m_dimensionLayout = new iscore::MarginLess<QVBoxLayout>;

        for(const auto& dim : m_space.dimensions())
        {
            m_dimensionLayout->addWidget(new DimensionEditWidget{dim, this});
        }

        m_addDim = new QPushButton{tr("+")};
        m_dimensionLayout->addWidget(m_addDim);
        m_dimensionLayout->addStretch();

        dimBox->setLayout(m_dimensionLayout);
    }

    // Widgets relative to the viewports
    {
        auto viewportBox = new QGroupBox{tr("Viewports")};
        lay->addWidget(viewportBox);
        m_viewportLayout = new iscore::MarginLess<QVBoxLayout>;

        for(const auto& vp : m_space.viewports())
        {
            m_viewportLayout->addWidget(new ViewportEditWidget{m_space, vp, this});
        }

        m_addViewport = new QPushButton{tr("+")};
        m_viewportLayout->addWidget(m_addViewport);
        m_viewportLayout->addStretch();

        viewportBox->setLayout(m_viewportLayout);
    }

    // In the inspector for the space, choose the default viewport for the process.

    // Set the number of dimensions
    // Name them
    // Set their bounds
    // Choose the mapping between our dimensions
    // and the viewport

    // For each viewport :
    //  - Which space dimension maps to viewport dimension
    //  - What are the default values for unmapped dimensions.

}

#include "SpaceTab.hpp"
#include <iscore/widgets/MarginLess.hpp>

#include "src/Space/SpaceModel.hpp"
#include "src/Space/DimensionModel.hpp"

class ViewportEditWidget : public QWidget
{
    public:
        ViewportEditWidget(const ViewportModel& vp, QWidget* parent):
            QWidget{parent},
            m_viewport{vp}
        {
            ISCORE_TODO;
        }

    private:

        const ViewportModel& m_viewport;
};

class DimensionEditWidget : public QWidget
{
    public:
        DimensionEditWidget(const DimensionModel& dim, QWidget* parent):
            QWidget{parent},
            m_dim{dim}
        {
            auto lay = new MarginLess<QHBoxLayout>;

            m_name = new QLineEdit{m_dim.name()};
            lay->addWidget(m_name);

            m_minBound = new QDoubleSpinBox;
            m_minBound->setMinimum(std::numeric_limits<float>::lowest());
            m_minBound->setMaximum(std::numeric_limits<float>::max());
            m_minBound->setValue(m_dim.sym().domain().min);
            lay->addWidget(m_minBound);

            m_maxBound = new QDoubleSpinBox;
            m_maxBound->setMinimum(std::numeric_limits<float>::lowest());
            m_maxBound->setMaximum(std::numeric_limits<float>::max());
            m_maxBound->setValue(m_dim.sym().domain().max);
            lay->addWidget(m_maxBound);

            m_remove = new QPushButton(tr("X"));
            lay->addWidget(m_remove);

            this->setLayout(lay);
        }

    private:
        const DimensionModel& m_dim;
        QLineEdit* m_name{};
        QDoubleSpinBox* m_minBound{};
        QDoubleSpinBox* m_maxBound{};
        QPushButton* m_remove{};
};

SpaceTab::SpaceTab(const SpaceModel& space, QWidget *parent):
    QWidget{parent},
    m_space{space}
{
    auto lay = new MarginLess<QGridLayout>;
    this->setLayout(lay);

    // Widgets relative to the space & dimension properties
    {
        auto dimBox = new QGroupBox{tr("Dimensions")};
        lay->addWidget(dimBox);
        m_dimensionLayout = new MarginLess<QVBoxLayout>;

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
        m_viewportLayout = new MarginLess<QVBoxLayout>;

        for(const auto& vp : m_space.viewports())
        {
            m_viewportLayout->addWidget(new ViewportEditWidget{vp, this});
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

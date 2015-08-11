#include "SpaceTab.hpp"
#include <iscore/widgets/MarginLess.hpp>

#include "src/Space/SpaceModel.hpp"
#include "src/Space/DimensionModel.hpp"

class ViewportEditWidget : public QWidget
{
    public:
        using QWidget::QWidget;
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
            m_minBound->setValue(m_dim.sym().domain().max);
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
        QGroupBox* dimBox = new QGroupBox;
        lay->addWidget(dimBox);
        m_dimensionLayout = new MarginLess<QVBoxLayout>;

        for(const auto& dim : m_space.dimensions())
        {
            m_dimensionLayout->addWidget(new DimensionEditWidget{dim, this});
        }

        m_addDim = new QPushButton{tr("+")};
        m_dimensionLayout->addWidget(m_addDim);

        dimBox->setLayout(m_dimensionLayout);
    }

    // Widgets relative to the viewports
    {
        QGroupBox* viewportBox = new QGroupBox;
        lay->addWidget(viewportBox);
        m_viewportLayout = new MarginLess<QVBoxLayout>;

        for(const auto& dim : m_space.dimensions())
        {
            m_viewportLayout->addWidget(new ViewportEditWidget{this});
        }

        m_addDim = new QPushButton{tr("+")};
        m_viewportLayout->addWidget(m_addDim);

        viewportBox->setLayout(m_viewportLayout);
    }

    // Set the number of dimensions
    // Name them
    // Set their bounds
    // Choose the mapping between our dimensions
    // and the viewport

    // What about multiple viewports ???
    // Make a "viewports" tab ? Woohooo

    // For each viewport :
    //  - Which space dimension maps to viewport dimension
    //  - What are the default values for unmapped dimensions.

}

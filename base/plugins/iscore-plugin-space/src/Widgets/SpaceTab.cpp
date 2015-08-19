#include "SpaceTab.hpp"
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include "src/Space/SpaceModel.hpp"
#include "src/Space/DimensionModel.hpp"

class ViewportEditWidget : public QWidget
{
    public:
        ViewportEditWidget(const SpaceModel& sp, const ViewportModel& vp, QWidget* parent):
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

    private:
        const ViewportModel& m_viewport;

        QLineEdit* m_name{};
        QDoubleSpinBox* m_zoom{};
        QDoubleSpinBox* m_rotation{};

        QDoubleSpinBox* m_topleftX{};
        QDoubleSpinBox* m_topleftY{};

        QComboBox* m_dimX{};
        QComboBox* m_dimY{};

        QVector<QComboBox*> m_parameterCBs;
        QPushButton* m_remove{};

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

            m_minBound = new MaxRangeSpinBox<TemplatedSpinBox<float>>;
            m_minBound->setValue(m_dim.sym().domain().min);
            lay->addWidget(m_minBound);

            m_maxBound = new MaxRangeSpinBox<TemplatedSpinBox<float>>;
            m_maxBound->setValue(m_dim.sym().domain().max);
            lay->addWidget(m_maxBound);

            m_remove = new QPushButton{tr("Remove")};
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

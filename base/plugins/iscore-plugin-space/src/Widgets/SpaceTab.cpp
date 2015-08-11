#include "SpaceTab.hpp"
#include <iscore/widgets/MarginLess.hpp>


class DimensionEditWidget : public QWidget
{
    public:
        DimensionEditWidget(QWidget* parent):
            QWidget{parent}
        {
            auto lay = new MarginLess<QHBoxLayout>;

            m_name = new QLineEdit;
            lay->addWidget(m_name);

            m_minBound = new QDoubleSpinBox;
            m_minBound->setMinimum(std::numeric_limits<float>::lowest());
            m_minBound->setMaximum(std::numeric_limits<float>::max());
            lay->addWidget(m_minBound);

            m_maxBound = new QDoubleSpinBox;
            m_maxBound->setMinimum(std::numeric_limits<float>::lowest());
            m_maxBound->setMaximum(std::numeric_limits<float>::max());
            lay->addWidget(m_maxBound);
        }

    private:
        QLineEdit* m_name{};
        QDoubleSpinBox* m_minBound{};
        QDoubleSpinBox* m_maxBound{};
};

SpaceTab::SpaceTab(QWidget *parent):
    QWidget{parent}
{
    auto lay = new MarginLess<QGridLayout>;
    this->setLayout(lay);

    {
        QGroupBox* dimBox = new QGroupBox;
        lay->addWidget(dimBox);
        m_dimensionLayout = new MarginLess<QVBoxLayout>;
        /*
        dimBox->setLayout(m_spaceMappingLayout);
        for(const DimensionModel& dim : m_space.space().dimensions())
        {
            auto cb = new QComboBox;
            m_spaceMappingLayout->addRow(dim.name(), cb);
            connect(cb,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &AreaWidget::on_dimensionMapped);
        }
        */
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

    // A viewport is :
    struct viewport
    {
        public:
            int zoomLevel;
            int orientation;
            int center; // or topleft ?

            QMap<QString, QString> dimensionsMap;
            QMap<QString, double> defaultValuesMap;
    };
}

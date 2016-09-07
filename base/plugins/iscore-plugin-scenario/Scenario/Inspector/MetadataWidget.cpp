#include "MetadataWidget.hpp"

#include <iscore/model/ModelMetadata.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Inspector/ExtendedMetadataWidget.hpp>
#include <Scenario/Inspector/CommentEdit.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QtColorWidgets/color_palette_widget.hpp>

#include <QBoxLayout>
#include <QColorDialog>
#include <QFormLayout>
#include <QMenu>
#include <QWidgetAction>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QSize>

namespace Scenario
{
MetadataWidget::MetadataWidget(
        const iscore::ModelMetadata& metadata,
        const iscore::CommandStackFacade& m,
        const QObject *docObject,
        QWidget* parent) :
    QWidget(parent),
    m_metadata {metadata},
    m_commandDispatcher{m}
{
    // main
    auto metadataLayout = new iscore::MarginLess<QVBoxLayout>{this};
    metadataLayout->setSizeConstraint(QLayout::SetMinimumSize);

    // btn
    auto btnLay = new iscore::MarginLess<QVBoxLayout>;

    // header
    auto headerLay = new iscore::MarginLess<QHBoxLayout>;

    // Name(s)
    auto descriptionWidget = new QWidget {this};
    auto descriptionLay = new iscore::MarginLess<QFormLayout>{descriptionWidget};

    m_scriptingNameLine = new QLineEdit{metadata.getName(), this};
    m_labelLine = new QLineEdit{metadata.getLabel(), this};

    descriptionLay->addRow("Name", m_scriptingNameLine);
    descriptionLay->addRow("Label", m_labelLine);

    descriptionWidget->setObjectName("Description");

     // color
    m_colorButton = new QPushButton{};
    m_colorButton->setMaximumSize(QSize(1.5 * m_colorIconSize, 1.5 * m_colorIconSize));
    m_colorButton->setIconSize(QSize(m_colorIconSize, m_colorIconSize));
    m_colorButtonPixmap.fill(metadata.getColor().getColor());
    m_colorButton->setIcon(QIcon(m_colorButtonPixmap));


    // comments
    m_comments = new CommentEdit{metadata.getComment(), this};
    m_comments->setVisible(false);

    m_cmtBtn = new QToolButton{};
    m_cmtBtn->setArrowType(Qt::RightArrow);

    m_meta = new ExtendedMetadataWidget{metadata.getExtendedMetadata(), this};
    m_meta->setVisible(false);

    btnLay->addWidget(m_colorButton);
    btnLay->addWidget(m_cmtBtn);

    headerLay->addWidget(descriptionWidget);
    headerLay->addLayout(btnLay);

    metadataLayout->addLayout(headerLay);
    metadataLayout->addWidget(m_comments);

    metadataLayout->addWidget(m_meta);

    connect(m_cmtBtn, &QToolButton::released,
            this,  [&] ()
    {
        m_cmtExpanded = !m_cmtExpanded;
        m_comments->setVisible(m_cmtExpanded);
        m_meta->setVisible(m_cmtExpanded);
        if(m_cmtExpanded)
            m_cmtBtn->setArrowType(Qt::DownArrow);
        else
            m_cmtBtn->setArrowType(Qt::RightArrow);
    });

    connect(m_scriptingNameLine, &QLineEdit::editingFinished,
            [ = ]()
    {
        emit scriptingNameChanged(m_scriptingNameLine->text());
    });

    connect(m_labelLine, &QLineEdit::editingFinished,
            [ = ]()
    {
        emit labelChanged(m_labelLine->text());
    });

    connect(m_comments, &CommentEdit::editingFinished,
            [ = ]()
    {
        emit commentsChanged(m_comments->toPlainText());
    });

    connect(m_meta, &ExtendedMetadataWidget::dataChanged,
            this, [=] () {
        emit extendedMetadataChanged(m_meta->currentMap());
    }, Qt::QueuedConnection);

    {
        using namespace color_widgets;

        auto palette_widget = new ColorPaletteWidget;
        delete m_palette;
        m_palette = new ColorPaletteModel;
        ColorPalette palette1;
        palette1.setColors(iscore::Skin::instance().getColors());
        palette1.setName("Choose a color");
        m_palette->addPalette(palette1, false);

        palette_widget->setModel(m_palette);
        palette_widget->setReadOnly(true);

        connect(palette_widget, static_cast<void (ColorPaletteWidget::*)(int)>(&ColorPaletteWidget::currentColorChanged),
                this, [=] (int idx) {
            auto colors = m_palette->palette(0).colors();
            if(colors.size() <= idx)
                return;

            auto col = iscore::ColorRef::ColorFromString(colors.at(idx).second);
            if(col)
                emit colorChanged(*col);
        } );

        auto colorMenu = new QMenu;
        auto act = new QWidgetAction(colorMenu);
        act->setDefaultWidget(palette_widget);
        colorMenu->insertAction(nullptr, act);
        m_colorButton->setMenu(colorMenu);
    }

    con(metadata,   &iscore::ModelMetadata::metadataChanged,
        this,       &MetadataWidget::updateAsked);
    updateAsked();
}

QString MetadataWidget::scriptingName() const
{
    return m_scriptingNameLine->text();
}

void MetadataWidget::setScriptingName(QString arg)
{
    if(m_scriptingNameLine->text() == arg)
    {
        return;
    }

    m_scriptingNameLine->setText(arg);
    emit scriptingNameChanged(arg);
}

void MetadataWidget::updateAsked()
{
    m_scriptingNameLine->setText(m_metadata.getName());
    m_labelLine->setText(m_metadata.getLabel());
    m_comments->setText(m_metadata.getComment());
    m_meta->update(m_metadata.getExtendedMetadata());

    m_colorButtonPixmap.fill(m_metadata.getColor().getColor());
    m_colorButton->setIcon(QIcon(m_colorButtonPixmap));

    // m_currentColor = newColor;
}
}

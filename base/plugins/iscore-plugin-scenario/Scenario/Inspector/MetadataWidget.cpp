#include <QBoxLayout>
#include <QColorDialog>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QSize>
#include <Process//ModelMetadata.hpp>

#include "CommentEdit.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include "MetadataWidget.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class QObject;

namespace Scenario
{
MetadataWidget::MetadataWidget(
        const ModelMetadata* metadata,
        CommandDispatcher<>* m,
        const QObject *docObject,
        QWidget* parent) :
    QWidget(parent),
    m_metadata {metadata},
    m_commandDispatcher{m}
{
    // main
    auto metadataLayout = new QVBoxLayout{this};
    metadataLayout->setContentsMargins(0,0,0,0);
    metadataLayout->setSpacing(1);

    // btn
    auto btnLay = new QVBoxLayout{};
    btnLay->setContentsMargins(0,0,0,0);
    btnLay->setSpacing(1);

    // header
    auto headerLay = new QHBoxLayout{};
    headerLay->setContentsMargins(0,0,0,0);
    headerLay->setSpacing(1);

    // Name(s)
    QWidget* descriptionWidget = new QWidget {this};
    QFormLayout* descriptionLay = new QFormLayout;

    m_scriptingNameLine = new QLineEdit{metadata->name(), this};
    m_labelLine = new QLineEdit{metadata->label(), this};

    descriptionLay->addRow("Name", m_scriptingNameLine);
    descriptionLay->addRow("Label", m_labelLine);

    descriptionWidget->setObjectName("Description");

    descriptionWidget->setLayout(descriptionLay);

     // color
    m_colorButton = new QPushButton{};
    m_colorButton->setMaximumSize(QSize(1.5 * m_colorIconSize, 1.5 * m_colorIconSize));
    m_colorButton->setIconSize(QSize(m_colorIconSize, m_colorIconSize));
    m_colorButtonPixmap.fill(metadata->color());
    m_colorButton->setIcon(QIcon(m_colorButtonPixmap));


    // comments
    m_comments = new CommentEdit{metadata->comment(), this};
    m_comments->setVisible(false);

    m_cmtBtn = new QToolButton{};
    m_cmtBtn->setArrowType(Qt::RightArrow);

    btnLay->addWidget(m_colorButton);
    btnLay->addWidget(m_cmtBtn);

    headerLay->addWidget(descriptionWidget);
    headerLay->addLayout(btnLay);

    metadataLayout->addLayout(headerLay);
    metadataLayout->addWidget(m_comments);

    connect(m_cmtBtn, &QToolButton::released,
            this,  [&] ()
    {
        m_cmtExpanded = !m_cmtExpanded;
        m_comments->setVisible(m_cmtExpanded);
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

    connect(m_colorButton,  &QPushButton::clicked,
            this,           &MetadataWidget::changeColor);

    connect(metadata,   &ModelMetadata::metadataChanged,
            this,       &MetadataWidget::updateAsked);
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

void MetadataWidget::changeColor()
{
    QColor color = QColorDialog::getColor(m_metadata->color(), this, "Select Color");

    if(color.isValid())
    {
        emit colorChanged(color);
    }
}

void MetadataWidget::updateAsked()
{
    m_scriptingNameLine->setText(m_metadata->name());
    m_labelLine->setText(m_metadata->label());
    m_comments->setText(m_metadata->comment());

    m_colorButtonPixmap.fill(m_metadata->color());
    m_colorButton->setIcon(QIcon(m_colorButtonPixmap));
    // m_currentColor = newColor;
}
}

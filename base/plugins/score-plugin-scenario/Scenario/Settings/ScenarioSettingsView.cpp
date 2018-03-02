// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioSettingsView.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QJsonDocument>
#include <QLineEdit>
#include <QDialog>
#include <QtColorWidgets/ColorWheel>
#include <QFileDialog>
#include <QApplication>
#include <score/widgets/SignalUtils.hpp>
#include <QTextEdit>
#include <QSyntaxHighlighter>
#include <QJsonObject>
#include <score/serialization/JSONVisitor.hpp>
#include <score/model/Skin.hpp>
namespace Scenario
{
namespace Settings
{
// CssHighlighter:
// License: GPLv3.
// Author: The Qt Project
// Taken from https://github.com/qt/qttools/blob/5.9.1/src/designer/src/lib/shared/csshighlighter.cpp

class CssHighlighter : public QSyntaxHighlighter
{
public:
    explicit CssHighlighter(QTextDocument *document);

protected:
    void highlightBlock(const QString&) override;
    void highlight(const QString&, int, int, int/*State*/);

private:
    enum State { Selector, Property, Value, Pseudo, Pseudo1, Pseudo2, Quote,
                 MaybeComment, Comment, MaybeCommentEnd };
};


CssHighlighter::CssHighlighter(QTextDocument *document)
: QSyntaxHighlighter(document)
{
}

void CssHighlighter::highlightBlock(const QString& text)
{
    enum Token { ALNUM, LBRACE, RBRACE, COLON, SEMICOLON, COMMA, QUOTE, SLASH, STAR };
    static const int transitions[10][9] = {
        { Selector, Property, Selector, Pseudo,    Property, Selector, Quote, MaybeComment, Selector }, // Selector
        { Property, Property, Selector, Value,     Property, Property, Quote, MaybeComment, Property }, // Property
        { Value,    Property, Selector, Value,     Property, Value,    Quote, MaybeComment, Value }, // Value
        { Pseudo1, Property, Selector, Pseudo2,    Selector, Selector, Quote, MaybeComment, Pseudo }, // Pseudo
        { Pseudo1, Property, Selector, Pseudo,    Selector, Selector, Quote, MaybeComment, Pseudo1 }, // Pseudo1
        { Pseudo2, Property, Selector, Pseudo,    Selector, Selector, Quote, MaybeComment, Pseudo2 }, // Pseudo2
        { Quote,    Quote,    Quote,    Quote,     Quote,    Quote,   -1, Quote, Quote }, // Quote
        { -1, -1, -1, -1, -1, -1, -1, -1, Comment }, // MaybeComment
        { Comment, Comment, Comment, Comment, Comment, Comment, Comment, Comment, MaybeCommentEnd }, // Comment
        { Comment, Comment, Comment, Comment, Comment, Comment, Comment, -1, MaybeCommentEnd } // MaybeCommentEnd
    };

    int lastIndex = 0;
    bool lastWasSlash = false;
    int state = previousBlockState(), save_state;
    if (state == -1) {
        // As long as the text is empty, leave the state undetermined
        if (text.isEmpty()) {
            setCurrentBlockState(-1);
            return;
        }
        // The initial state is based on the precense of a : and the absense of a {.
        // This is because Qt style sheets support both a full stylesheet as well as
        // an inline form with just properties.
        state = save_state = (text.indexOf(QLatin1Char(':')) > -1 &&
                              text.indexOf(QLatin1Char('{')) == -1) ? Property : Selector;
    } else {
        save_state = state>>16;
        state &= 0x00ff;
    }

    if (state == MaybeCommentEnd) {
        state = Comment;
    } else if (state == MaybeComment) {
        state = save_state;
    }

    for (int i = 0; i < text.length(); i++) {
        int token = ALNUM;
        const QChar c = text.at(i);
        const char a = c.toLatin1();

        if (state == Quote) {
            if (a == '\\') {
                lastWasSlash = true;
            } else {
                if (a == '\"' && !lastWasSlash) {
                    token = QUOTE;
                }
                lastWasSlash = false;
            }
        } else {
            switch (a) {
            case '{': token = LBRACE; break;
            case '}': token = RBRACE; break;
            case ':': token = COLON; break;
            case ';': token = SEMICOLON; break;
            case ',': token = COMMA; break;
            case '\"': token = QUOTE; break;
            case '/': token = SLASH; break;
            case '*': token = STAR; break;
            default: break;
            }
        }

        int new_state = transitions[state][token];

        if (new_state != state) {
            bool include_token = new_state == MaybeCommentEnd || (state == MaybeCommentEnd && new_state!= Comment)
                                 || state == Quote;
            highlight(text, lastIndex, i-lastIndex+include_token, state);

            if (new_state == Comment) {
                lastIndex = i-1; // include the slash and star
            } else {
                lastIndex = i + ((token == ALNUM || new_state == Quote) ? 0 : 1);
            }
        }

        if (new_state == -1) {
            state = save_state;
        } else if (state <= Pseudo2) {
            save_state = state;
            state = new_state;
        } else {
            state = new_state;
        }
    }

    highlight(text, lastIndex, text.length() - lastIndex, state);
    setCurrentBlockState(state + (save_state<<16));
}

void CssHighlighter::highlight(const QString &text, int start, int length, int state)
{
    if (start >= text.length() || length <= 0)
        return;

    QTextCharFormat format;

    switch (state) {
    case Selector:
        setFormat(start, length, qRgb(250, 200, 200));
        break;
    case Property:
        setFormat(start, length, qRgb(200, 200, 250));
        break;
    case Value:
        setFormat(start, length, qRgb(200, 200, 200));
        break;
    case Pseudo1:
        setFormat(start, length, qRgb(250, 220, 180));
        break;
    case Pseudo2:
        setFormat(start, length, qRgb(220, 250, 180));
        break;
    case Quote:
        setFormat(start, length, qRgb(180, 220, 180));
        break;
    case Comment:
    case MaybeCommentEnd:
        format.setForeground(Qt::darkGreen);
        setFormat(start, length, format);
        break;
    default:
        break;
    }
}


class ThemeDialog: public QDialog
{
public:
    QHBoxLayout layout;
    QVBoxLayout sublay;
    QListWidget list;
    QLineEdit hexa;
    QLineEdit rgb;
    QPushButton save{tr("Save")};
    QTextEdit css;
    CssHighlighter highlight{css.document()};
    color_widgets::ColorWheel wheel;
    ThemeDialog(QWidget* p): QDialog{p}
    {
        layout.addWidget(&list);
        layout.addLayout(&sublay);
        sublay.addWidget(&wheel);
        sublay.addWidget(&hexa);
        sublay.addWidget(&rgb);
        sublay.addWidget(&save);
        layout.addWidget(&css);
        this->setLayout(&layout);
        score::Skin& s = score::Skin::instance();
        for(auto& col : s.getColors())
        {
            QPixmap p{16, 16};
            p.fill(col.first);
            list.addItem(new QListWidgetItem(p, col.second));
        }

        connect(&list, &QListWidget::currentItemChanged, this,
                [&] (QListWidgetItem* cur, auto prev) {
            if(cur)
                wheel.setColor(s.fromString(cur->text())->color());
        });

        connect(&wheel, &color_widgets::ColorWheel::colorChanged,
                this, [&] (QColor c) {
            if(list.currentItem())
            {
                s.fromString(list.currentItem()->text())->setColor(c);
                QPixmap p{16, 16};
                p.fill(c);
                list.currentItem()->setIcon(p);
                s.changed();
                hexa.setText(c.name(QColor::HexRgb));
                rgb.setText(QString("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue()));
            }
        });

        connect(&save, &QPushButton::clicked, this, [] {
            auto f = QFileDialog::getSaveFileName(nullptr, tr("Skin"), "", tr("*.json"));
            if(f.isEmpty())
                return;
            QFile fl{f};
            fl.open(QIODevice::WriteOnly);
            if(!fl.isOpen())
                return;

            QJsonObject obj;
            for(auto& col : score::Skin::instance().getColors())
            {
                obj.insert(col.second, toJsonValue(col.first));
            }

            QJsonDocument doc;
            doc.setObject(obj);
            fl.write(doc.toJson());
        });

        QFile css_f(":/qdarkstyle/qdarkstyle.qss");
        css_f.open(QFile::ReadOnly);
        css.document()->setPlainText(css_f.readAll());
        connect(css.document(), &QTextDocument::contentsChanged,
                this, [=] {
            qApp->setStyleSheet(css.document()->toPlainText());
        });
    }
};

View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;
  m_widg->setLayout(lay);

  // SKIN
  m_skin = new QComboBox;
  m_skin->addItems({"Default", "Dark", "IEEE"});
  lay->addRow(tr("Skin"), m_skin);
  auto es = new QPushButton{tr("Edit skin")};
  connect(es, &QPushButton::clicked, this, []
  {
      ThemeDialog d{nullptr};
      d.exec();
  });
  auto ls = new QPushButton{tr("Load skin")};
  connect(ls, &QPushButton::clicked, this, [=] {
      auto f = QFileDialog::getOpenFileName(nullptr, tr("Skin"), tr("*.json"));
      skinChanged(f);
  });
  lay->addWidget(ls);
  lay->addWidget(es);

  connect(m_skin, &QComboBox::currentTextChanged, this, &View::skinChanged);

  // ZOOM
  m_zoomSpinBox = new QSpinBox;
  m_zoomSpinBox->setMinimum(50);
  m_zoomSpinBox->setMaximum(300);

  connect(
      m_zoomSpinBox, SignalUtils::QSpinBox_valueChanged_int(), this,
      &View::zoomChanged);

  m_zoomSpinBox->setSuffix(tr("%"));

  lay->addRow(tr("Graphical Zoom \n (50% -- 300%)"), m_zoomSpinBox);

  // SLOT HEIGHT
  m_slotHeightBox = new QSpinBox;
  m_slotHeightBox->setMinimum(0);
  m_slotHeightBox->setMaximum(10000);

  connect(
      m_slotHeightBox,
      static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
      &View::slotHeightChanged);

  lay->addRow(tr("Default Slot Height"), m_slotHeightBox);

  // Default duration
  m_defaultDur = new score::TimeSpinBox;
  connect(
      m_defaultDur, &score::TimeSpinBox::timeChanged, this,
      [=](const QTime& t) { defaultDurationChanged(TimeVal{t}); });
  lay->addRow(tr("New score duration"), m_defaultDur);

  m_sequence = new QCheckBox{m_widg};
  connect(m_sequence, &QCheckBox::toggled, this, &View::sequenceChanged);
  lay->addRow(tr("Auto-Sequence"), m_sequence);

  SETTINGS_UI_TOGGLE_SETUP("Time Bar", TimeBar);
}

SETTINGS_UI_TOGGLE_IMPL(TimeBar)

void View::setSkin(const QString& val)
{
  if (val != m_skin->currentText())
  {
    int index = m_skin->findText(val);
    if (index != -1)
    {
      m_skin->setCurrentIndex(index);
    }
  }
}

void View::setZoom(const int val)
{
  if (val != m_zoomSpinBox->value())
    m_zoomSpinBox->setValue(val);
}

void View::setDefaultDuration(const TimeVal& t)
{
  auto qtime = t.toQTime();
  if (qtime != m_defaultDur->time())
    m_defaultDur->setTime(qtime);
}

void View::setSlotHeight(const double val)
{
  if (val != m_slotHeightBox->value())
    m_slotHeightBox->setValue(val);
}

void View::setSequence(const bool val)
{
  if (val != m_sequence->checkState())
    m_sequence->setChecked(val);
}

QWidget* View::getWidget()
{
  return m_widg;
}
}
}

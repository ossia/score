#include "AboutWidget.hpp"

#include <score/widgets/Pixmap.hpp>

#include <core/presenter/Licenses.hpp>

#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <score_git_info.hpp>

#include <algorithm>
#include <memory>
#include <vector>

namespace score
{
namespace
{
//! A partner logo on a light card; clicking it opens the partner's web site.
class LogoCard final : public QWidget
{
public:
  static constexpr int Width = 190;
  static constexpr int LogoHeight = 88;
  static constexpr int Height = LogoHeight + 26;

  LogoCard(
      const AboutWidget::Style& style, const QString& name, const QString& logo,
      const QString& url, const QString& tooltip, QWidget* parent)
      : QWidget{parent}
      , m_style{style}
      , m_name{name}
      , m_url{url}
      , m_logo{logo.isEmpty() ? QPixmap{} : QPixmap{logo}}
  {
    setFixedSize(Width, Height);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tooltip.isEmpty() ? url : tooltip);
  }

private:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setRenderHints(
        QPainter::Antialiasing | QPainter::TextAntialiasing
        | QPainter::SmoothPixmapTransform);

    const QRectF card{0.5, 0.5, Width - 1., LogoHeight - 1.};
    p.setPen(QPen{m_hovered ? m_style.hover : m_style.outline, 1});
    p.setBrush(QColor{"#f4f4f4"});
    p.drawRoundedRect(card, 5, 5);

    if(!m_logo.isNull())
    {
      const QRectF inner = card.adjusted(14, 12, -14, -12);
      QSizeF sz = m_logo.deviceIndependentSize();
      sz.scale(inner.size(), Qt::KeepAspectRatio);
      const QRectF target{
          inner.center() - QPointF{sz.width() / 2., sz.height() / 2.}, sz};
      p.drawPixmap(target, m_logo, m_logo.rect());
    }
    else
    {
      // No logo available: the name itself, set large on the card
      QFont f = m_style.itemFont;
      f.setPointSizeF(f.pointSizeF() * 1.6);
      f.setWeight(QFont::DemiBold);
      p.setFont(f);
      p.setPen(QColor{"#1a1a1a"});
      p.drawText(card, Qt::AlignCenter, m_name);
    }

    p.setPen(m_hovered ? m_style.hover : m_style.text);
    p.setFont(m_style.smallFont);
    const QFontMetrics fm{m_style.smallFont};
    p.drawText(
        QRect{0, LogoHeight, Width, Height - LogoHeight}, Qt::AlignCenter,
        fm.elidedText(m_name, Qt::ElideRight, Width - 8));
  }
  void enterEvent(QEnterEvent*) override
  {
    m_hovered = true;
    update();
  }
  void leaveEvent(QEvent*) override
  {
    m_hovered = false;
    update();
  }
  void mouseReleaseEvent(QMouseEvent* e) override
  {
    if(e->button() == Qt::LeftButton && rect().contains(e->pos()))
      QDesktopServices::openUrl(QUrl{m_url});
  }

  const AboutWidget::Style& m_style;
  QString m_name;
  QString m_url;
  QPixmap m_logo;
  bool m_hovered{};
};

//! Plain clickable text, highlighted when hovered or selected: tabs and links
class TabLabel final : public QWidget
{
public:
  std::function<void()> onPressed;

  TabLabel(
      const AboutWidget::Style& style, const QFont& font, const QString& text,
      QWidget* parent)
      : QWidget{parent}
      , m_style{style}
      , m_font{font}
      , m_text{text}
  {
    setCursor(Qt::PointingHandCursor);
    const QFontMetrics fm{m_font};
    setFixedSize(fm.horizontalAdvance(m_text), std::max(30, fm.height() + 6));
  }
  void setSelected(bool b)
  {
    m_selected = b;
    update();
  }

private:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setFont(m_font);
    p.setPen((m_selected || m_hovered) ? m_style.hover : m_style.text);
    p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, m_text);
  }
  void enterEvent(QEnterEvent*) override
  {
    m_hovered = true;
    update();
  }
  void leaveEvent(QEvent*) override
  {
    m_hovered = false;
    update();
  }
  void mouseReleaseEvent(QMouseEvent* e) override
  {
    if(e->button() == Qt::LeftButton && rect().contains(e->pos()) && onPressed)
      onPressed();
  }

  const AboutWidget::Style& m_style;
  QFont m_font;
  QString m_text;
  bool m_hovered{};
  bool m_selected{};
};

QLabel*
makeText(const QString& text, const QFont& font, const QColor& color, QWidget* parent)
{
  auto label = new QLabel{text, parent};
  label->setFont(font);
  label->setWordWrap(true);
  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, color);
  label->setPalette(pal);
  return label;
}
}

AboutWidget::Style AboutWidget::defaultStyle()
{
  Style s;
  s.sectionFont = QFont("Montserrat", 10, QFont::Medium);
  s.sectionFont.setCapitalization(QFont::AllUppercase);
  s.sectionFont.setLetterSpacing(QFont::PercentageSpacing, 108);
  s.itemFont = QFont("Ubuntu", 12, QFont::Normal);
  s.smallFont = QFont("Ubuntu", 10, QFont::Normal);
  return s;
}

AboutWidget::AboutWidget(const Style& st, QWidget* parent)
    : QWidget{parent}
{
  // The style outlives every child: they keep a reference to this copy
  auto style = std::make_shared<Style>(st);
  const Style& s = *style;

  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(6);

  // Version, commit, license
  {
    // "3.8.2+42 (branch @ commit)": the codename goes right after the number
    QString version = QCoreApplication::applicationVersion();
    const QString codename = QStringLiteral(" \u201c%1\u201d").arg(SCORE_CODENAME);
    if(const int sp = version.indexOf(' '); sp >= 0)
      version.insert(sp, codename);
    else
      version += codename;
    version = tr("Version %1").arg(version);
    if(const QString commit{GIT_COMMIT};
       !commit.isEmpty() && !version.contains(commit.left(8)))
      version += tr(", commit %1").arg(commit.left(12));
    lay->addWidget(makeText(version, s.itemFont, s.version, this));
    lay->addWidget(makeText(
        tr("Copyright © ossia 2014-%1. ossia score is free software, distributed "
           "under the GNU General Public License 3.0.")
            .arg(QDate::currentDate().year()),
        s.itemFont, s.muted, this));
  }

  // Tabs
  auto tabs = new QHBoxLayout;
  tabs->setSpacing(24);
  auto stack = new QStackedWidget{this};
  auto tabItems = std::make_shared<std::vector<TabLabel*>>();
  auto addTab = [&](const QString& name, QWidget* content) {
    const int index = stack->addWidget(content);
    auto item = new TabLabel{s, s.sectionFont, name, this};
    item->onPressed = [=] {
      stack->setCurrentIndex(index);
      for(auto tab : *tabItems)
        tab->setSelected(tab == item);
    };
    tabItems->push_back(item);
    tabs->addWidget(item);
  };

  // Support: the partners behind the project and the organizations funding it
  {
    auto support = new QWidget;
    support->setAutoFillBackground(false);
    auto slay = new QVBoxLayout{support};
    slay->setContentsMargins(0, 8, 8, 0);
    slay->setSpacing(10);

    struct Partner
    {
      QString name, logo, url, tooltip;
    };
    static constexpr int columns = 3;
    auto addGroup = [&](const QString& title, const std::vector<Partner>& partners) {
      slay->addWidget(makeText(title, s.itemFont, s.muted, support));
      auto grid = new QGridLayout;
      grid->setHorizontalSpacing(14);
      grid->setVerticalSpacing(10);
      int i = 0;
      for(const auto& partner : partners)
      {
        auto card = new LogoCard{s,           partner.name,    partner.logo,
                                 partner.url, partner.tooltip, support};
        grid->addWidget(card, i / columns, i % columns, Qt::AlignLeft | Qt::AlignTop);
        i++;
      }
      grid->setColumnStretch(columns, 1);
      slay->addLayout(grid);
    };

    addGroup(
        tr("ossia score has been developed and supported by these organizations:"),
        {{"ossia.io", ":/about/logos/ossia.png", "https://ossia.io", {}},
         {"SAT", ":/about/logos/sat.png", "https://sat.qc.ca",
          tr("Society for Arts and Technology, Montreal")},
         {"LaBRI", ":/about/logos/labri.png", "https://www.labri.fr", {}},
         {"SCRIME", ":/about/logos/scrime.png", "https://scrime.u-bordeaux.fr", {}},
         {"Blue Yeti", ":/about/logos/blueyeti.png", "http://www.blueyeti.fr", {}}});

    slay->addSpacing(8);
    addGroup(
        tr("Development has been funded over the years by grants and donations from:"),
        {{"Epic MegaGrants",
          ":/about/logos/unreal.png",
          "https://www.unrealengine.com/",
          {}},
         {"Mozilla", ":/about/logos/mozilla.png", "https://www.mozilla.org/", {}},
         {"Open Collective",
          ":/about/logos/opencollective.png",
          "https://opencollective.com/ossia",
          {}},
         {"Anthropic",
          {},
          "https://www.anthropic.com/",
          tr("Six months of Claude Max for the development of score")},
         {"ANR", ":/about/logos/anr.png", "https://anr.fr/",
          tr("French National Research Agency")},
         {"Celtera", ":/about/logos/celtera.png", "https://celtera.dev", {}},
         {"CIFRE / ANRT", ":/about/logos/cifre.png", "https://www.anrt.asso.fr", {}},
         {tr("Ministry of Higher Education"), ":/about/logos/mesr.png",
          "https://www.enseignementsup-recherche.gouv.fr/",
          tr("French Ministry of Higher Education and Research")},
         {tr("Ministry of Culture"), ":/about/logos/culture.png",
          "https://www.culture.gouv.fr/", tr("French Ministry of Culture")}});

    slay->addWidget(makeText(
        tr("With additional support and contributions from GMEA, L\u2019Arboretum, "
           "didascalie.net and StudioMirio."),
        s.itemFont, s.muted, support));

    // ... and by the people
    auto people = new QHBoxLayout;
    people->setSpacing(0);
    people->addWidget(
        makeText(tr("Thanks as well to all the "), s.itemFont, s.muted, support));
    auto addLink = [&](const QString& text, const QString& url) {
      auto link = new TabLabel{s, s.itemFont, text, support};
      link->setToolTip(url);
      link->onPressed = [url] { QDesktopServices::openUrl(QUrl{url}); };
      people->addWidget(link);
    };
    addLink(
        tr("contributors on GitHub"),
        "https://github.com/ossia/score/graphs/contributors");
    people->addWidget(makeText(tr(" and "), s.itemFont, s.muted, support));
    addLink(tr("donors on Open Collective"), "https://opencollective.com/ossia");
    people->addWidget(makeText(".", s.itemFont, s.muted, support));
    people->addStretch();
    slay->addLayout(people);
    slay->addStretch();

    auto scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(support);
    addTab(tr("Support"), scroll);
  }

  // Licenses of the bundled third-party software
  {
    auto licenses = new QWidget;
    auto llay = new QHBoxLayout{licenses};
    llay->setContentsMargins(0, 8, 0, 0);
    llay->setSpacing(10);

    auto list = new QListWidget{licenses};
    list->setFixedWidth(200);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto text = new QPlainTextEdit{licenses};
    text->setReadOnly(true);
    text->setFont(s.smallFont);
    text->setPlaceholderText(tr("Select a component to read its license"));

    const auto all = thirdPartyLicenses();
    for(const auto& lic : all)
      list->addItem(lic.name);
    connect(list, &QListWidget::currentTextChanged, this, [=](const QString& current) {
      for(const auto& lic : all)
        if(lic.name == current)
        {
          text->setPlainText(lic.url + "\n\n" + lic.header + "\n" + lic.license);
          break;
        }
    });

    llay->addWidget(list);
    llay->addWidget(text, 1);
    addTab(tr("Licenses"), licenses);
  }

  // Authors: the AUTHORS file of the repository
  {
    auto authors = new QWidget;
    auto alay = new QVBoxLayout{authors};
    alay->setContentsMargins(0, 8, 0, 0);
    auto text = new QPlainTextEdit{authors};
    text->setReadOnly(true);
    text->setFont(s.itemFont);
    if(QFile f{":/about/AUTHORS"}; f.open(QIODevice::ReadOnly))
      text->setPlainText(QString::fromUtf8(f.readAll()).trimmed());
    alay->addWidget(text);
    addTab(tr("Authors"), authors);
  }

  tabs->addStretch();

  // Qt's own about box, as its license asks
  {
    auto aboutQt = new TabLabel{s, s.sectionFont, tr("About Qt"), this};
    aboutQt->onPressed = [this] { QMessageBox::aboutQt(this, tr("About Qt")); };
    tabs->addWidget(aboutQt);
  }

  lay->addLayout(tabs);
  lay->addWidget(stack, 1);

  if(!tabItems->empty())
    tabItems->front()->setSelected(true);

  // Keep the style alive as long as the widget
  connect(this, &QObject::destroyed, [style] { });
}

AboutWidget::~AboutWidget() { }
}

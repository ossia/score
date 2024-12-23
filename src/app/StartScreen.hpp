#pragma once
#include <score/model/Skin.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/view/QRecentFilesMenu.h>

#include <QApplication>
#include <QDesktopServices>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTextLayout>
#include <QVBoxLayout>

#include <score_git_info.hpp>

#include <optional>
#include <verdigris>

namespace score
{
namespace
{
template <typename OnSuccess, typename OnError>
class HTTPGet final : public QNetworkAccessManager
{
public:
  explicit HTTPGet(QUrl url, OnSuccess on_success, OnError on_error) noexcept
      : m_callback{std::move(on_success)}
      , m_error{std::move(on_error)}
  {
    connect(this, &QNetworkAccessManager::finished, this, [this](QNetworkReply* reply) {
      if(reply->error())
      {
        qDebug() << reply->errorString();
        m_error();
      }
      else
      {
        m_callback(reply->readAll());
      }

      reply->deleteLater();
      this->deleteLater();
    });

    QNetworkRequest req{std::move(url)};
    req.setRawHeader("User-Agent", "curl/7.35.0");

    req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    req.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::UserVerifiedRedirectPolicy);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    auto reply = get(req);
    connect(reply, &QNetworkReply::redirected, reply, &QNetworkReply::redirectAllowed);
  }

private:
  OnSuccess m_callback;
  OnError m_error;
};

}
class InteractiveLabel : public QWidget
{
  W_OBJECT(InteractiveLabel)

public:
  InteractiveLabel(
      const QFont& font, const QString& text, const QString url, QWidget* parent = 0);

  void setOpenExternalLink(bool val) { m_openExternalLink = val; }
  void setPixmaps(const QPixmap& pixmap, const QPixmap& pixmapOn);

  void disableInteractivity();
  void setActiveColor(const QColor& c);
  void setInactiveColor(const QColor& c);
  void setTextOption(QTextOption opt) { m_textOption = opt; }

  void labelPressed(const QString& file) W_SIGNAL(labelPressed, file)

  QRectF textBoundingBox(double width) const noexcept;

protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

private:
  QFont m_font;
  QString m_title;

  QString m_url;
  bool m_openExternalLink{};

  bool m_drawPixmap{};
  QPixmap m_currentPixmap;
  QPixmap m_pixmap;
  QPixmap m_pixmapOn;

  bool m_interactive{};
  QColor m_currentColor;
  QColor m_activeColor;
  QColor m_inactiveColor;

  QTextOption m_textOption;
};

InteractiveLabel::InteractiveLabel(
    const QFont& font, const QString& title, const QString url, QWidget* parent)
    : QWidget{parent}
    , m_font(font)
    , m_title(title)
    , m_url(url)
    , m_openExternalLink(false)
    , m_drawPixmap(false)
    , m_interactive(true)
{
  auto& skin = score::Skin::instance();
  m_inactiveColor = QColor{"#d3d3d3"}; //"#f0f0f0"};
  m_activeColor = QColor{"#03C3DD"};
  m_currentColor = m_inactiveColor;
  setCursor(skin.CursorPointingHand);
  setFixedSize(200, 34);
}

void InteractiveLabel::setPixmaps(const QPixmap& pixmap, const QPixmap& pixmapOn)
{
  m_drawPixmap = true;
  m_pixmap = pixmap;
  m_pixmapOn = pixmapOn;
  m_currentPixmap = m_pixmap;
}

void InteractiveLabel::disableInteractivity()
{
  m_interactive = false;
  setCursor(Qt::ArrowCursor);
}

void InteractiveLabel::setActiveColor(const QColor& c)
{
  m_activeColor = c;
}

void InteractiveLabel::setInactiveColor(const QColor& c)
{
  m_inactiveColor = c;
  m_currentColor = m_inactiveColor;
}

QRectF InteractiveLabel::textBoundingBox(double width) const noexcept
{
  int leading = QFontMetrics{m_font}.leading();
  QTextLayout lay;
  lay.setText(m_title);
  lay.setFont(m_font);
  lay.setTextOption(m_textOption);
  lay.beginLayout();
  double height{};
  while(true)
  {
    QTextLine line = lay.createLine();
    if(!line.isValid())
      break;

    line.setLineWidth(width);
    height += leading;
    line.setPosition(QPointF(0, height));
    height += line.height();
  }
  lay.endLayout();
  return lay.boundingRect();
}

void InteractiveLabel::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setPen(QPen{m_currentColor});

  painter.save();
  QRectF textRect = rect();
  if(m_drawPixmap)
  {
    int size = m_currentPixmap.width() / qApp->devicePixelRatio();
    int pixmapX = 0;

    if(m_textOption.alignment() == Qt::AlignRight)
    {
      QTextLayout lay;
      lay.setText(m_title);
      lay.setFont(m_font);
      lay.setTextOption(m_textOption);
      lay.beginLayout();
      lay.createLine();
      lay.endLayout();
      pixmapX = lay.boundingRect().width();
    }
    painter.drawPixmap(
        width() - pixmapX - m_currentPixmap.width() - 6,
        (textRect.height() - size - 10) / 2, m_currentPixmap);
    textRect.setX(textRect.x() + size + 6);
  }
  painter.setFont(m_font);
  painter.drawText(textRect, m_title, m_textOption);
  painter.restore();
}

void InteractiveLabel::enterEvent(QEnterEvent* event)
{
  if(!m_interactive)
    return;

  m_currentColor = m_activeColor;
  m_currentPixmap = m_pixmapOn;

  repaint();
}

void InteractiveLabel::leaveEvent(QEvent* event)
{
  if(!m_interactive)
    return;

  m_currentColor = m_inactiveColor;
  m_currentPixmap = m_pixmap;

  repaint();
}

void InteractiveLabel::mousePressEvent(QMouseEvent* event)
{
  if(m_openExternalLink)
  {
    QDesktopServices::openUrl(QUrl(m_url));
  }
  else
  {
    labelPressed(m_url);
  }
}
class StartScreen : public QWidget
{
  W_OBJECT(StartScreen)
public:
  StartScreen(const QPointer<QRecentFilesMenu>& recentFiles, QWidget* parent = 0);

  void openNewDocument() W_SIGNAL(openNewDocument)
  void openFile(const QString& file) W_SIGNAL(openFile, file)
  void openFileDialog() W_SIGNAL(openFileDialog)
  void loadCrashedSession() W_SIGNAL(loadCrashedSession)
  void exitApp() W_SIGNAL(exitApp)

  void addLoadCrashedSession();
  void setupTabs();

protected:
  void paintEvent(QPaintEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

private:
  QPixmap m_background;
  InteractiveLabel* m_crashLabel{};
};
struct StartScreenLink
{
  QString name;
  QString url;
  QString pixmap;
  QString pixmapOn;
  StartScreenLink(
      const QString& n, const QString& u, const QString& p, const QString& pOn)
      : name(n)
      , url(u)
      , pixmap(p)
      , pixmapOn(pOn)
  {
  }
};

StartScreen::StartScreen(const QPointer<QRecentFilesMenu>& recentFiles, QWidget* parent)
    : QWidget(parent)
{
#if defined(__APPLE__)
  static constexpr double font_factor = 96. / 72.;
#else
  static constexpr double font_factor = 1.;
#endif

  QFont navFont("Montserrat", 12 * font_factor, QFont::DemiBold);
  QFont headerFont("Montserrat", 12 * font_factor);
  QFont labelFont("Montserrat", 11 * font_factor, QFont::Normal);
  labelFont.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  labelFont.setStyleStrategy(QFont::PreferAntialias);

  // Header Widget
  QWidget* headerWidget = new QWidget(this);
  QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);

  QLabel* logoLabel = new QLabel(headerWidget);

  QPixmap placeholderPixmap(300, 100); // TODO: Replace with actual logo when available
  placeholderPixmap.fill(Qt::gray);
  logoLabel->setPixmap(placeholderPixmap);
  logoLabel->setFixedSize(placeholderPixmap.size());
  headerLayout->addWidget(logoLabel, 0, Qt::AlignLeft);

  //Donate label
  InteractiveLabel* donateLabel = new InteractiveLabel(
      navFont, tr("Donate"), "https://opencollective.com/ossia", this);
  donateLabel->setOpenExternalLink(true);
  donateLabel->setPixmaps(
      score::get_pixmap(":/icons/new_file_off.png"),
      score::get_pixmap(":/icons/new_file_on.png")); // TODO: Replace with  heart icon
  headerLayout->addWidget(donateLabel);

  // side nav list and stacked widget
  QListWidget* navigationList = new QListWidget(this);
  QStackedWidget* stackedWidget = new QStackedWidget(this);

  navigationList->setFixedWidth(155);
  navigationList->setSpacing(8);
  navigationList->setFont(navFont);

  navigationList->addItem(new QListWidgetItem(
      QIcon(score::get_pixmap(":/icons/new_file_off.png")), tr("Home")));
  navigationList->addItem(new QListWidgetItem(
      QIcon(score::get_pixmap(":/icons/load_examples_off.png")), tr("Learn")));
  navigationList->addItem(new QListWidgetItem(
      QIcon(score::get_pixmap(":/icons/forum_off.png")), tr("Community")));

  navigationList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  navigationList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  navigationList->setStyleSheet(
      "QListWidget { background-color: black;color: white; border: none;}"
      "QListWidget::item { padding: 10px 10; margin: 8px 0; color: white;}"
      "QListWidget::item:selected { background-color: #211f1f; color: white;"
      "padding:00px; margin: 00px 0; }");

  // Home View
  QWidget* homeView = new QWidget;
  QVBoxLayout* homeLayout = new QVBoxLayout(homeView);
  homeView->setStyleSheet("background-color: #211f1f;");

  // Left Col
  QLabel* createNewLabel = new QLabel(tr("Create score"));
  createNewLabel->setFont(headerFont);

  QVBoxLayout* leftColumnLayout = new QVBoxLayout;
  leftColumnLayout->addWidget(createNewLabel);

  leftColumnLayout->addSpacing(10);

  InteractiveLabel* newLabel
      = new InteractiveLabel{labelFont, qApp->tr("Empty score"), "", this};
  newLabel->setPixmaps(
      score::get_pixmap(":/icons/new_file_off.png"),
      score::get_pixmap(":/icons/new_file_on.png"));
  newLabel->setTextOption(QTextOption(Qt::AlignLeft));
  connect(
      newLabel, &score::InteractiveLabel::labelPressed, this,
      &StartScreen::openNewDocument);
  leftColumnLayout->addWidget(newLabel);

  for(const auto& action : recentFiles->actions())
  {
    auto* fileLabel = new InteractiveLabel{
        labelFont, action->text(), action->data().toString(), this};
    fileLabel->setTextOption(QTextOption(Qt::AlignLeft));
    fileLabel->setPixmaps(
        score::get_pixmap(":/icons/new_file_off.png"),
        score::get_pixmap(":/icons/new_file_off.png"));
    fileLabel->setInactiveColor(QColor{"#f0f0f0"});
    fileLabel->setActiveColor(QColor{"#03C3DD"});
    connect(
        fileLabel, &score::InteractiveLabel::labelPressed, this, &StartScreen::openFile);
    leftColumnLayout->addWidget(fileLabel);
  }
  leftColumnLayout->addStretch();

  // Right Column - Load Score
  QLabel* loadScoreLabel = new QLabel(tr("Load a score"));
  loadScoreLabel->setFont(headerFont);

  QVBoxLayout* rightColumnLayout = new QVBoxLayout;
  rightColumnLayout->addWidget(loadScoreLabel);
  rightColumnLayout->addSpacing(10);

  for(const auto& action : recentFiles->actions())
  {
    auto* fileLabel = new InteractiveLabel{
        labelFont, action->text(), action->data().toString(), this};
    fileLabel->setTextOption(QTextOption(Qt::AlignLeft));
    fileLabel->setInactiveColor(QColor{"#f0f0f0"});
    fileLabel->setActiveColor(QColor{"#03C3DD"});
    connect(
        fileLabel, &score::InteractiveLabel::labelPressed, this, &StartScreen::openFile);

    rightColumnLayout->addWidget(fileLabel);
  }
  rightColumnLayout->addStretch();

  InteractiveLabel* openScoreLabel
      = new InteractiveLabel(labelFont, tr("Open.."), "", this);
  openScoreLabel->setPixmaps(
      score::get_pixmap(":/icons/load_off.png"),
      score::get_pixmap(":/icons/load_on.png"));
  openScoreLabel->setTextOption(QTextOption(Qt::AlignLeft));
  connect(
      openScoreLabel, &score::InteractiveLabel::labelPressed, this,
      &StartScreen::openFileDialog);

  rightColumnLayout->addWidget(openScoreLabel);

  m_crashLabel
      = new InteractiveLabel{labelFont, qApp->tr("Restore last session"), "", this};
  m_crashLabel->setTextOption(QTextOption(Qt::AlignLeft));
  m_crashLabel->setPixmaps(
      score::get_pixmap(":/icons/reload_crash_off.png"),
      score::get_pixmap(":/icons/reload_crash_on.png"));
  m_crashLabel->setFixedWidth(250);
  m_crashLabel->setInactiveColor(QColor{"#f0f0f0"});
  m_crashLabel->setActiveColor(QColor{"#f6a019"});
  m_crashLabel->setDisabled(true);
  m_crashLabel->hide();
  connect(
      m_crashLabel, &score::InteractiveLabel::labelPressed, this,
      &score::StartScreen::loadCrashedSession);

  rightColumnLayout->addWidget(m_crashLabel);

  QHBoxLayout* mainRowLayout = new QHBoxLayout;
  mainRowLayout->addLayout(leftColumnLayout);
  mainRowLayout->addLayout(rightColumnLayout);

  homeLayout->addLayout(mainRowLayout);
  homeView->setLayout(homeLayout);
  stackedWidget->addWidget(homeView);

  // Learn View
  QWidget* learnView = new QWidget;
  QVBoxLayout* learnLayout = new QVBoxLayout(learnView);
  learnView->setStyleSheet("background-color: #211f1f;");

  QLabel* learnLabel = new QLabel(tr("Browse examples"));
  learnLabel->setFont(headerFont);
  learnLayout->addWidget(learnLabel);
  learnLayout->addSpacing(10);

  // Add "Tutorials" link to the Learn section
  InteractiveLabel* tutorialsLabel = new InteractiveLabel(
      navFont, tr("Tutorials"), "https://www.youtube.com/...", learnView);
  tutorialsLabel->setOpenExternalLink(true);
  tutorialsLabel->setPixmaps(
      score::get_pixmap(":/icons/tutorials_off.png"),
      score::get_pixmap(":/icons/tutorials_on.png"));
  tutorialsLabel->setStyleSheet(
      "QLabel { padding: 8px; background-color: #161514; color: white; }"
      "QLabel:hover { color: #f6a019; }");
  learnLayout->addWidget(tutorialsLabel);

  learnLayout->addStretch();
  learnView->setLayout(learnLayout);
  stackedWidget->addWidget(learnView);

  // Community View
  QWidget* communityView = new QWidget;
  QVBoxLayout* communityLayout = new QVBoxLayout(communityView);
  communityView->setStyleSheet("background-color: #211f1f;");

  QLabel* communityLabel = new QLabel(tr("Get involved"));
  communityLabel->setFont(headerFont);

  communityLayout->addWidget(communityLabel);
  communityLayout->addSpacing(10);

  std::array<score::StartScreenLink, 3> communityMenus
      = {{{tr("ossia Forum"), "http://forum.ossia.io/", ":/icons/forum_off.png",
           ":/icons/forum_on.png"},
          {tr("Contribute"), "https://opencollective.com/ossia/contribute",
           ":/icons/contribute_off.png", ":/icons/contribute_on.png"},
          {tr("Discord chat"), "https://discord.gg/ossia",
           ":/icons/chat_off.png", // TODO : replace by Discord icon
           ":/icons/chat_on.png"}}};

  for(const auto& m : communityMenus)
  {
    InteractiveLabel* menuLabel
        = new InteractiveLabel(navFont, m.name, m.url, communityView);
    menuLabel->setOpenExternalLink(true);
    menuLabel->setPixmaps(score::get_pixmap(m.pixmap), score::get_pixmap(m.pixmapOn));

    menuLabel->setStyleSheet(
        "QLabel {padding: 8px; background-color: #161514; color: white;}"
        "QLabel:hover {color: #f6a019;}");

    communityLayout->addWidget(menuLabel);
  }

  communityLayout->addStretch();
  communityView->setLayout(communityLayout);
  stackedWidget->addWidget(communityView);

  // Connect navigation list to stacked widget
  connect(
      navigationList, &QListWidget::currentRowChanged, stackedWidget,
      &QStackedWidget::setCurrentIndex);
  navigationList->setCurrentRow(0);

  QHBoxLayout* contentLayout = new QHBoxLayout;
  contentLayout->addWidget(navigationList);
  contentLayout->addWidget(stackedWidget);

  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->addWidget(headerWidget);
  mainLayout->addLayout(contentLayout);
  setLayout(mainLayout);

  mainLayout->addStretch();

  this->setEnabled(true);
  setWindowModality(Qt::ApplicationModal);

  setFixedSize(600, 400);
  {
    // TODO : make this work
    // new version
    auto m_getLastVersion = new HTTPGet{
        QUrl("https://ossia.io/score-last-version.txt"),
        [this, navFont](const QByteArray& data) {
      auto version = QString::fromUtf8(data.simplified());
      if(SCORE_TAG_NO_V < version)
      {
        QString text
            = qApp->tr("New version %1 is available, click to update !").arg(version);
        QString url = "https://github.com/ossia/score/releases/latest/";
        InteractiveLabel* label = new InteractiveLabel{navFont, text, url, this};
        label->setPixmaps(
            score::get_pixmap(":/icons/version_on.png"),
            score::get_pixmap(":/icons/version_off.png"));
        label->setOpenExternalLink(true);
        label->setInactiveColor(QColor{"#f6a019"});
        label->setActiveColor(QColor{"#f0f0f0"});
        label->setFixedWidth(600);
        label->move(280, 170);
        label->show();
      }
    },
        [] {}};
  }
}

void StartScreen::addLoadCrashedSession()
{
  m_crashLabel->show();
  m_crashLabel->setEnabled(true);
  update();
}

void StartScreen::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);
  painter.fillRect(rect(), Qt::black);
}

void StartScreen::keyPressEvent(QKeyEvent* event)
{
  if(event->key() == Qt::Key_Escape)
  {
    this->openNewDocument();
    this->close();
    return QWidget::keyPressEvent(event);
  }
}
}

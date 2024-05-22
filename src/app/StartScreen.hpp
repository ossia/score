#pragma once
#include <score/model/Skin.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/view/QRecentFilesMenu.h>

#include <QApplication>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QSettings>

#include <score_git_info.hpp>

#include <optional>
#include <verdigris>

namespace score
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

  void labelPressed(const QString& file) W_SIGNAL(labelPressed, file)

protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

private:
  QFont m_font;
  QString m_title;

  QString m_url;
  bool m_openExternalLink;

  bool m_drawPixmap;
  QPixmap m_currentPixmap;
  QPixmap m_pixmap;
  QPixmap m_pixmapOn;

  bool m_interactive;
  QColor m_currentColor;
  QColor m_activeColor;
  QColor m_inactiveColor;
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
    painter.drawPixmap(0, (textRect.height() - size - 10) / 2, m_currentPixmap);
    textRect.setX(textRect.x() + size + 6);
  }
  painter.setFont(m_font);
  //painter.rotate(20);
  painter.drawText(textRect, m_title);
  painter.restore();
}

void InteractiveLabel::enterEvent(QEnterEvent* event)
{
  if(!m_interactive)
    return;

  m_currentColor = m_activeColor;
  m_font.setUnderline(true);
  m_currentPixmap = m_pixmapOn;

  repaint();
}

void InteractiveLabel::leaveEvent(QEvent* event)
{
  if(!m_interactive)
    return;

  m_currentColor = m_inactiveColor;
  m_font.setUnderline(false);
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
// Workaround until https://bugreports.qt.io/browse/QTBUG-103225 is fixed
#if defined(__APPLE__)
  static constexpr double font_factor = 96. / 72.;
#else
  static constexpr double font_factor = 1.;
#endif
  QFont f("Ubuntu", 14  * font_factor, QFont::Light);
  f.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  f.setStyleStrategy(QFont::PreferAntialias);

  QFont titleFont("Montserrat", 14  * font_factor, QFont::DemiBold);
  titleFont.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  titleFont.setStyleStrategy(QFont::PreferAntialias);

  this->setEnabled(true);
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint); //| Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::ApplicationModal);

  m_background = score::get_pixmap(":/startscreen/startscreensplash.png");

  if(QPainter painter; painter.begin(&m_background))
  {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setPen(QPen(QColor("#0092CF")));

    painter.setFont(f);
    //painter.drawText(QPointF(250, 170), QCoreApplication::applicationVersion());
    painter.drawText(QPointF(217, 188), QCoreApplication::applicationVersion());
    painter.end();
  }

  // Weird code here is because the window size seems to scale only to integer ratios.
  setFixedSize(m_background.size() / std::floor(qApp->devicePixelRatio()));

  {
    // new version
    auto m_getLastVersion = new HTTPGet{
        QUrl("https://ossia.io/score-last-version.txt"),
        [=](const QByteArray& data) {
      auto version = QString::fromUtf8(data.simplified());
      if(SCORE_TAG_NO_V < version)
      {
        QString text
            = qApp->tr("New version %1 is available, click to update !").arg(version);
        QString url = "https://github.com/ossia/score/releases/latest/";
        InteractiveLabel* label = new InteractiveLabel{titleFont, text, url, this};
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

  float label_x = 160;
  float label_y = 215;

  { // recent files
    InteractiveLabel* label
        = new InteractiveLabel{titleFont, qApp->tr("Recent files"), "", this};
    label->setPixmaps(
        score::get_pixmap(":/icons/recent_files.png"),
        score::get_pixmap(":/icons/recent_files.png"));
    label->setInactiveColor(QColor{"#f0f0f0"});
    label->setActiveColor(QColor{"#03C3DD"});
    label->disableInteractivity();
    label->move(label_x, label_y);
    label_y += 35;
  }
  f.setPointSize(12);

  label_x += 40;
  for(const auto& action : recentFiles->actions())
  {
    InteractiveLabel* fileLabel
        = new InteractiveLabel{f, action->text(), action->data().toString(), this};
    connect(
        fileLabel, &score::InteractiveLabel::labelPressed, this,
        &score::StartScreen::openFile);

    fileLabel->move(label_x, label_y);

    label_y += 25;
  }
  label_x = 160;
  label_y += 10;

  m_crashLabel
      = new InteractiveLabel{titleFont, qApp->tr("Restore last session"), "", this};
  m_crashLabel->setPixmaps(
      score::get_pixmap(":/icons/reload_crash_off.png"),
      score::get_pixmap(":/icons/reload_crash_on.png"));
  m_crashLabel->move(label_x, label_y);
  m_crashLabel->setFixedWidth(600);
  m_crashLabel->setInactiveColor(QColor{"#f0f0f0"});
  m_crashLabel->setActiveColor(QColor{"#f6a019"});
  m_crashLabel->setDisabled(true);
  m_crashLabel->hide();
  connect(
      m_crashLabel, &score::InteractiveLabel::labelPressed, this,
      &score::StartScreen::loadCrashedSession);

  label_x = 590;
  label_y = 215;
  { // Create new
    InteractiveLabel* label = new InteractiveLabel{titleFont, qApp->tr("New"), "", this};
    label->setPixmaps(
        score::get_pixmap(":/icons/new_file_off.png"),
        score::get_pixmap(":/icons/new_file_on.png"));
    connect(
        label, &score::InteractiveLabel::labelPressed, this,
        &score::StartScreen::openNewDocument);
    label->move(label_x, label_y);
    label_y += 35;
  }
  { // Load file
    InteractiveLabel* label
        = new InteractiveLabel{titleFont, qApp->tr("Load"), "", this};
    label->setPixmaps(
        score::get_pixmap(":/icons/load_off.png"),
        score::get_pixmap(":/icons/load_on.png"));
    connect(
        label, &score::InteractiveLabel::labelPressed, this,
        &score::StartScreen::openFileDialog);
    label->move(label_x, label_y);
    label_y += 35;
  }
  { // Load Examples
    QSettings settings;
    auto library_path = settings.value("Library/RootPath").toString();
    InteractiveLabel* label = new InteractiveLabel{
        titleFont, qApp->tr("Examples"), "https://github.com/ossia/score-examples", this};
    label->setPixmaps(
        score::get_pixmap(":/icons/load_examples_off.png"),
        score::get_pixmap(":/icons/load_examples_on.png"));
    label->setOpenExternalLink(true);
    label->move(label_x, label_y);
    label_y += 35;
  }

  label_y += 20;

  std::array<score::StartScreenLink, 4> menus
      = {{{qApp->tr("Tutorials"),
           "https://www.youtube.com/"
           "watch?v=R-3d8K6gQkw&list=PLIHLSiZpIa6YoY1_aW1yetDgZ7tZcxfEC&index=1",
           ":/icons/tutorials_off.png", ":/icons/tutorials_on.png"},
          {qApp->tr("Contribute"), "https://opencollective.com/ossia/contribute",
           ":/icons/contribute_off.png", ":/icons/contribute_on.png"},
          {qApp->tr("Forum"), "http://forum.ossia.io/", ":/icons/forum_off.png",
           ":/icons/forum_on.png"},
          {qApp->tr("Chat"), "https://gitter.im/ossia/score", ":/icons/chat_off.png",
           ":/icons/chat_on.png"}}};
  for(const auto& m : menus)
  {
    InteractiveLabel* menu_url = new InteractiveLabel{titleFont, m.name, m.url, this};
    menu_url->setOpenExternalLink(true);
    menu_url->setPixmaps(score::get_pixmap(m.pixmap), score::get_pixmap(m.pixmapOn));
    menu_url->move(label_x, label_y);
    label_y += 40;
  }

  label_y += 8;

  { // Exit App
    InteractiveLabel* label
        = new InteractiveLabel{titleFont, qApp->tr("Exit"), "", this};
    label->setPixmaps(
        score::get_pixmap(":/icons/exit_off.png"),
        score::get_pixmap(":/icons/exit_on.png"));
    connect(
        label, &score::InteractiveLabel::labelPressed, this,
        &score::StartScreen::exitApp);
    label->move(label_x, label_y);
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
  painter.drawPixmap(0, 0, m_background);
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

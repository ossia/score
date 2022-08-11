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

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::UserVerifiedRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    req.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
    req.setAttribute(QNetworkRequest::SpdyAllowedAttribute, true);
#else
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#endif

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

  void labelPressed(const QString& file) W_SIGNAL(labelPressed, file)

protected:
  void paintEvent(QPaintEvent* event) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEvent* event) override;
#else
  void enterEvent(QEnterEvent* event) override;
#endif
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
  m_currentColor = QColor{"#f0f0f0"};
  m_activeColor = QColor{"#03C3DD"};
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

void InteractiveLabel::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setPen(QPen{m_currentColor});

  QRectF textRect = rect();
  if(m_drawPixmap)
  {
    int size = m_currentPixmap.width();
    painter.drawPixmap(0, 0, m_currentPixmap);
    textRect.setX(textRect.x() + size / qApp->devicePixelRatio() + 6);
  }
  painter.setFont(m_font);
  painter.drawText(textRect, m_title);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void InteractiveLabel::enterEvent(QEvent* event)
#else
void InteractiveLabel::enterEvent(QEnterEvent* event)
#endif
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

  m_currentColor = QColor{"#f0f0f0"};
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
  QFont f("Ubuntu", 14, QFont::Light);
  f.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  f.setStyleStrategy(QFont::PreferAntialias);

  QFont titleFont("Montserrat", 14, QFont::DemiBold);
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
    painter.drawText(QPointF(381, 195), QCoreApplication::applicationVersion());
    painter.end();
  }

  // Weird code here is because the window size seems to scale only to integer ratios.
  setFixedSize(m_background.size() / std::floor(qApp->devicePixelRatio()));

  float label_y = 285;
  { // Create new
    InteractiveLabel* label = new InteractiveLabel{titleFont, qApp->tr("New"), "", this};
    label->setPixmaps(
        score::get_pixmap(":/icons/new_file_off.png"),
        score::get_pixmap(":/icons/new_file_on.png"));
    connect(
        label, &score::InteractiveLabel::labelPressed, this,
        &score::StartScreen::openNewDocument);
    label->move(100, label_y);
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
    label->move(100, label_y);
    label_y += 35;
  }
  { // Load Examples
    QSettings settings;
    auto library_path = settings.value("Library/Path").toString();
    InteractiveLabel* label = new InteractiveLabel{
        titleFont, qApp->tr("Examples"), "file://" + library_path, this};
    label->setPixmaps(
        score::get_pixmap(":/icons/load_examples_off.png"),
        score::get_pixmap(":/icons/load_examples_on.png"));
    label->setOpenExternalLink(true);
    label->move(100, label_y);
    label_y += 35;
  }
  { // Exit App
    InteractiveLabel* label
        = new InteractiveLabel{titleFont, qApp->tr("Exit"), "", this};
    label->setPixmaps(
        score::get_pixmap(":/icons/exit_off.png"),
        score::get_pixmap(":/icons/exit_on.png"));
    connect(
        label, &score::InteractiveLabel::labelPressed, this,
        &score::StartScreen::exitApp);
    label->move(100, label_y);
    label_y += 50;
  }
  label_y = 285;
  { // recent files
    InteractiveLabel* label
        = new InteractiveLabel{titleFont, qApp->tr("Recent files"), "", this};
    label->setPixmaps(
        score::get_pixmap(":/icons/recent_files.png"),
        score::get_pixmap(":/icons/recent_files.png"));
    label->disableInteractivity();
    label->move(280, label_y);
    label_y += 30;
  }
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
            score::get_pixmap(":/icons/version_off.png"),
            score::get_pixmap(":/icons/version_on.png"));
        label->setOpenExternalLink(true);
        label->setFixedWidth(600);
        label->move(140, 245);
        label->show();
      }
        },
        [] {}};
  }
  f.setPointSize(12);
  for(const auto& action : recentFiles->actions())
  {
    InteractiveLabel* fileLabel
        = new InteractiveLabel{f, action->text(), action->data().toString(), this};
    connect(
        fileLabel, &score::InteractiveLabel::labelPressed, this,
        &score::StartScreen::openFile);

    fileLabel->move(310, label_y);

    label_y += 25;
  }

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
  label_y = 285;
  for(const auto& m : menus)
  {
    InteractiveLabel* menu_url = new InteractiveLabel{titleFont, m.name, m.url, this};
    menu_url->setOpenExternalLink(true);
    menu_url->setPixmaps(score::get_pixmap(m.pixmap), score::get_pixmap(m.pixmapOn));
    menu_url->move(530, label_y);
    label_y += 40;
  }

  m_crashLabel = new InteractiveLabel{
      titleFont, qApp->tr("Reload your previously crashed work ?"), "", this};
  m_crashLabel->setPixmaps(
      score::get_pixmap(":/icons/reload_crash_off.png"),
      score::get_pixmap(":/icons/reload_crash_on.png"));
  m_crashLabel->move(150, 460);
  m_crashLabel->setFixedWidth(600);
  m_crashLabel->setActiveColor(QColor{"#f6a019"});
  m_crashLabel->setDisabled(true);
  m_crashLabel->hide();
  connect(
      m_crashLabel, &score::InteractiveLabel::labelPressed, this,
      &score::StartScreen::loadCrashedSession);
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

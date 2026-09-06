#pragma once
#include <score/model/Skin.hpp>
#include <score/tools/ThreadPool.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/document/DocumentTemplates.hpp>
#include <core/document/ProjectInfo.hpp>
#include <core/presenter/AboutWidget.hpp>
#include <core/view/QRecentFilesMenu.h>

#include <QApplication>
#include <QCloseEvent>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVersionNumber>

#include <score_git_info.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>
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

// Brand colors of the start screen. They match the splash artwork and the
// start screen icon set (see src/lib/resources/icons_svg/readme.md), and are
// deliberately independent from the editor skin.
namespace StartScreenColors
{
static const QColor Panel{
    0x21, 0x1f, 0x1f, 205}; // translucent: the artwork shows through
static const QColor Card{"#2b2929"};
static const QColor CardDark{"#161514"};
static const QColor Outline{"#3d3a3a"};
static const QColor Text{"#f0f0f0"};
static const QColor Muted{"#8a8a8a"};
static const QColor Hover{"#03C3DD"};
static const QColor Accent{"#f6a019"};
static const QColor Version{"#0092CF"};
}

// Crops an image to the given aspect and scales it to exactly `size`.
QPixmap coverPixmap(const QImage& img, QSize size)
{
  if(img.isNull())
    return {};
  QImage scaled
      = img.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  const QRect crop{
      (scaled.width() - size.width()) / 2, (scaled.height() - size.height()) / 2,
      size.width(), size.height()};
  return QPixmap::fromImage(scaled.copy(crop));
}
}

/**
 * @brief A clickable label with an optional icon and a hover state.
 *
 * Used for every actionable entry of the start screen: either it opens an
 * external URL, or it emits labelPressed(url) for the StartScreen to act upon.
 * When made checkable it also serves as a navigation tab.
 */
class InteractiveLabel : public QWidget
{
  W_OBJECT(InteractiveLabel)

public:
  InteractiveLabel(
      const QFont& font, const QString& text, const QString& url,
      QWidget* parent = nullptr);

  void setOpenExternalLink(bool val) { m_openExternalLink = val; }
  void setPixmaps(const QPixmap& pixmap, const QPixmap& pixmapOn);

  void disableInteractivity();
  void setActiveColor(const QColor& c);
  void setInactiveColor(const QColor& c);
  void setElideMode(Qt::TextElideMode mode) { m_elideMode = mode; }
  void setLeftPadding(int px) { m_leftPadding = px; }
  //! Width reserved for the icon, so that texts align whatever the icon size.
  void setIconColumn(int px) { m_iconColumn = px; }
  void setItemHeight(int px);

  void setCheckable(bool b) { m_checkable = b; }
  void setCheckedBackground(const QColor& c) { m_checkedBackground = c; }
  void setChecked(bool b);
  bool isChecked() const noexcept { return m_checked; }

  void setText(const QString& text);
  const QString& text() const noexcept { return m_title; }
  const QString& url() const noexcept { return m_url; }

  void labelPressed(const QString& file) W_SIGNAL(labelPressed, file)
  void hovered(bool state) W_SIGNAL(hovered, state)

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  bool highlighted() const noexcept { return m_checked || (m_interactive && m_hovered); }
  int iconWidth() const noexcept { return m_pixmap.isNull() ? 0 : m_iconColumn; }

  QFont m_font;
  QString m_title;
  QString m_url;

  QPixmap m_pixmap;
  QPixmap m_pixmapOn;

  QColor m_activeColor{StartScreenColors::Hover};
  QColor m_inactiveColor{StartScreenColors::Text};
  QColor m_checkedBackground;

  Qt::TextElideMode m_elideMode{Qt::ElideRight};
  int m_leftPadding{4};
  int m_iconColumn{34};
  int m_height{30};

  bool m_openExternalLink{};
  bool m_interactive{true};
  bool m_hovered{};
  bool m_pressed{};
  bool m_checkable{};
  bool m_checked{};
};

InteractiveLabel::InteractiveLabel(
    const QFont& font, const QString& title, const QString& url, QWidget* parent)
    : QWidget{parent}
    , m_font(font)
    , m_title(title)
    , m_url(url)
{
  setCursor(score::Skin::instance().CursorPointingHand);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setFixedHeight(m_height);
}

void InteractiveLabel::setPixmaps(const QPixmap& pixmap, const QPixmap& pixmapOn)
{
  m_pixmap = pixmap;
  m_pixmapOn = pixmapOn;
  // Never let a big icon overflow the label
  const int h = std::max(
      int(m_pixmap.height() / m_pixmap.devicePixelRatio()),
      int(m_pixmapOn.height() / m_pixmapOn.devicePixelRatio()));
  if(h + 4 > m_height)
    setItemHeight(h + 4);
  updateGeometry();
}

void InteractiveLabel::setItemHeight(int px)
{
  m_height = px;
  setFixedHeight(px);
  updateGeometry();
}

void InteractiveLabel::disableInteractivity()
{
  m_interactive = false;
  setCursor(score::Skin::instance().CursorPointer);
  update();
}

void InteractiveLabel::setActiveColor(const QColor& c)
{
  m_activeColor = c;
  update();
}

void InteractiveLabel::setInactiveColor(const QColor& c)
{
  m_inactiveColor = c;
  update();
}

void InteractiveLabel::setChecked(bool b)
{
  if(m_checked == b)
    return;
  m_checked = b;
  update();
}

void InteractiveLabel::setText(const QString& text)
{
  m_title = text;
  updateGeometry();
  update();
}

QSize InteractiveLabel::sizeHint() const
{
  int w = m_leftPadding + iconWidth();
  if(!m_title.isEmpty())
    w += QFontMetrics{m_font}.horizontalAdvance(m_title) + 6;
  return {w, m_height};
}

QSize InteractiveLabel::minimumSizeHint() const
{
  // Allow the layout to squeeze us: the text gets elided.
  int w = m_leftPadding + iconWidth();
  if(!m_title.isEmpty())
    w += 40;
  return {w, m_height};
}

//! Recolors a monochrome icon; the alpha channel is kept as-is
static QPixmap tintedPixmap(const QPixmap& source, const QColor& color)
{
  if(source.isNull())
    return source;
  static std::map<std::pair<qint64, QRgb>, QPixmap> cache;
  const auto key = std::make_pair(source.cacheKey(), color.rgba());
  if(auto it = cache.find(key); it != cache.end())
    return it->second;

  QPixmap res{source.size()};
  res.setDevicePixelRatio(source.devicePixelRatio());
  res.fill(Qt::transparent);
  {
    QPainter p{&res};
    p.drawPixmap(0, 0, source);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(res.rect(), color);
  }
  return cache.emplace(key, res).first->second;
}

void InteractiveLabel::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const bool hl = highlighted();

  if(m_checked && m_checkedBackground.isValid())
  {
    painter.fillRect(rect(), m_checkedBackground);
    painter.fillRect(QRect{0, 0, 3, height()}, m_activeColor);
  }

  QRectF textRect = rect().adjusted(m_leftPadding, 0, -4, 0);

  // The icon always takes the color of the text, whatever the state
  const QColor color = hl ? m_activeColor : m_inactiveColor;
  const QPixmap pm = tintedPixmap(m_pixmap, color);
  if(!pm.isNull())
  {
    const qreal w = pm.width() / pm.devicePixelRatio();
    const qreal h = pm.height() / pm.devicePixelRatio();
    // Icons of different sizes share the same column, centered on the same axis
    painter.drawPixmap(
        QPointF{textRect.x() + (m_iconColumn - 8 - w) / 2., (height() - h) / 2.}, pm);
    textRect.setX(textRect.x() + m_iconColumn);
  }

  if(!m_title.isEmpty())
  {
    painter.setPen(QPen{color});
    painter.setFont(m_font);
    const QString txt
        = QFontMetrics{m_font}.elidedText(m_title, m_elideMode, int(textRect.width()));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, txt);
  }
}

void InteractiveLabel::enterEvent(QEnterEvent* event)
{
  m_hovered = true;
  if(m_interactive)
  {
    update();
    hovered(true);
  }
}

void InteractiveLabel::leaveEvent(QEvent* event)
{
  m_hovered = false;
  m_pressed = false;
  if(m_interactive)
  {
    update();
    hovered(false);
  }
}

void InteractiveLabel::mousePressEvent(QMouseEvent* event)
{
  if(!m_interactive || event->button() != Qt::LeftButton)
    return QWidget::mousePressEvent(event);
  m_pressed = true;
  event->accept();
}

void InteractiveLabel::mouseReleaseEvent(QMouseEvent* event)
{
  if(!m_pressed || event->button() != Qt::LeftButton)
    return QWidget::mouseReleaseEvent(event);
  m_pressed = false;
  event->accept();

  if(!rect().contains(event->pos()))
    return;

  if(m_checkable)
    setChecked(true);

  if(m_openExternalLink)
    QDesktopServices::openUrl(QUrl(m_url));
  else
    labelPressed(m_url);
}

/**
 * @brief Preview of a score shown next to a hovered entry: thumbnail, name, author.
 */
class ThumbnailPopup final : public QWidget
{
public:
  static constexpr int Width = 264;
  static constexpr int ThumbHeight = 158;
  static constexpr int Margin = 6;
  static constexpr int DescriptionLines = 2;

  ThumbnailPopup(const QFont& titleFont, const QFont& subFont, QWidget* parent)
      : QWidget{parent}
      , m_titleFont{titleFont}
      , m_subFont{subFont}
  {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
  }

  void setContent(
      const QImage& thumbnail, const QString& title, const QString& author,
      const QString& description, const QString& path)
  {
    m_thumbnail = coverPixmap(thumbnail, {Width - 2 * Margin, ThumbHeight});
    m_title = title;
    m_author = author;
    m_description = description.simplified();
    m_path = path;

    const QFontMetrics tm{m_titleFont}, sm{m_subFont};
    int h = 2 * Margin + tm.height();
    if(!m_thumbnail.isNull())
      h += ThumbHeight + Margin;
    if(!m_author.isEmpty())
      h += sm.height();
    if(!m_description.isEmpty())
      h += 4 + DescriptionLines * sm.lineSpacing();
    if(!m_path.isEmpty())
      h += 4 + sm.height();
    resize(Width, h);
    update();
  }

  //! Shows the popup next to `anchor`, inside `bounds` (parent coordinates).
  void showFor(QWidget* anchor, const QRect& bounds)
  {
    const QRect a{anchor->mapTo(parentWidget(), QPoint{0, 0}), anchor->size()};
    int x = a.left() - width() - 12;
    if(x < bounds.left())
      x = a.right() + 12;
    if(x + width() > bounds.right())
      x = bounds.right() - width();
    int y = a.center().y() - height() / 2;
    y = std::clamp(y, bounds.top(), std::max(bounds.top(), bounds.bottom() - height()));
    move(x, y);
    raise();
    show();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addRoundedRect(QRectF{rect()}.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    p.fillPath(path, StartScreenColors::CardDark);
    p.setPen(QPen{StartScreenColors::Outline});
    p.drawPath(path);

    const int textX = Margin + 2;
    const int textW = width() - 2 * Margin - 4;
    const QFontMetrics tm{m_titleFont}, sm{m_subFont};
    int y = Margin;
    if(!m_thumbnail.isNull())
    {
      p.drawPixmap(Margin, y, m_thumbnail);
      y += ThumbHeight + Margin;
    }

    p.setPen(StartScreenColors::Text);
    p.setFont(m_titleFont);
    p.drawText(
        QRect{textX, y, textW, tm.height()}, Qt::AlignLeft | Qt::AlignVCenter,
        tm.elidedText(m_title, Qt::ElideMiddle, textW));
    y += tm.height();

    p.setFont(m_subFont);
    if(!m_author.isEmpty())
    {
      p.setPen(StartScreenColors::Hover);
      p.drawText(
          QRect{textX, y, textW, sm.height()}, Qt::AlignLeft | Qt::AlignVCenter,
          sm.elidedText(m_author, Qt::ElideRight, textW));
      y += sm.height();
    }
    if(!m_description.isEmpty())
    {
      y += 4;
      p.setPen(StartScreenColors::Text);
      const QRect descRect{textX, y, textW, DescriptionLines * sm.lineSpacing()};
      p.save();
      p.setClipRect(descRect);
      p.drawText(
          descRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_description);
      p.restore();
      y += descRect.height();
    }
    if(!m_path.isEmpty())
    {
      y += 4;
      p.setPen(StartScreenColors::Muted);
      p.drawText(
          QRect{textX, y, textW, sm.height()}, Qt::AlignLeft | Qt::AlignVCenter,
          sm.elidedText(m_path, Qt::ElideMiddle, textW));
    }
  }

private:
  QFont m_titleFont;
  QFont m_subFont;
  QPixmap m_thumbnail;
  QString m_title;
  QString m_author;
  QString m_description;
  QString m_path;
};

/**
 * @brief A score presented as a card: thumbnail, name, author.
 */
class ExampleCard final : public QWidget
{
public:
  static constexpr int Width = 190;
  static constexpr int ThumbHeight = 119;
  static constexpr int Height = ThumbHeight + 66;

  ExampleCard(
      const QFont& titleFont, const QFont& subFont, const QString& title,
      const QString& subtitle, const QString& path, QWidget* parent)
      : QWidget{parent}
      , m_titleFont{titleFont}
      , m_subFont{subFont}
      , m_title{title}
      , m_subtitle{subtitle}
      , m_path{path}
  {
    setFixedSize(Width, Height);
    setCursor(score::Skin::instance().CursorPointingHand);
    setToolTip(QDir::toNativeSeparators(path));
  }

  const QString& path() const noexcept { return m_path; }

  void setInfo(const ProjectInfo::Info& info)
  {
    if(!info.name.isEmpty())
      m_title = info.name;
    if(!info.author.isEmpty())
      m_subtitle = info.author;
    m_thumbnail = coverPixmap(info.thumbnail, {Width, ThumbHeight});
    QString tip = m_title;
    if(!info.author.isEmpty())
      tip += "\n" + info.author;
    if(!info.description.isEmpty())
      tip += "\n\n" + info.description;
    tip += "\n\n" + QDir::toNativeSeparators(m_path);
    setToolTip(tip);
    update();
  }

  std::function<void(const QString&)> onActivated;

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addRoundedRect(QRectF{rect()}.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    p.fillPath(path, StartScreenColors::Card);

    // Thumbnail area
    p.save();
    p.setClipPath(path);
    const QRect thumbRect{0, 0, Width, ThumbHeight};
    if(!m_thumbnail.isNull())
    {
      p.drawPixmap(thumbRect, m_thumbnail);
    }
    else
    {
      p.fillRect(thumbRect, StartScreenColors::CardDark);
      static const QPixmap placeholder
          = score::get_pixmap(":/icons/load_examples_off.png");
      const qreal w = placeholder.width() / placeholder.devicePixelRatio();
      const qreal h = placeholder.height() / placeholder.devicePixelRatio();
      p.setOpacity(0.35);
      p.drawPixmap(QPointF{(Width - w) / 2., (ThumbHeight - h) / 2.}, placeholder);
      p.setOpacity(1.);
    }
    p.restore();

    p.setPen(QPen{m_hovered ? StartScreenColors::Hover : StartScreenColors::Outline, 1});
    p.drawPath(path);

    const int textX = 8;
    const int textW = Width - 16;
    p.setPen(m_hovered ? StartScreenColors::Hover : StartScreenColors::Text);
    p.setFont(m_titleFont);
    const QFontMetrics tm{m_titleFont};
    p.drawText(
        QRect{textX, ThumbHeight + 6, textW, tm.height()},
        Qt::AlignLeft | Qt::AlignVCenter, tm.elidedText(m_title, Qt::ElideRight, textW));

    p.setPen(StartScreenColors::Muted);
    p.setFont(m_subFont);
    const QFontMetrics sm{m_subFont};
    p.drawText(
        QRect{textX, ThumbHeight + 6 + tm.height(), textW, sm.height()},
        Qt::AlignLeft | Qt::AlignVCenter,
        sm.elidedText(m_subtitle, Qt::ElideRight, textW));

    // Full path, so that the user knows which file is going to be opened
    p.setOpacity(0.7);
    p.drawText(
        QRect{textX, ThumbHeight + 8 + tm.height() + sm.height(), textW, sm.height()},
        Qt::AlignLeft | Qt::AlignVCenter,
        sm.elidedText(QDir::toNativeSeparators(m_path), Qt::ElideMiddle, textW));
    p.setOpacity(1.);
  }

  void enterEvent(QEnterEvent*) override
  {
    m_hovered = true;
    update();
  }
  void leaveEvent(QEvent*) override
  {
    m_hovered = false;
    m_pressed = false;
    update();
  }
  void mousePressEvent(QMouseEvent* e) override
  {
    if(e->button() == Qt::LeftButton)
    {
      m_pressed = true;
      e->accept();
    }
  }
  void mouseReleaseEvent(QMouseEvent* e) override
  {
    const bool activate
        = m_pressed && e->button() == Qt::LeftButton && rect().contains(e->pos());
    m_pressed = false;
    if(activate && onActivated)
      onActivated(m_path);
  }

private:
  QFont m_titleFont;
  QFont m_subFont;
  QString m_title;
  QString m_subtitle;
  QString m_path;
  QPixmap m_thumbnail;
  bool m_hovered{};
  bool m_pressed{};
};

/**
 * @brief The window shown when score starts without a document.
 *
 * Layout:
 *
 *   +---------------------------------------------------------------+
 *   | splash artwork: logo, tagline, version      [update notice] × |
 *   +-----------+---------------------------------------------------+
 *   | Home      |                                                   |
 *   | Examples  |   page (home / examples / learn / community)      |
 *   | Learn     |                                                   |
 *   | Community |                                                   |
 *   | Exit      |                                                   |
 *   +-----------+---------------------------------------------------+
 *
 * Every way of leaving the screen without choosing anything (Escape, the ×
 * button, a close request) opens a new empty score, so the user always ends
 * up in a usable editor.
 *
 * Templates, examples and recent files are plain .score files; their project
 * information (name, author, thumbnail) is read in the background with
 * ProjectInfo::peek() and shown on hover or on the example cards.
 */
class StartScreen : public QWidget
{
  W_OBJECT(StartScreen)
public:
  StartScreen(const QPointer<QRecentFilesMenu>& recentFiles, QWidget* parent = nullptr);

  void openNewDocument() W_SIGNAL(openNewDocument)
  void openFile(const QString& file) W_SIGNAL(openFile, file)
  //! Opens the file as a new untitled document (templates, examples).
  void openTemplate(const QString& file) W_SIGNAL(openTemplate, file)
  void openFileDialog() W_SIGNAL(openFileDialog)
  void loadCrashedSession() W_SIGNAL(loadCrashedSession)
  //! Join a collaborative session hosted by another instance of score.
  void joinSession() W_SIGNAL(joinSession)
  void exitApp() W_SIGNAL(exitApp)

  //! Shows the "Restore last session" entry.
  void addLoadCrashedSession();
  //! Shows the "Join a collaborative session" entry.
  void addJoinSession();
  //! Closes the start screen without opening anything (something else took over).
  void dismiss();
  //! Shows the start screen again after an action that led nowhere (cancelled dialog...)
  void reopen();

  static constexpr int Width = 880;
  static constexpr int Height = 640;
  static constexpr int HeaderHeight = 220;
  static constexpr int NavWidth = 190;
  static constexpr int MaxRecentFiles = 8;

protected:
  void paintEvent(QPaintEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

private:
  struct Link
  {
    QString text;
    QString url;
    QString icon; // base name in :/icons, without the _on / _off suffix
    QString tooltip;
  };

  QWidget* createHeader();
  QWidget* createNavigation();
  //! Pages other than Home are built the first time they are shown: scanning the
  //! library for templates and examples must not delay the start screen.
  int addPage(const QString& name, const QString& icon, std::function<QWidget*()> make);
  QWidget* createHomePage(const QPointer<QRecentFilesMenu>& recentFiles);
  QWidget* createTemplatesPage();
  QWidget* createAboutPage();
  QWidget* createExamplesPage();
  //! A scrollable grid of thumbnail cards, one per document
  QWidget* createCardsPage(
      const QString& title, const QString& hint, const QString& emptyHint,
      const std::vector<DocumentTemplate>& docs,
      std::function<void(const QString&)> onActivated, const Link& more);
  QWidget* createLinksPage(const QString& title, const std::vector<Link>& links);

  int addPage(const QString& name, const QString& icon, QWidget* page);
  void setCurrentPage(int index);

  QLabel* makeSectionTitle(const QString& text, QWidget* parent);
  QLabel* makeHint(const QString& text, QWidget* parent);
  InteractiveLabel* makeItem(
      const QString& text, const QString& icon, const QString& url, QWidget* parent);
  //! Same, drawn in the accent color with the matching icon state.
  InteractiveLabel* makeAccentItem(
      const QString& text, const QString& icon, const QString& url, QWidget* parent);
  InteractiveLabel* makeExternalLink(const Link& link, QWidget* parent);
  //! An entry that opens a score (recent file or template) and previews it on hover.
  InteractiveLabel* makeScoreItem(
      const QString& text, const QString& icon, const QString& path, bool asTemplate,
      QWidget* parent);

  // Wraps the emission of the choice signals: once the user chose something,
  // closing the window must not additionally open a new document.
  template <typename F>
  void choose(F&& emitSignal)
  {
    if(m_actionTaken)
      return;
    m_actionTaken = true;
    emitSignal();
  }

  //! Opens an example as a new document, and its web page if it has one.
  void openExample(const QString& path);
  void requestInfo(const QString& path);
  void onInfoLoaded(const QString& path, const std::optional<ProjectInfo::Info>& info);
  void showPreview(InteractiveLabel* label);

  void checkForNewVersion();
  void showUpdateAvailable(const QString& version);

  QFont m_navFont;
  QFont m_sectionFont;
  QFont m_itemFont;
  QFont m_smallFont;
  QFont m_versionFont;

  QPixmap m_background;

  QStackedWidget* m_pages{};
  std::vector<InteractiveLabel*> m_navItems;
  std::vector<std::function<QWidget*()>> m_pageFactories; //!< empty once built
  QVBoxLayout* m_navLayout{};

  InteractiveLabel* m_updateLabel{};
  InteractiveLabel* m_crashLabel{};
  InteractiveLabel* m_joinLabel{};
  int m_templatesPage{};
  ThumbnailPopup* m_preview{};

  std::map<QString, std::optional<ProjectInfo::Info>> m_infos;
  std::vector<ExampleCard*> m_cards;

  bool m_firstRun{};
  bool m_actionTaken{};
};

StartScreen::StartScreen(const QPointer<QRecentFilesMenu>& recentFiles, QWidget* parent)
    : QWidget(parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointer);
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  setWindowModality(Qt::ApplicationModal);
  setFocusPolicy(Qt::StrongFocus);
  setFixedSize(Width, Height);

  {
    QSettings s;
    m_firstRun = !s.value("score/StartScreenSeen", false).toBool();
    s.setValue("score/StartScreenSeen", true);
  }

  // px, not pt: pt would shrink on macOS' 72 DPI.
  m_navFont = QFont("Montserrat");
  m_navFont.setPixelSize(16);
  m_navFont.setWeight(QFont::DemiBold);
  m_sectionFont = QFont("Montserrat");
  m_sectionFont.setPixelSize(13);
  m_sectionFont.setWeight(QFont::Medium);
  m_sectionFont.setCapitalization(QFont::AllUppercase);
  m_sectionFont.setLetterSpacing(QFont::PercentageSpacing, 108);
  m_itemFont = QFont("Ubuntu");
  m_itemFont.setPixelSize(16);
  m_itemFont.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  m_itemFont.setStyleStrategy(QFont::PreferAntialias);
  m_smallFont = QFont("Ubuntu");
  m_smallFont.setPixelSize(13);
  m_smallFont.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  m_smallFont.setStyleStrategy(QFont::PreferAntialias);
  m_versionFont = QFont("Ubuntu");
  m_versionFont.setPixelSize(19);
  m_versionFont.setWeight(QFont::Light);
  m_versionFont.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  m_versionFont.setStyleStrategy(QFont::PreferAntialias);

  m_background = score::get_pixmap(":/startscreen/startscreensplash.png");

  auto mainLayout = new QVBoxLayout{this};
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  mainLayout->addWidget(createHeader());

  auto body = new QHBoxLayout;
  body->setContentsMargins(0, 0, 0, 0);
  body->setSpacing(0);

  m_pages = new QStackedWidget{this};
  body->addWidget(createNavigation());
  body->addWidget(m_pages, 1);
  mainLayout->addLayout(body, 1);

  addPage(tr("Home"), "home", createHomePage(recentFiles));
  m_templatesPage
      = addPage(tr("Templates"), "new_file", [this] { return createTemplatesPage(); });
  addPage(tr("Examples"), "load_examples", [this] { return createExamplesPage(); });
  addPage(
      tr("Learn"), "learn",
      createLinksPage(
          tr("Learn ossia score"),
          {{tr("Quick start guide"), "https://ossia.io/score-docs/quick-start.html",
            "version", tr("The recommended starting point if you are new to score")},
           {tr("Video tutorials"),
            "https://www.youtube.com/"
            "watch?v=R-3d8K6gQkw&list=PLIHLSiZpIa6YoY1_aW1yetDgZ7tZcxfEC",
            "tutorials",
            {}},
              {tr("Documentation"), "https://ossia.io/score-docs/", "learn", {}},
              {tr("Process reference"), "https://ossia.io/score-docs/processes.html", "learn", {}},
              {tr("Device reference"), "https://ossia.io/score-docs/devices.html", "learn", {}},
              {tr("Web version"), "https://ossia.io/score-web/", "tutorials", {}},
          }));
  addPage(
      tr("Community"), "community",
      createLinksPage(
          tr("Get involved"),
          {{tr("GitHub Discussions"), "https://github.com/ossia/score/discussions",
            "forum", tr("Ask questions and share what you do with score")},
           {tr("Discord Chat"), "https://discord.gg/8Hzm4UduaS", "chat", {}},
           {tr("Report a bug or suggest a feature"),
            "https://github.com/ossia/score/issues",
            "new_file",
            {}},
           {tr("Support ossia score through the Suite SAT"), "https://suite.sat.qc.ca/en",
            "contribute",
            tr("ossia score is free software: donations fund its development")},
           {tr("Donate on Open Collective"), "https://opencollective.com/ossia",
            "contribute", tr("ossia score is free software: donations fund its development")},
           {tr("Contributing"), "https://ossia.io/score-docs/development", "contribute",
               tr("Come implement your dream feature!")},
          }));
  addPage(tr("About"), "about", [this] { return createAboutPage(); });

  m_navLayout->addStretch();

  auto exitLabel = makeItem(tr("Exit"), "exit", "", this);
  exitLabel->setLeftPadding(20);
  exitLabel->setItemHeight(40);
  connect(exitLabel, &InteractiveLabel::labelPressed, this, [this] {
    choose([this] { exitApp(); });
  });
  m_navLayout->addWidget(exitLabel);

  // Created last so that it is above every page
  m_preview = new ThumbnailPopup{m_itemFont, m_smallFont, this};

  setCurrentPage(0);

  checkForNewVersion();
}

QWidget* StartScreen::createHeader()
{
  // The artwork itself is painted in paintEvent; this widget only hosts the
  // controls that sit on top of it.
  auto header = new QWidget{this};
  header->setFixedHeight(HeaderHeight);
  auto lay = new QHBoxLayout{header};
  lay->setContentsMargins(0, 10, 10, 0);
  lay->setSpacing(12);
  lay->addStretch();

  m_updateLabel = new InteractiveLabel{
      m_navFont, {}, "https://github.com/ossia/score/releases/latest/", header};
  m_updateLabel->setOpenExternalLink(true);
  m_updateLabel->setPixmaps(
      score::get_pixmap(":/icons/version_off.png"),
      score::get_pixmap(":/icons/version_on.png"));
  m_updateLabel->setInactiveColor(StartScreenColors::Accent);
  m_updateLabel->hide();
  lay->addWidget(m_updateLabel, 0, Qt::AlignTop);

  auto closeLabel = new InteractiveLabel{m_navFont, {}, "", header};
  closeLabel->setPixmaps(
      score::get_pixmap(":/icons/close_window_off.png"),
      score::get_pixmap(":/icons/close_window_on.png"));
  closeLabel->setLeftPadding(0);
  closeLabel->setIconColumn(32);
  closeLabel->setFixedWidth(32);
  closeLabel->setToolTip(tr("Close this window and start with an empty score"));
  connect(closeLabel, &InteractiveLabel::labelPressed, this, [this] {
    choose([this] { openNewDocument(); });
  });
  lay->addWidget(closeLabel, 0, Qt::AlignTop);

  return header;
}

QWidget* StartScreen::createNavigation()
{
  auto nav = new QWidget{this};
  nav->setFixedWidth(NavWidth);
  m_navLayout = new QVBoxLayout{nav};
  // The first entry starts exactly where the page panel starts
  m_navLayout->setContentsMargins(0, 0, 0, 16);
  m_navLayout->setSpacing(4);
  return nav;
}

int StartScreen::addPage(const QString& name, const QString& icon, QWidget* page)
{
  const int index = addPage(name, icon, std::function<QWidget*()>{});
  m_pages->widget(index)->layout()->addWidget(page);
  return index;
}

int StartScreen::addPage(
    const QString& name, const QString& icon, std::function<QWidget*()> make)
{
  // A host widget per page; the content is added to it now or on first display
  auto host = new QWidget;
  auto hostLayout = new QVBoxLayout{host};
  hostLayout->setContentsMargins(0, 0, 0, 0);
  const int index = m_pages->addWidget(host);
  m_pageFactories.resize(index + 1);
  m_pageFactories[index] = std::move(make);

  auto item = makeItem(name, icon, "", this);
  item->setCheckable(true);
  item->setCheckedBackground(StartScreenColors::Panel);
  item->setLeftPadding(20);
  item->setItemHeight(44);
  connect(item, &InteractiveLabel::labelPressed, this, [this, index] {
    setCurrentPage(index);
  });

  m_navItems.push_back(item);
  m_navLayout->addWidget(item);
  return index;
}

void StartScreen::setCurrentPage(int index)
{
  if(index >= 0 && index < int(m_pageFactories.size()))
  {
    if(auto make = std::exchange(m_pageFactories[index], {}))
      m_pages->widget(index)->layout()->addWidget(make());
  }
  m_pages->setCurrentIndex(index);
  for(int i = 0; i < int(m_navItems.size()); i++)
    m_navItems[i]->setChecked(i == index);
  if(m_preview)
    m_preview->hide();
}

QLabel* StartScreen::makeSectionTitle(const QString& text, QWidget* parent)
{
  auto label = new QLabel{text, parent};
  label->setFont(m_sectionFont);
  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, StartScreenColors::Muted);
  label->setPalette(pal);
  return label;
}

QLabel* StartScreen::makeHint(const QString& text, QWidget* parent)
{
  auto hint = new QLabel{text, parent};
  hint->setFont(m_itemFont);
  hint->setWordWrap(true);
  QPalette pal = hint->palette();
  pal.setColor(QPalette::WindowText, StartScreenColors::Muted);
  hint->setPalette(pal);
  return hint;
}

InteractiveLabel* StartScreen::makeItem(
    const QString& text, const QString& icon, const QString& url, QWidget* parent)
{
  auto label = new InteractiveLabel{m_itemFont, text, url, parent};
  if(!icon.isEmpty())
  {
    label->setPixmaps(
        score::get_pixmap(QString(":/icons/%1_off.png").arg(icon)),
        score::get_pixmap(QString(":/icons/%1_on.png").arg(icon)));
  }
  return label;
}

InteractiveLabel* StartScreen::makeAccentItem(
    const QString& text, const QString& icon, const QString& url, QWidget* parent)
{
  // Accent text at rest; hovering behaves like every other item
  auto label = makeItem(text, icon, url, parent);
  label->setInactiveColor(StartScreenColors::Accent);
  return label;
}

InteractiveLabel* StartScreen::makeExternalLink(const Link& link, QWidget* parent)
{
  auto label = makeItem(link.text, link.icon, link.url, parent);
  label->setOpenExternalLink(true);
  label->setToolTip(link.tooltip.isEmpty() ? link.url : link.tooltip);
  return label;
}

InteractiveLabel* StartScreen::makeScoreItem(
    const QString& text, const QString& icon, const QString& path, bool asTemplate,
    QWidget* parent)
{
  auto label = makeItem(text, icon, path, parent);
  label->setElideMode(Qt::ElideMiddle);
  if(asTemplate)
  {
    connect(label, &InteractiveLabel::labelPressed, this, [this](const QString& file) {
      choose([&] { openTemplate(file); });
    });
  }
  else
  {
    connect(label, &InteractiveLabel::labelPressed, this, [this](const QString& file) {
      choose([&] { openFile(file); });
    });
  }
  connect(label, &InteractiveLabel::hovered, this, [this, label](bool on) {
    if(on)
      showPreview(label);
    else
      m_preview->hide();
  });

  requestInfo(path);
  return label;
}

QWidget* StartScreen::createHomePage(const QPointer<QRecentFilesMenu>& recentFiles)
{
  auto page = new QWidget;
  auto lay = new QHBoxLayout{page};
  lay->setContentsMargins(28, 24, 28, 24);
  lay->setSpacing(32);

  const QString firstRunScore = firstRunDocumentTemplate();

  // Left column: create
  {
    auto col = new QVBoxLayout;
    col->setSpacing(6);
    col->addWidget(makeSectionTitle(tr("Start"), page));

    auto newLabel = makeItem(tr("New empty score"), "new_file", "", page);
    connect(newLabel, &InteractiveLabel::labelPressed, this, [this] {
      choose([this] { openNewDocument(); });
    });
    col->addWidget(newLabel);

    auto templatesLabel = makeItem(tr("Start from a template..."), "new_file", "", page);
    connect(templatesLabel, &InteractiveLabel::labelPressed, this, [this] {
      setCurrentPage(m_templatesPage);
    });
    col->addWidget(templatesLabel);

    auto openLabel = makeItem(tr("Open a score file..."), "load", "", page);
    connect(openLabel, &InteractiveLabel::labelPressed, this, [this] {
      choose([this] { openFileDialog(); });
    });
    col->addWidget(openLabel);

    m_joinLabel
        = makeItem(tr("Join a collaborative session..."), "net_session", "", page);
    m_joinLabel->setToolTip(tr("Connect to a score session hosted on another computer"));
    m_joinLabel->hide();
    connect(m_joinLabel, &InteractiveLabel::labelPressed, this, [this] {
      choose([this] { joinSession(); });
    });
    col->addWidget(m_joinLabel);

    col->addSpacing(18);
    col->addWidget(makeSectionTitle(tr("New to ossia score?"), page));

    if(!firstRunScore.isEmpty())
    {
      if(m_firstRun)
      {
        col->addWidget(makeHint(
            tr("Welcome! This guided score shows you around the interface and "
               "the main concepts in a few minutes."),
            page));
      }
      auto guided = makeAccentItem(
          tr("Open the demo project"), "version", firstRunScore, page);
      connect(guided, &InteractiveLabel::labelPressed, this, [this](const QString& f) {
        choose([&] { openTemplate(f); });
      });
      col->addWidget(guided);

      col->addWidget(makeExternalLink(
          {tr("Read the quick start guide"),
           "https://ossia.io/score-docs/quick-start.html",
           "version",
           {}},
          page));
    }
    else
    {
      col->addWidget(makeHint(
          tr("Start with the quick start guide: it walks you through the "
             "interface and your first score."),
          page));
      auto guide = makeAccentItem(
          tr("Read the quick start guide"), "version",
          "https://ossia.io/score-docs/quick-start.html", page);
      guide->setOpenExternalLink(true);
      col->addWidget(guide);
    }

    col->addStretch();
    lay->addLayout(col, 1);
  }

  // Right column: open
  {
    auto col = new QVBoxLayout;
    col->setSpacing(6);
    col->addWidget(makeSectionTitle(tr("Recent"), page));

    int shown = 0;
    if(recentFiles)
    {
      for(const auto& action : recentFiles->actions())
      {
        if(shown++ >= MaxRecentFiles)
          break;

        const QString path = action->data().toString();
        col->addWidget(
            makeScoreItem(QFileInfo{path}.fileName(), "load", path, false, page));
      }
    }

    if(shown == 0)
    {
      auto none = makeItem(tr("No recent scores yet"), "", "", page);
      none->disableInteractivity();
      none->setInactiveColor(StartScreenColors::Muted);
      col->addWidget(none);
    }

    col->addSpacing(6);

    m_crashLabel = makeAccentItem(tr("Restore last session"), "reload_crash", "", page);
    m_crashLabel->setToolTip(
        tr("score did not exit cleanly last time: reopen the documents that were open"));
    m_crashLabel->hide();
    connect(m_crashLabel, &InteractiveLabel::labelPressed, this, [this] {
      choose([this] { loadCrashedSession(); });
    });
    col->addWidget(m_crashLabel);

    col->addStretch();
    lay->addLayout(col, 1);
  }

  return page;
}

QWidget* StartScreen::createTemplatesPage()
{
  return createCardsPage(
      tr("Templates"),
      tr("A template opens as a new untitled score with devices, processes and a "
         "structure already in place."),
      tr("No templates are installed yet. Templates are .score files in the "
         "Templates folder of your user library (%1) or of an installed package.")
          .arg(QDir::toNativeSeparators(libraryRootPath())),
      availableDocumentTemplates(),
      [this](const QString& path) { choose([&] { openTemplate(path); }); },
      {tr("Learn how to write your own templates"),
       "https://ossia.io/score-docs/",
       "learn",
       {}});
}

QWidget* StartScreen::createAboutPage()
{
  // Shared with Help > About
  score::AboutWidget::Style style;
  style.sectionFont = m_sectionFont;
  style.itemFont = m_itemFont;
  style.smallFont = m_smallFont;
  style.text = StartScreenColors::Text;
  style.muted = StartScreenColors::Muted;
  style.hover = StartScreenColors::Hover;
  style.version = StartScreenColors::Version;
  style.outline = StartScreenColors::Outline;

  auto page = new QWidget;
  auto lay = new QVBoxLayout{page};
  lay->setContentsMargins(28, 20, 28, 16);
  lay->addWidget(new score::AboutWidget{style, page});
  return page;
}

QWidget* StartScreen::createExamplesPage()
{
  return createCardsPage(
      tr("Example scores"),
      tr("Each example opens as a new, untitled score."),
      tr("No example scores are installed yet. Examples are .score files in the "
         "Examples folder of your user library (%1) or of an installed package.")
          .arg(QDir::toNativeSeparators(libraryRootPath())),
      availableExampleDocuments(), [this](const QString& path) { openExample(path); },
      {tr("More examples online"),
       "https://ossia.io/score-docs/examples",
       "load_examples",
       {}});
}

QWidget* StartScreen::createCardsPage(
    const QString& title, const QString& hint, const QString& emptyHint,
    const std::vector<DocumentTemplate>& docs,
    std::function<void(const QString&)> onActivated, const Link& more)
{
  auto page = new QWidget;
  auto lay = new QVBoxLayout{page};
  lay->setContentsMargins(28, 24, 28, 16);
  lay->setSpacing(8);

  lay->addWidget(makeSectionTitle(title, page));

  if(docs.empty())
  {
    lay->addWidget(makeHint(emptyHint, page));
  }
  else
  {
    lay->addWidget(makeHint(hint, page));

    auto scroll = new QScrollArea{page};
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto container = new QWidget;
    container->setAutoFillBackground(false);
    auto grid = new QGridLayout{container};
    grid->setContentsMargins(0, 4, 8, 4);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);

    static constexpr int columns = 3;
    int i = 0;
    for(const auto& doc : docs)
    {
      const QString subtitle = !doc.category.isEmpty()   ? doc.category
                               : doc.source == "library" ? tr("User library")
                                                         : doc.source;
      auto card = new ExampleCard{m_itemFont, m_smallFont, doc.name,
                                  subtitle,   doc.path,    container};
      card->onActivated = onActivated;
      grid->addWidget(card, i / columns, i % columns, Qt::AlignLeft | Qt::AlignTop);
      m_cards.push_back(card);
      requestInfo(doc.path);
      i++;
    }
    grid->setColumnStretch(columns, 1);
    grid->setRowStretch((i + columns - 1) / columns, 1);

    scroll->setWidget(container);
    lay->addWidget(scroll, 1);
  }

  lay->addWidget(makeExternalLink(more, page));

  if(docs.empty())
    lay->addStretch();

  return page;
}

QWidget*
StartScreen::createLinksPage(const QString& title, const std::vector<Link>& links)
{
  auto page = new QWidget;
  auto lay = new QVBoxLayout{page};
  lay->setContentsMargins(28, 24, 28, 24);
  lay->setSpacing(6);

  lay->addWidget(makeSectionTitle(title, page));
  for(const auto& link : links)
    lay->addWidget(makeExternalLink(link, page));
  lay->addStretch();

  return page;
}

void StartScreen::openExample(const QString& path)
{
  if(m_actionTaken)
    return;

  // The information is normally already loaded in the background; if the
  // user was faster than the parser, read it now.
  std::optional<ProjectInfo::Info> info;
  if(auto it = m_infos.find(path); it != m_infos.end() && it->second)
    info = it->second;
  else
    info = ProjectInfo::peek(path);

  if(info && !info->url.isEmpty())
  {
    if(const QUrl url{info->url}; url.isValid() && !url.scheme().isEmpty())
      QDesktopServices::openUrl(url);
  }

  choose([&] { openTemplate(path); });
}

void StartScreen::requestInfo(const QString& path)
{
  if(m_infos.find(path) != m_infos.end())
    return;
  m_infos.emplace(path, std::nullopt);

  // Parsing a big .score file can take a moment: do it off the GUI thread.
  score::TaskPool::instance().post([path, self = QPointer{this}] {
    auto info = ProjectInfo::peek(path);
    QMetaObject::invokeMethod(
        QCoreApplication::instance(), [self, path, info = std::move(info)] {
      if(self)
        self->onInfoLoaded(path, info);
    }, Qt::QueuedConnection);
  });
}

void StartScreen::onInfoLoaded(
    const QString& path, const std::optional<ProjectInfo::Info>& info)
{
  m_infos[path] = info;
  if(!info)
    return;

  for(auto card : m_cards)
    if(card->path() == path)
      card->setInfo(*info);
}

void StartScreen::showPreview(InteractiveLabel* label)
{
  const QString& path = label->url();
  ProjectInfo::Info info;
  if(auto it = m_infos.find(path); it != m_infos.end() && it->second)
    info = *it->second;

  m_preview->setContent(
      info.thumbnail, info.name.isEmpty() ? label->text() : info.name, info.author,
      info.description, QDir::toNativeSeparators(path));
  m_preview->showFor(
      label, QRect{
                 NavWidth + 8, HeaderHeight + 8, Width - NavWidth - 16,
                 Height - HeaderHeight - 16});
}

void StartScreen::addLoadCrashedSession()
{
  m_crashLabel->show();
  update();
}

void StartScreen::addJoinSession()
{
  m_joinLabel->show();
  update();
}

void StartScreen::dismiss()
{
  m_actionTaken = true;
  close();
}

void StartScreen::reopen()
{
  m_actionTaken = false;
  show();
  raise();
  activateWindow();
}

void StartScreen::checkForNewVersion()
{
  // The request itself is asynchronous (QNetworkAccessManager); the reply is
  // handled on the GUI thread and only then touches the widgets.
  auto& tp = score::ThreadPool::instance();
  auto t = tp.acquireThread();
  QMetaObject::invokeMethod(t, [t, self = QPointer{this}] {
    auto getLastVersion = new HTTPGet{
        QUrl("https://ossia.io/score-last-version.txt"), [self](const QByteArray& data) {
      const auto version = QString::fromUtf8(data.simplified());
      if(QVersionNumber::fromString(version)
         > QVersionNumber::fromString(SCORE_TAG_NO_V))
      {
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, version] {
          if(self)
            self->showUpdateAvailable(version);
        });
      }
    }, [] { }};
    connect(getLastVersion, &QObject::destroyed, t, [] {
      QMetaObject::invokeMethod(QCoreApplication::instance(), [] {
        auto& tp = score::ThreadPool::instance();
        tp.releaseThread();
      });
    });
  });
}

void StartScreen::showUpdateAvailable(const QString& version)
{
  m_updateLabel->setText(
      tr("New version %1 is available, click to update").arg(version));
  m_updateLabel->setToolTip(tr("Open the download page in your browser"));
  m_updateLabel->show();
}

void StartScreen::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // The splash artwork covers the whole window: logo and tagline in the
  // header, decorations behind the navigation column.
  painter.fillRect(rect(), Qt::black);
  qreal scale = 1.;
  if(!m_background.isNull())
  {
    // Scaled uniformly to cover the window, anchored top-left so that the logo
    // and tagline keep their place; the overflow is cropped at the bottom.
    const QSizeF logical = m_background.deviceIndependentSize();
    scale = std::max(width() / logical.width(), height() / logical.height());
    painter.drawPixmap(
        QRectF{QPointF{}, logical * scale}, m_background, m_background.rect());
  }

  // The version sits under the tagline, which is part of the artwork
  painter.setFont(m_versionFont);
  painter.setPen(QPen{StartScreenColors::Version});
  {
    // Branch builds carry the branch name: keep it inside the header
    const qreal x = 217 * scale;
    const QFontMetrics fm{m_versionFont};
    painter.drawText(
        QPointF(x, 188 * scale), fm.elidedText(
                                     QCoreApplication::applicationVersion(),
                                     Qt::ElideMiddle, int(width() - x - 60)));
  }

  // Dim the artwork behind the navigation so that its text stays readable
  painter.fillRect(
      QRect{0, HeaderHeight, NavWidth, Height - HeaderHeight}, QColor{0, 0, 0, 120});

  // Page panel. The selected navigation item paints itself with the panel
  // color so that it visually connects to the page.
  painter.fillRect(
      QRect{NavWidth, HeaderHeight, Width - NavWidth, Height - HeaderHeight},
      StartScreenColors::Panel);
}

void StartScreen::keyPressEvent(QKeyEvent* event)
{
  switch(event->key())
  {
    case Qt::Key_Escape:
      choose([this] { openNewDocument(); });
      event->accept();
      return;
    case Qt::Key_Left:
    case Qt::Key_Up:
      setCurrentPage(
          (m_pages->currentIndex() + m_pages->count() - 1) % m_pages->count());
      event->accept();
      return;
    case Qt::Key_Right:
    case Qt::Key_Down:
      setCurrentPage((m_pages->currentIndex() + 1) % m_pages->count());
      event->accept();
      return;
    default:
      QWidget::keyPressEvent(event);
  }
}

void StartScreen::closeEvent(QCloseEvent* event)
{
  // Closed by other means (e.g. the window manager): make sure that the user
  // does not end up with an empty application window.
  choose([this] { openNewDocument(); });
  QWidget::closeEvent(event);
}
}

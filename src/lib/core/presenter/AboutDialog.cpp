#include "AboutDialog.hpp"

#include <score/widgets/Pixmap.hpp>

#include <QGuiApplication>
#include <QScreen>
#include <QUrl>
#include <QListWidget>
#include <QPlainTextEdit>
#include <score_git_info.hpp>
#if __has_include(<score_licenses.hpp>)
#include <score_licenses.hpp>
#endif

#include <QDate>
#include <QDesktopServices>
#include <QPainter>
#include <map>
namespace score
{
AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
    , m_windowSize(492, 437)
    , m_backgroundImage(score::get_image(":/about/about_background.png"))
    , m_catamaranFont("Catamaran", 13, QFont::Weight::Normal)
    , m_montserratFont("Montserrat", 10, QFont::Weight::Normal)
    , m_mouseAreaOssiaScore(102, 13, 295, 84)
    , m_mouseAreaLabri(16, 218, 116, 55)
    , m_mouseAreaScrime(21, 275, 114, 40)
    , m_mouseAreaBlueYeti(33, 321, 85, 84)
{
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
  resize(m_windowSize.width(), m_windowSize.height());
  setMouseTracking(true);

  if (auto scr = qApp->screens(); !scr.empty())
  {
    auto dpi = scr.first()->devicePixelRatio();
    if (dpi >= 2.)
    {
      m_catamaranFont.setPointSize(13);
      m_montserratFont.setPointSize(10);
    }
    else
    {
      m_catamaranFont.setPointSize(11);
      m_montserratFont.setPointSize(9);
    }
  }

}

void AboutDialog::mousePressEvent(QMouseEvent* event)
{
  QPointF pos = event->localPos();
  if (m_mouseAreaOssiaScore.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://ossia.io/"));
  }
  else if (m_mouseAreaLabri.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://www.labri.fr/"));
  }
  else if (m_mouseAreaScrime.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://scrime.u-bordeaux.fr/"));
  }
  else if (m_mouseAreaBlueYeti.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("http://www.blueyeti.fr/"));
  }
  this->close();
}

void AboutDialog::mouseMoveEvent(QMouseEvent* event)
{
  QPointF pos = event->localPos();
  if (m_mouseAreaOssiaScore.contains(pos) || m_mouseAreaLabri.contains(pos)
      || m_mouseAreaScrime.contains(pos) || m_mouseAreaBlueYeti.contains(pos))
  {
    this->setCursor(Qt::PointingHandCursor);
  }
  else
  {
    this->setCursor(Qt::ArrowCursor);
  }
}
void AboutDialog::paintEvent(QPaintEvent* event)
{
  QPen textPen(QColor("#0092cf"));
  QPen titleText(QColor("#aaaaaa"));
  QPen rectPen(QColor("#03c3dd"));
  QBrush rectBrush(QColor(18, 23, 26));

  // draw background image
  QPainter painter(this);
  painter.drawImage(QPoint(0, 0), m_backgroundImage);

  // write version and commit
  {
    auto version_text = QStringLiteral("Version: %1.%2.%3")
        .arg(SCORE_VERSION_MAJOR)
        .arg(SCORE_VERSION_MINOR)
        .arg(SCORE_VERSION_PATCH);
    if (std::string_view(SCORE_VERSION_EXTRA).size() != 0) {
      version_text += QStringLiteral("-%4").arg(SCORE_VERSION_EXTRA);
    }
    version_text += QStringLiteral(" “%5”\n")
        .arg(SCORE_CODENAME);

    QString commit{GIT_COMMIT};

    if (!commit.isEmpty())
    {
      version_text += tr("Commit: %1\n").arg(commit);
    }
    painter.setPen(textPen);
    painter.setFont(m_catamaranFont);
    painter.drawText(
        QRectF(0, 100, m_windowSize.width(), 60),
        Qt::AlignHCenter,
        version_text);
  }

  // write copyright
  {
    auto copyright_text = QString(
        "Copyright © Ossia " + QString::number(QDate::currentDate().year()));

    painter.setFont(m_montserratFont);
    painter.drawText(
        QRectF(0, 160, m_windowSize.width(), 30),
        Qt::AlignHCenter,
        copyright_text);
  }

  // write title above listview
  painter.setPen(titleText);
  QFont mb = m_montserratFont;
  mb.setBold(true);
  painter.setFont(mb);
  painter.drawText(QRectF(145, 210, 120, 15), Qt::AlignHCenter, "Project");

  // write title above license
  painter.drawText(QRectF(280, 210, 185, 15), Qt::AlignHCenter, "License");
}
}

#include "AboutDialog.hpp"
#include <QDate>
#include <QEvent>
#include <QDebug>
#include <QSize>
#include <QPicture>
#include <QFont>
#include <QPainter>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QDesktopServices>
#include <map>
#include <score_git_info.hpp>

namespace score {
AboutDialog::AboutDialog(QWidget *parent) :
  QDialog(parent),
  m_windowSize(492,437),
  m_backgroundImage(":/about/about_background.png"),
  m_catamaranFont(":/Catamaran-Regular.ttf", 12),
  m_montserratFont(":/Montserrat-Regular.ttf", 11),
  m_mouseAreaLabri(17, 221, 126, 62),
  m_mouseAreaBlueYeti(20, 287, 110, 29),
  m_mouseAreaScrime(22, 320, 100, 35)
{
  setWindowFlag(Qt::FramelessWindowHint);
  resize(m_windowSize.width(),m_windowSize.height());
  setMouseTracking(true);

  // map
  std::map<QString, QString> map;
  map["Qt"] = "GNU General Public License v3";
  map["Boost"] = "Boost License";


  // software list
  auto softwareList = new QListWidget{this};
  softwareList->move(145,230);
  softwareList->resize(120,183);
  softwareList->setFont(m_catamaranFont);
  softwareList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  softwareList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  softwareList->setStyleSheet(R"_(
                              QListView::item:!selected:hover{
                              background-color:#415491;
                              color: #ffffff;
                              }
                              QListView::item:selected:active, QListView::item:selected:!active{
                              background-color:#73C1C6;
                              color: #1A2024;
                              }
                              QListView {
                              background-color: rgb(18,23,26);
                              margin:0px;
                              }
                              /* QListView::item{
                              color: rgb(0,146,207);
                              }*/
                              QScrollBar::right-arrow:horizontal, QScrollBar::left-arrow:horizontal
                              {
                              border: none;
                              background: none;
                              color: none;
                              }
                              QScrollBar::add-line:vertical {
                              border: none;
                              background: none;
                              }
                              QScrollBar::sub-line:vertical {
                              border: none;
                              background: none;
                              }
                              )_");
  for(auto item : map)
  {
    softwareList->addItem(item.first);
  }


  // license
  auto license = new QPlainTextEdit{this};
  license->move(280,230);
  license->resize(185,183);
  license->setFont(m_catamaranFont);
  license->setReadOnly(true);
  connect(softwareList,&QListWidget::currentTextChanged,this,
          [=] (const QString& currentText)
  {
    license->setPlainText(map.at(currentText));
  });
}

void AboutDialog::mousePressEvent(QMouseEvent *event)
{

  QPointF pos = event->localPos();
  if(m_mouseAreaLabri.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://www.labri.fr/"));
  }
  else if(m_mouseAreaBlueYeti.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("http://www.blueyeti.fr/"));
  }
  else if(m_mouseAreaScrime.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://scrime.u-bordeaux.fr/"));
  }
  this->close();
}

void AboutDialog::mouseMoveEvent(QMouseEvent *event)
{
  QPointF pos = event->localPos();
  if( m_mouseAreaLabri.contains(pos)
      || m_mouseAreaBlueYeti.contains(pos)
      || m_mouseAreaScrime.contains(pos))
  {
    this->setCursor(Qt::PointingHandCursor);
  }
  else
  {
    this->setCursor(Qt::ArrowCursor);
  }
}
void AboutDialog::paintEvent(QPaintEvent *event)
{
  QPen textPen(QColor("#0092cf"));
  QPen titleText(QColor("#aaaaaa"));
  QPen rectPen(QColor("#03c3dd"));
  QBrush rectBrush(QColor(18,23,26));

  auto version_text = QStringLiteral("Version: %1.%2.%3-%4 '%5'\n")
                      .arg(SCORE_VERSION_MAJOR)
                      .arg(SCORE_VERSION_MINOR)
                      .arg(SCORE_VERSION_PATCH)
                      .arg(SCORE_VERSION_EXTRA)
                      .arg(SCORE_CODENAME);

  QString commit{GIT_COMMIT};

  if (!commit.isEmpty())
  {
    version_text += tr("Commit: %1\n").arg(commit);
  }

  auto copyright_text = QString("Copyright © Ossia "+ QString::number(QDate::currentDate().year()));

  // draw background image
  QPainter painter(this);
  painter.drawImage(QPoint(0,0), m_backgroundImage);

  // write version and commit
  painter.setPen(textPen);
  painter.setFont(m_catamaranFont);
  painter.drawText(QRectF(0,120,m_windowSize.width(),40),
                   Qt::AlignHCenter,
                   version_text);

  // write copyright
  painter.setFont(m_montserratFont);
  painter.drawText(QRectF(0,160,m_windowSize.width(),30),
                   Qt::AlignHCenter,
                   copyright_text);

  // write title above listview
  m_montserratFont.setPointSize(13);
  painter.setPen(titleText);
  painter.setFont(m_montserratFont);
  painter.drawText(QRectF(145,210,120,15),
                   Qt::AlignHCenter,
                   "Project");

  // write title above license
  painter.drawText(QRectF(280,210,120,15),
                   Qt::AlignHCenter,
                   "License");
}


}

#pragma once
#include <QDialog>

namespace score {
class AboutDialog : public QDialog
{
  public:
    AboutDialog(QWidget *parent = 0);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

  private:
    QSize m_windowSize;

    QImage m_backgroundImage;
    QFont m_catamaranFont;
    QFont m_montserratFont;

    QRectF m_mouseAreaLabri;
    QRectF m_mouseAreaBlueYeti;
    QRectF m_mouseAreaScrime;

};

}

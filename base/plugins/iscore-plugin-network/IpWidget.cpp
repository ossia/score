#include "IpWidget.hpp"
IpWidget::IpWidget(QWidget *parent) : QFrame(parent)
{
    setFrameShape( QFrame::StyledPanel );
    setFrameShadow( QFrame::Sunken );

    QHBoxLayout* pLayout = new QHBoxLayout( this );
    setLayout( pLayout );
    pLayout->setContentsMargins( 0, 0, 0, 0 );
    pLayout->setSpacing( 0 );

    for ( int i = 0; i != QTUTL_IP_SIZE; ++i )
    {
        if ( i != 0 )
        {
            QLabel* pDot = new QLabel( ".", this );
            pDot->setStyleSheet( "background: white" );
            pLayout->addWidget( pDot );
            pLayout->setStretch( pLayout->count(), 0 );
        }

        lineEdits[i] = new QLineEdit( this );
        QLineEdit* pEdit = lineEdits[i];
        pEdit->installEventFilter( this );

        pLayout->addWidget( pEdit );
        pLayout->setStretch( pLayout->count(), 1 );

        pEdit->setFrame( false );
        pEdit->setAlignment( Qt::AlignCenter );

        QFont font = pEdit->font();
        font.setStyleHint( QFont::Monospace );
        font.setFixedPitch( true );
        pEdit->setFont( font );

        QRegExp rx ( "^(0|[1-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$" );
        QValidator *validator = new QRegExpValidator(rx, pEdit);
        pEdit->setValidator( validator );

    }

    lineEdits[0]->setText("127");
    lineEdits[1]->setText("0");
    lineEdits[2]->setText("0");
    lineEdits[3]->setText("1");

    setMaximumWidth( 30 * QTUTL_IP_SIZE );

    connect( this, SIGNAL(signalTextChanged(QLineEdit*)),
             this, SLOT(slotTextChanged(QLineEdit*)),
             Qt::QueuedConnection );
}

IpWidget::~IpWidget()
{

}

void IpWidget::slotTextChanged( QLineEdit* pEdit )
{
    for ( unsigned int i = 0; i != QTUTL_IP_SIZE; ++i )
    {
        if ( pEdit == lineEdits[i] )
        {
            if ( ( pEdit->text().size() == MAX_DIGITS &&  pEdit->text().size() == pEdit->cursorPosition() ) || ( pEdit->text() == "0") )
            {
                // auto-move to next item
                if ( i+1 != QTUTL_IP_SIZE )
                {
                    lineEdits[i+1]->setFocus();
                    lineEdits[i+1]->selectAll();
                }
            }
        }
    }
}

bool IpWidget::eventFilter(QObject *obj, QEvent *event)
{
    bool bRes = QFrame::eventFilter(obj, event);

    if ( event->type() == QEvent::KeyPress )
    {
        QKeyEvent* pEvent = dynamic_cast<QKeyEvent*>( event );
        if ( pEvent )
        {
            for ( unsigned int i = 0; i != QTUTL_IP_SIZE; ++i )
            {
                QLineEdit* pEdit = lineEdits[i];
                if ( pEdit == obj )
                {
                    switch ( pEvent->key() )
                    {
                        case Qt::Key_Left:
                            if ( pEdit->cursorPosition() == 0 )
                            {
                                // user wants to move to previous item
                                MovePrevLineEdit(i);
                            }
                            break;

                        case Qt::Key_Right:
                            if ( pEdit->text().isEmpty() || (pEdit->text().size() == pEdit->cursorPosition()) )
                            {
                                // user wants to move to next item
                                MoveNextLineEdit(i);
                            }
                            break;

                        case Qt::Key_0:
                            if ( pEdit->text().isEmpty() || pEdit->text() == "0" )
                            {
                                pEdit->setText("0");
                                // user wants to move to next item
                                MoveNextLineEdit(i);
                            }
                            emit signalTextChanged( pEdit );
                            break;

                        case Qt::Key_Backspace:
                            if ( pEdit->text().isEmpty() || pEdit->cursorPosition() == 0)
                            {
                                // user wants to move to previous item
                                MovePrevLineEdit(i);
                            }
                            break;

                        case Qt::Key_Comma:
                        case Qt::Key_Period:
                            MoveNextLineEdit(i);
                            break;

                        default:
                            emit signalTextChanged( pEdit );
                            break;

                    }
                }
            }
        }
    }

    return bRes;
}

void IpWidget::MoveNextLineEdit(int i)
{
    if ( i+1 != QTUTL_IP_SIZE )
    {
        lineEdits[i+1]->setFocus();
        lineEdits[i+1]->setCursorPosition( 0 );
        lineEdits[i+1]->selectAll();
    }
}

void IpWidget::MovePrevLineEdit(int i)
{
    if ( i != 0 )
    {
        lineEdits[i-1]->setFocus();
        lineEdits[i-1]->setCursorPosition( lineEdits[i-1]->text().size() );
        //m_pLineEdit[i-1]->selectAll();
    }
}

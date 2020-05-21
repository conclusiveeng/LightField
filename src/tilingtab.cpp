#include <QtCore>
#include <QtWidgets>
#include "progressdialog.h"
#include "utils.h"
#include "tilingtab.h"
#include "tilingmanager.h"
#include "window.h"


TilingExpoTimePopup::TilingExpoTimePopup( ) {
    auto origFont    = font( );
    auto normalFont = ModifyFont( origFont, "FontAwesome", NormalFontSize );
    auto fontAwesome = ModifyFont( origFont, "FontAwesome", LargeFontSize );
    auto font22pt    = ModifyFont( origFont, LargeFontSize );

    this->setModal(true);
    this->_okButton->setFont(font22pt);
    this->_cancelButton->setFont(font22pt);

    QGroupBox* baseLr = new QGroupBox("Base layer");
    baseLr->setLayout(
        WrapWidgetsInVBox(
            _minExposureBase,
            _stepBase
        )
    );

    QGroupBox* bodyLr = new QGroupBox("Body layer");
    bodyLr->setLayout(
        WrapWidgetsInVBox(
            _minExposureBody,
            _stepBody
        )
    );

    QObject::connect( _okButton, &QPushButton::clicked, this, &TilingExpoTimePopup::confirm );
    QObject::connect( _cancelButton, &QPushButton::clicked, this, &TilingExpoTimePopup::cancel );

    setLayout(
        WrapWidgetsInVBox(
            baseLr,
            bodyLr,
            nullptr,
            WrapWidgetsInHBox( _okButton, _cancelButton )
        )
    );
}

void TilingExpoTimePopup::confirm( bool ) {
    this->setResult( QDialog::Accepted );
    this->accept( );
    this->close( );
}

void TilingExpoTimePopup::cancel( bool ) {
    this->setResult( QDialog::Rejected );
    this->reject( );
    this->close( );
}

TilingTab::TilingTab( QWidget* parent ): TabBase( parent )
{
    auto origFont    = font( );
    auto boldFont    = ModifyFont( origFont, QFont::Bold );
    auto fontAwesome = ModifyFont( origFont, "FontAwesome", LargeFontSize );
    auto font22pt    = ModifyFont( origFont, LargeFontSize );

    QGroupBox* all { new QGroupBox };

    QVBoxLayout* _currentLayerLayout;
    _currentLayerImage->setAlignment( Qt::AlignCenter );
    _currentLayerImage->setContentsMargins( { } );
    _currentLayerImage->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    _currentLayerImage->setStyleSheet( "QWidget { background: black }" );
    _currentLayerImage->setMinimumSize( MaximalRightHandPaneSize );
    _currentLayerImage->setFixedSize( _currentLayerImage->width( ), _currentLayerImage->width( ) / AspectRatio16to10 + 0.5 );


    _setupTiling->setEnabled( false );
    _setupTiling->setMinimumWidth( MainButtonSize.width() );
    _setupTiling->setFont( font22pt );
    _setupTiling->setText( "Setup tiling" );
    QObject::connect( _setupTiling, &QPushButton::clicked, this, &TilingTab::setupTilingClicked );

    _currentLayerLayout = WrapWidgetsInVBox(
        _currentLayerImage
    );
    _currentLayerLayout->setAlignment( Qt::AlignTop | Qt::AlignHCenter );


    _confirm->setFont( fontAwesome );
    _confirm->setMinimumWidth( MainButtonSize.width() );
    _confirm->setEnabled( false );

    _setupExpoTimeBt->setFont(font22pt);
    _setupExpoTimeBt->setMinimumWidth( MainButtonSize.width() );

    QGroupBox* lrInfo     = new QGroupBox();
    QGroupBox* baseLrInfo = new QGroupBox("Base layer");
    QGroupBox* bodyLrInfo = new QGroupBox("Body layer");

    baseLrInfo->setLayout(
        WrapWidgetsInVBox(
            WrapWidgetsInHBox(_minExposureBaseLabel, nullptr, _minExposureBaseValue),
            WrapWidgetsInHBox(_stepBaseLabel, nullptr, _stepBaseValue)
        )
    );

    bodyLrInfo->setLayout(
        WrapWidgetsInVBox(
            WrapWidgetsInHBox(_minExposureBodyLabel, nullptr, _minExposureBodyValue),
            WrapWidgetsInHBox(_stepBodyLabel, nullptr, _stepBodyValue)
        )
    );

    lrInfo->setLayout(
        WrapWidgetsInVBox( baseLrInfo, nullptr, bodyLrInfo )
    );


    all->setLayout(
        WrapWidgetsInVBox(
           _setupTiling,
           nullptr,
           lrInfo,
           nullptr,
           _space,
           _count,
           nullptr,
           _setupExpoTimeBt,
           _confirm
        )
    );

    QObject::connect( _space, &ParamSlider::valueChanged, this, &TilingTab::setStepValue );
    QObject::connect( _count, &ParamSlider::valueChanged, this, &TilingTab::setStepValue );


    QObject::connect( _setupExpoTimeBt, &QPushButton::clicked, this, &TilingTab::setupExpoTimeClicked);

    QObject::connect( _confirm, &QPushButton::clicked, this, &TilingTab::confirmButton_clicked );


    all->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Expanding );

    setLayout( WrapWidgetsInHBox( all, _currentLayerImage ) );

    _setEnabled( false );
    update( );
}

void TilingTab::setStepValue()
{
    debug( "+ TilingTab::setStepValue\n");

    auto area = QPixmap( _currentLayerImage->width( ), _currentLayerImage->height( ) );
    int maxCount = _getMaxCount( );
    debug( "+ TilingTab::setCount %d\n", maxCount);
    _count->setMaxValue( maxCount );

    int wCount = _count->getValue();
    int spacePx = ( (double)_space->getValue() ) / ProjectorPixelSize * _wRatio;

    if( maxCount < 1 ) {
        _showWarningAndClose();
        _space->setEnabled( false );

        return;
    } else {
        _space->setEnabled( true );
    }

    //int hCount =  floor( (_currentLayerImage->height( ) - pixmap.height() * value)  / (pixmap.height() + pixmap.height() * value) );

    QPainter painter ( &area );

    painter.fillRect(0,0, _currentLayerImage->width( ), _currentLayerImage->height( ), QBrush("#000000"));

    painter.setFont( QFont( "Arial", 15 ) );
    painter.setPen( Qt::red );

    // multi row tilling
    /*for(int i=0,z=1; i<wCount; ++i) {
        for(int j=0; j<hCount; ++j,++z)
        {
                            /*margin*/                /* image */                 /* space */
    /*      int x = ( pixmap.width( ) * value) + ( pixmap.width( ) * i ) + ( pixmap.width( ) * value * i );
            int y = ( pixmap.height( ) * value) + ( pixmap.height( ) * j )  + ( pixmap.height( ) * value * j );

            int e = _minExposure->getValue() + ( ((wCount*hCount) - z) * _step->getValue() );

            painter.drawPixmap( x, y, pixmap );
            painter.drawText( QPoint(x, y), QString( "Exposure %1 sec" ).arg( e ) );
        }
    }*/

    // single row tiling
    int y = ( _areaHeight - _pixmapHeight ) / 2;

    // 3 mm Y offset for first element
    int deltaY = ( 3 / ProjectorPixelSize ) * _hRatio;
    std::vector<int> tileSlots;

    int deltax = (_areaWidth - (wCount*_pixmapWidth) - (wCount -1)*spacePx)/2 - TilingMargin;

    for(int i=0; i<wCount; ++i) {
        int x1 = TilingMargin + ( _pixmapWidth * i ) + ( spacePx * i );
        tileSlots.push_back(x1 + deltax);
    }

    //std::reverse(tileSlots.begin(), tileSlots.end());
    std::rotate(tileSlots.begin(),
                tileSlots.end()-1, // this will be the new first element
                tileSlots.end());

    for(int i=0; i<wCount; ++i) {
        int x = tileSlots[i];

        double minExposureBase = _minExposureBase;
        double stepBase = _stepBase;
        double minExposureBody = _minExposureBody;
        double stepBody = _stepBody;

        double eBase = minExposureBase + ( ( wCount - ( i + 1 ) ) * stepBase );
        double eBody = minExposureBody + ( ( wCount - ( i + 1 ) ) * stepBody );

        if(i==0) {
            painter.drawPixmap( x, y - deltaY, *_pixmap );
             _renderText( &painter, _pixmapWidth, QPoint(x, y - deltaY ), eBase, eBody );
        }
        else
        {
            painter.drawPixmap( x, y, *_pixmap );
             _renderText( &painter, _pixmapWidth, QPoint(x, y), eBase, eBody  );
        }
    }

    _currentLayerImage->setPixmap( area );

    update( );
}

void TilingTab::_renderText(QPainter* painter, int tileWidth, QPoint pos, double expoBase, double expoBody)
{
    QFontMetrics fm( painter->font() );
    QString text = QString( "Exposure %1/%2 sec" ).arg( expoBase ).arg(expoBody);

    int textWidth=fm.horizontalAdvance(text);

    if(textWidth > tileWidth)
    {
        int textHeight = fm.height();

        text = QString( "Expo." );
        QString text2 = QString( "%1/%2 s" ).arg(expoBase).arg(expoBody);

        painter->drawText( QPoint(pos.x(), pos.y() - textHeight - 2), text );
        painter->drawText( pos, text2 );
    }
    else
    {
        painter->drawText( pos, text );
    }
}

void TilingTab::_showLayerImage( ) {
    debug( "+ TilingTab::_showLayerImage");

    setStepValue ();

    update( );
}



void TilingTab::tab_uiStateChanged( TabIndex const sender, UiState const state ) {
    debug( "+ TilingTab::tab_uiStateChanged: from %sTab: %s => %s\n", ToString( sender ), ToString( _uiState ), ToString( state ) );
    _uiState = state;

    switch (state) {
    case UiState::SelectedDirectory:
        this->_stepBase = 2.0;
        this->_stepBody = 2.0;
        this->_minExposureBase = 10.0;
        this->_minExposureBody = 20.0;
        this->_space->setValue( 1 );
        this->_currentLayerImage->clear();
        _setEnabled( false );
        _setupTiling->setEnabled( false );
        break;

    case UiState::SelectCompleted:
        _setEnabled( false );
        _setupTiling->setEnabled( false );
        break;

    case UiState::SelectStarted:
        _setEnabled( false );
        break;

    case UiState::SliceStarted:
        _setEnabled( false );
        break;

    case UiState::PrintStarted:
        _setEnabled( false );
        break;

    case UiState::SliceCompleted:
    case UiState::PrintCompleted:
        break;
    case UiState::TilingClicked:
        _setupTiling->setEnabled( false );
        _setEnabled( true );
        break;
    case UiState::EnableTiling:
        _setupTiling->setEnabled( true );
        _setEnabled( false );
        break;

    case UiState::DisableTiling:
        _setupTiling->setEnabled( false );
        _setEnabled( false );
        break;
    }

    update( );
}


void TilingTab::confirmButton_clicked (bool)
{
    debug( "+ TilingTab::confirmButton_clicked\n" );

    TilingManager* tilingMgr = new  TilingManager( printJob() );
    ProgressDialog* dialog = new ProgressDialog(this);

    QObject::connect(tilingMgr, &TilingManager::statusUpdate, dialog, &ProgressDialog::setMessage);
    QObject::connect(tilingMgr, &TilingManager::progressUpdate, dialog, &ProgressDialog::setProgress);

    dialog->show();

    QThread *thread = QThread::create(
        [this, tilingMgr, dialog]
        {
            tilingMgr->processImages( ProjectorWindowSize.width(),
                                      ProjectorWindowSize.height(),
                                     _minExposureBase,
                                     _stepBase,
                                     _minExposureBody,
                                     _stepBody,
                                     _space->getValue(),
                                     _count->getValue() );

            printJob()->directoryMode = true;
            printJob()->directoryPath = tilingMgr->getPath();

            dialog->close();
            delete dialog;
            delete tilingMgr;

        }
    );

    thread->start();

    dialog->exec();
    emit uiStateChanged( TabIndex::Tiling, UiState::SelectedDirectory );
}

int TilingTab::_getMaxCount()
{
    int wCount=0;
    int space = ((double)_space->getValue()) / ProjectorPixelSize * _wRatio;

    for (
         int i = ( TilingMargin * _wRatio );
         i < ( _areaWidth - ( TilingMargin * _wRatio ) );
         i += _pixmapWidth, wCount++
    ) {
        if(wCount>0)
            i+=space;

        debug( " i: %d wCount: %d \n", i, wCount );
    }

    wCount--;

    return wCount;
}

void TilingTab::_showWarningAndClose ()
{
    auto origFont    = font( );
    auto fontAwesome = ModifyFont( origFont, "FontAwesome" );

    Window* w = App::mainWindow();
    QRect r = w->geometry();

    QMessageBox msgBox;
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setFont(fontAwesome);
    msgBox.setText( "Slices are too wide to be tiled." );
    msgBox.show();
    msgBox.move(r.x() + ((r.width() - msgBox.width())/2), r.y() + ((r.height() - msgBox.height())/2) );
    msgBox.exec();
}

void TilingTab::_setEnabled(bool enabled)
{
    this->_confirm->setEnabled( enabled );
    this->_space->setEnabled( enabled );
    this->_setupExpoTimeBt->setEnabled( enabled );
    this->_count->setEnabled( enabled );

    this->_currentLayerImage->clear();
}

void TilingTab::setupExpoTimeClicked(bool) {
    debug( "+ TilingTab::setupExpoTimeClicked\n" );
    _expoTimePopup.setMinExposureBase( _minExposureBase );
    _expoTimePopup.setStepBase( _stepBase );
    _expoTimePopup.setMinExposureBody( _minExposureBody );
    _expoTimePopup.setStepBody( _stepBody );

    if(_expoTimePopup.exec() == QDialog::Accepted) {
        _minExposureBase = _expoTimePopup.minExposureBase();
        _stepBase = _expoTimePopup.stepBase();
        _minExposureBody = _expoTimePopup.minExposureBody();
        _stepBody = _expoTimePopup.stepBody();

        _minExposureBaseValue->setText ( QString("%1s").arg( _expoTimePopup.minExposureBase() ) );
        _stepBaseValue->setText ( QString("%1s").arg( _expoTimePopup.stepBase() ) );
        _minExposureBodyValue->setText ( QString("%1s").arg( _expoTimePopup.minExposureBody() ) );
        _stepBodyValue->setText ( QString("%1s").arg( _expoTimePopup.stepBody() ) );
    }

    setStepValue();
}

void TilingTab::setupTilingClicked ( bool ) {
    debug( "+ TilingTab::setupTilingClicked\n" );

    this->_areaWidth = _currentLayerImage->width( );
    this->_areaHeight = _currentLayerImage->height( );
    this->_wRatio = ((double)_areaWidth) /  ProjectorWindowSize.width();
    this->_hRatio = ((double)_areaHeight) /  ProjectorWindowSize.height();
    QPixmap pixmap ( printJob()->getLayerDirectory(0) % Slash % printJob()->getLayerFileName(0) );

    if( this->_pixmap )
        delete this->_pixmap;

    this->_pixmap = new QPixmap ( pixmap.scaled( pixmap.width( ) * _wRatio, pixmap.height( ) * _hRatio) );

    this->_pixmapWidth = this->_pixmap->width( );
    this->_pixmapHeight = this->_pixmap->height( );

    if(_getMaxCount() < 1) {
            _showWarningAndClose();
            return;
    }

    _setEnabled( true );

    _showLayerImage();

    emit uiStateChanged( TabIndex::Prepare, UiState::TilingClicked );
}

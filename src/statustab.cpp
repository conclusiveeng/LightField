#include "pch.h"

#include "statustab.h"

#include "printjob.h"
#include "printmanager.h"
#include "shepherd.h"
#include "strings.h"
#include "utils.h"

StatusTab::StatusTab( QWidget* parent ): QWidget( parent ) {
    _initialShowEventFunc = std::bind( &StatusTab::_initialShowEvent, this );

    auto boldFont = ModifyFont( font( ), font( ).pointSizeF( ), QFont::Bold );

    printerStateLabel->setText( "Printer state:" );
    printerStateLabel->setBuddy( printerStateDisplay );

    printerStateDisplay->setText( "offline" );
    printerStateDisplay->setFont( boldFont );

    projectorLampStateLabel->setText( "Projector state:" );
    projectorLampStateLabel->setBuddy( projectorLampStateDisplay );

    projectorLampStateDisplay->setText( "off" );
    projectorLampStateDisplay->setFont( boldFont );

    jobStateLabel->setText( "Print job:" );
    jobStateLabel->setBuddy( jobStateDisplay );

    jobStateDisplay->setText( "not printing" );
    jobStateDisplay->setFont( boldFont );

    currentLayerLabel->setText( "Current layer:" );
    currentLayerLabel->setBuddy( currentLayerDisplay );

    currentLayerDisplay->setFont( boldFont );

    elapsedTimeLabel->setText( "Elapsed time:" );
    elapsedTimeLabel->setBuddy( elapsedTimeDisplay );

    elapsedTimeDisplay->setFont( boldFont );

    estimatedTimeLeftLabel->setText( "Estimated time left:" );
    estimatedTimeLeftLabel->setBuddy( elapsedTimeDisplay );

    estimatedTimeLeftDisplay->setFont( boldFont );

    percentageCompleteLabel->setText( "Percentage completed:" );
    percentageCompleteLabel->setBuddy( elapsedTimeDisplay );

    percentageCompleteDisplay->setFont( boldFont );

    progressControlsLayout->setContentsMargins( { } );
    progressControlsLayout->addLayout( WrapWidgetsInHBox( { printerStateLabel,       nullptr, printerStateDisplay       } ) );
    progressControlsLayout->addLayout( WrapWidgetsInHBox( { projectorLampStateLabel, nullptr, projectorLampStateDisplay } ) );
    progressControlsLayout->addLayout( WrapWidgetsInHBox( { jobStateLabel,           nullptr, jobStateDisplay           } ) );
    progressControlsLayout->addLayout( WrapWidgetsInHBox( { currentLayerLabel,       nullptr, currentLayerDisplay       } ) );
    progressControlsLayout->addLayout( WrapWidgetsInHBox( { elapsedTimeLabel,        nullptr, elapsedTimeDisplay        } ) );
    progressControlsLayout->addLayout( WrapWidgetsInHBox( { estimatedTimeLeftLabel,  nullptr, estimatedTimeLeftDisplay  } ) );
    progressControlsLayout->addLayout( WrapWidgetsInHBox( { percentageCompleteLabel, nullptr, percentageCompleteDisplay } ) );
    progressControlsLayout->addStretch( );

    progressControlsContainer->setContentsMargins( { } );
    progressControlsContainer->setLayout( progressControlsLayout );
    progressControlsContainer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    currentLayerImage->setAlignment( Qt::AlignCenter );
    currentLayerImage->setContentsMargins( { } );
    currentLayerImage->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    currentLayerImage->setStyleSheet( QString( "QWidget { background: black }" ) );

    currentLayerLayout->setAlignment( Qt::AlignCenter );
    currentLayerLayout->setContentsMargins( { } );
    currentLayerLayout->addWidget( currentLayerImage );

    currentLayerImageGroup->setTitle( "Current layer" );
    currentLayerImageGroup->setContentsMargins( { } );
    currentLayerImageGroup->setMinimumSize( MaximalRightHandPaneSize );
    currentLayerImageGroup->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    currentLayerImageGroup->setLayout( WrapWidgetInVBox( currentLayerImage ) );

    {
        _stopButtonEnabledPalette  =  stopButton->palette( );
        _stopButtonDisabledPalette = _stopButtonEnabledPalette;
        _stopButtonEnabledPalette.setColor( QPalette::Button,     Qt::red    );
        _stopButtonEnabledPalette.setColor( QPalette::ButtonText, Qt::yellow );
    }
    stopButton->setEnabled( false );
    stopButton->setFixedSize( MainButtonSize );
    stopButton->setFont( ModifyFont( stopButton->font( ), 22.25f, QFont::Bold ) );
    stopButton->setText( "STOP" );
    QObject::connect( stopButton, &QPushButton::clicked, this, &StatusTab::stopButton_clicked );

    _layout->setContentsMargins( { } );
    _layout->addWidget( progressControlsContainer,  0, 0, 1, 1 );
    _layout->addWidget( stopButton,                 1, 0, 1, 1 );
    _layout->addWidget( currentLayerImageGroup,     0, 1, 2, 1 );
    _layout->setRowStretch( 0, 4 );
    _layout->setRowStretch( 1, 1 );

    _updatePrintTimeInfo = new QTimer( this );
    _updatePrintTimeInfo->setInterval( 1000 );
    _updatePrintTimeInfo->setSingleShot( false );
    _updatePrintTimeInfo->setTimerType( Qt::PreciseTimer );
    QObject::connect( _updatePrintTimeInfo, &QTimer::timeout, this, &StatusTab::updatePrintTimeInfo_timeout );

    setLayout( _layout );
}

StatusTab::~StatusTab( ) {
    /*empty*/
}

void StatusTab::showEvent( QShowEvent* event ) {
    if ( _initialShowEventFunc ) {
        _initialShowEventFunc( );
        _initialShowEventFunc = nullptr;
    }
    event->ignore( );
}

void StatusTab::_initialShowEvent( ) {
    currentLayerImage->setFixedWidth( currentLayerImage->width( ) );
    currentLayerImage->setFixedHeight( currentLayerImage->width( ) / AspectRatio16to10 + 0.5 );
}

void StatusTab::printer_online( ) {
    debug( "+ StatusTab::printer_online\n" );
    _isPrinterOnline = true;
    printerStateDisplay->setText( "online" );

    if ( !_isFirstOnlineTaskDone ) {
        debug( "+ StatusTab::printer_online: printer has come online for the first time; sending 'disable steppers' command\n" );
        QObject::connect( _shepherd, &Shepherd::action_sendComplete, this, &StatusTab::disableSteppers_sendComplete );
        _shepherd->doSend( QString( "M18" ) );
    }
}

void StatusTab::printer_offline( ) {
    debug( "+ StatusTab::printer_offline\n" );
    _isPrinterOnline = false;
    printerStateDisplay->setText( "offline" );
}

void StatusTab::stopButton_clicked( bool ) {
    debug( "+ StatusTab::stopButton_clicked\n" );
    _updatePrintTimeInfo->stop( );
    emit stopButtonClicked( );
}

void StatusTab::printManager_printStarting( ) {
    debug( "+ StatusTab::printManager_printStarting\n" );
    jobStateDisplay->setText( "print started" );
    estimatedTimeLeftDisplay->setText( QString( "calculating..." ) );
}

void StatusTab::printManager_startingLayer( int const layer ) {
    debug( "+ StatusTab::printManager_startingLayer: layer %d/%d\n", layer + 1, _printJob->layerCount );
    currentLayerDisplay->setText( QString( "%1/%2" ).arg( layer + 1 ).arg( _printJob->layerCount ) );

    _printJobStartTime = GetBootTimeClock( );
    _updatePrintTimeInfo->start( );

    auto pixmap = QPixmap( _printJob->jobWorkingDirectory + QString( "/%2.png" ).arg( layer, 6, 10, DigitZero ) );
    if ( ( pixmap.width( ) > currentLayerImage->width( ) ) || ( pixmap.height( ) > currentLayerImage->height( ) ) ) {
        pixmap = pixmap.scaled( currentLayerImage->size( ), Qt::KeepAspectRatio, Qt::SmoothTransformation );
    }
    currentLayerImage->setPixmap( pixmap );
}

void StatusTab::printManager_lampStatusChange( bool const on ) {
    debug( "+ StatusTab::printManager_lampStatusChange: lamp %s\n", on ? "ON" : "off" );
    projectorLampStateDisplay->setText( QString( on ? "ON" : "off" ) );
}

void StatusTab::printManager_printComplete( bool const success ) {
    debug( "+ StatusTab::printManager_printComplete: %s\n", success ? "print complete" : "print failed" );
    jobStateDisplay->setText( QString( success ? "print complete" : "print failed" ) );
    _updatePrintTimeInfo->stop( );
    emit printComplete( );
}

void StatusTab::printManager_printAborted( ) {
    debug( "+ StatusTab::printManager_printAborted\n" );
    jobStateDisplay->setText( QString( "print aborted" ) );
    _updatePrintTimeInfo->stop( );
    emit printComplete( );
}

void StatusTab::disableSteppers_sendComplete( bool const success ) {
    debug( "+ StatusTab::disableSteppers_sendComplete: success %s\n", ToString( success ) );
    QObject::disconnect( _shepherd, &Shepherd::action_sendComplete, this, &StatusTab::disableSteppers_sendComplete );

    if ( success ) {
        debug( "+ StatusTab::disableSteppers_sendComplete: sending 'set fan speed' command\n" );
        QObject::connect( _shepherd, &Shepherd::action_sendComplete, this, &StatusTab::setFanSpeed_sendComplete );
        _shepherd->doSend( QString( "M106 S220" ) );
    }
}

void StatusTab::setFanSpeed_sendComplete( bool const success ) {
    debug( "+ StatusTab::setFanSpeed_sendComplete: success %s\n", ToString( success ) );
    QObject::disconnect( _shepherd, &Shepherd::action_sendComplete, this, &StatusTab::setFanSpeed_sendComplete );

    if ( success ) {
        debug( "+ StatusTab::setFanSpeed_sendComplete: first-online tasks completed\n" );
        _isFirstOnlineTaskDone = true;
    }
}

void StatusTab::updatePrintTimeInfo_timeout( ) {
    if ( !_printManager ) {
        debug( "+ StatusTab::updatePrintTimeInfo_timeout: no print manager. don't know why timer is even running, but stopping it.\n" );
        _updatePrintTimeInfo->stop( );
        return;
    }

    percentageCompleteDisplay->setText( QString( "%1%" ).arg( static_cast<int>( _printManager->currentLayer( ) / _printJob->layerCount + 0.5 ) ) );

    double delta = GetBootTimeClock( ) - _printJobStartTime;
    debug( "+ StatusTab::updatePrintTimeInfo_timeout: delta %f\n" );
    elapsedTimeDisplay->setText( TimeDeltaToString( delta ) );

    auto currentLayer = _printManager->currentLayer( );
    if ( currentLayer < 4 ) {
        return;
    }

    double estimatedTime = delta / ( _printManager->currentLayer( ) / _printJob->layerCount );
    debug( "+ StatusTab::updatePrintTimeInfo_timeout: estimated time %f\n", delta );
    estimatedTimeLeftDisplay->setText( TimeDeltaToString( estimatedTime ) );
}

void StatusTab::setPrintJob( PrintJob* printJob ) {
    debug( "+ StatusTab::setPrintJob: printJob %p\n", printJob );
    _printJob = printJob;
}

void StatusTab::setPrintManager( PrintManager* printManager ) {
    debug( "+ StatusTab::setPrintManager: printManager %p\n", printManager );
    if ( _printManager ) {
        QObject::disconnect( _printManager, nullptr, this, nullptr );
    }

    _printManager = printManager;

    if ( _printManager ) {
        QObject::connect( _printManager, &PrintManager::printStarting,    this, &StatusTab::printManager_printStarting    );
        QObject::connect( _printManager, &PrintManager::printComplete,    this, &StatusTab::printManager_printComplete    );
        QObject::connect( _printManager, &PrintManager::printAborted,     this, &StatusTab::printManager_printAborted     );
        QObject::connect( _printManager, &PrintManager::startingLayer,    this, &StatusTab::printManager_startingLayer    );
        QObject::connect( _printManager, &PrintManager::lampStatusChange, this, &StatusTab::printManager_lampStatusChange );
    }
}

void StatusTab::setShepherd( Shepherd* newShepherd ) {
    if ( _shepherd ) {
        QObject::disconnect( _shepherd, nullptr, this, nullptr );
    }

    _shepherd = newShepherd;

    QObject::connect( _shepherd, &Shepherd::printer_online,  this, &StatusTab::printer_online  );
    QObject::connect( _shepherd, &Shepherd::printer_offline, this, &StatusTab::printer_offline );
}

void StatusTab::setStopButtonEnabled( bool value ) {
    stopButton->setEnabled( value );
    stopButton->setPalette( value ? _stopButtonEnabledPalette : _stopButtonDisabledPalette );
}

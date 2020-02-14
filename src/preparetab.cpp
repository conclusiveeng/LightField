#include "pch.h"

#include <sys/sysinfo.h>

#include "preparetab.h"

#include "hasher.h"
#include "printjob.h"
#include "printmanager.h"
#include "shepherd.h"
#include "svgrenderer.h"
#include "timinglogger.h"
#include "usbmountmanager.h"

#include "iostream"

PrepareTab::PrepareTab( QWidget* parent ): InitialShowEventMixin<PrepareTab, TabBase>( parent ) {
    auto origFont    = font( );
    auto boldFont    = ModifyFont( origFont, QFont::Bold );
    auto font12pt    = ModifyFont( origFont, 14.0 );
    auto font22pt    = ModifyFont( origFont, LargeFontSize );
    auto fontAwesome = ModifyFont( origFont, "FontAwesome" );

    _layerThicknessLabel->setEnabled( false );
    _layerThicknessLabel->setText( "Layer height:" );

    _layerThickness100Button->setEnabled( false );
    _layerThickness100Button->setChecked( true );
    _layerThickness100Button->setFont( font12pt );
    _layerThickness100Button->setText( "Standard res (100 µm)" );
    QObject::connect( _layerThickness100Button, &QPushButton::clicked, this, &PrepareTab::layerThickness100Button_clicked );

    _layerThickness50Button->setEnabled( false );
    _layerThickness50Button->setText( "High res (50 µm)" );
    _layerThickness50Button->setFont( font12pt );
    QObject::connect( _layerThickness50Button, &QPushButton::clicked, this, &PrepareTab::layerThickness50Button_clicked );

    _sliceStatusLabel->setText( "Slicer:" );

    _sliceStatus->setText( "idle" );
    _sliceStatus->setFont( boldFont );

    _imageGeneratorStatusLabel->setText( "Image generator:" );

    _imageGeneratorStatus->setText( "idle" );
    _imageGeneratorStatus->setFont( boldFont );

    _prepareMessage->setAlignment( Qt::AlignCenter );
    _prepareMessage->setTextFormat( Qt::RichText );
    _prepareMessage->setWordWrap( true );
    _prepareMessage->setText( "Tap the <b>Prepare</b> button to prepare the printer." );

    _prepareProgress->setAlignment( Qt::AlignCenter );
    _prepareProgress->setFormat( { } );
    _prepareProgress->setRange( 0, 0 );
    _prepareProgress->setTextVisible( false );
    _prepareProgress->hide( );

    _prepareGroup->setTitle( "Printer preparation" );
    _prepareGroup->setLayout( WrapWidgetsInVBox(
        nullptr,
        WrapWidgetsInHBox( _prepareMessage ),
        nullptr,
        WrapWidgetsInHBox( _prepareProgress ),
        nullptr
    ) );

    _warningHotImage = new QPixmap { QString { ":images/warning-hot.png" } };
    _warningHotLabel->setAlignment( Qt::AlignCenter );
    _warningHotLabel->setContentsMargins( { } );
    _warningHotLabel->setFixedSize( 43, 43 );
    _warningHotLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

    _warningUvImage = new QPixmap { QString { ":images/warning-uv.png"  } };
    _warningUvLabel->setAlignment( Qt::AlignCenter );
    _warningUvLabel->setContentsMargins( { } );
    _warningUvLabel->setFixedSize( 43, 43 );
    _warningUvLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

    _prepareButton->setEnabled( false );
    _prepareButton->setFixedSize( MainButtonSize );
    _prepareButton->setFont( font22pt );
    _prepareButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _prepareButton->setText( "Prepare" );
    QObject::connect( _prepareButton, &QPushButton::clicked, this, &PrepareTab::prepareButton_clicked );

    _copyToUSBButton->setEnabled( false );
    _copyToUSBButton->setFixedSize( MainButtonSize );
    _copyToUSBButton->setFont( font22pt );
    _copyToUSBButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _copyToUSBButton->setText( "Copy to USB" );
    QObject::connect( _copyToUSBButton, &QPushButton::clicked, this, &PrepareTab::copyToUSB_clicked );

    _optionsContainer->setFixedWidth( MainButtonSize.width( ) );
    _optionsContainer->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );
    _optionsContainer->setLayout( WrapWidgetsInVBox(
        _layerThicknessLabel,
        WrapWidgetsInVBox(
            _layerThickness100Button,
            _layerThickness50Button
        ),
        WrapWidgetsInHBox( _sliceStatusLabel,          nullptr, _sliceStatus          ),
        WrapWidgetsInHBox( _imageGeneratorStatusLabel, nullptr, _imageGeneratorStatus ),
        WrapWidgetsInVBox(
            _prepareGroup,
            WrapWidgetsInHBox( nullptr, _warningHotLabel, nullptr, _warningUvLabel, nullptr ),
            _prepareButton
        )
    ) );

    _sliceButton->setEnabled( false );
    _sliceButton->setFixedSize( MainButtonSize );
    _sliceButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _sliceButton->setFont( font22pt );
    _sliceButton->setText( "Slice" );
    QObject::connect( _sliceButton, &QPushButton::clicked, this, &PrepareTab::sliceButton_clicked );

    _currentLayerImage->setAlignment( Qt::AlignCenter );
    _currentLayerImage->setContentsMargins( { } );
    _currentLayerImage->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    _currentLayerImage->setStyleSheet( "QWidget { background: black }" );

    for ( auto button : { _navigateFirst, _navigatePrevious, _navigateNext, _navigateLast } ) {
        button->setFont( fontAwesome );
    }

    _navigateFirst   ->setText( FA_FastBackward );
    _navigatePrevious->setText( FA_Backward     );
    _navigateNext    ->setText( FA_Forward      );
    _navigateLast    ->setText( FA_FastForward  );

    QObject::connect( _navigateFirst,    &QPushButton::clicked, this, &PrepareTab::navigateFirst_clicked    );
    QObject::connect( _navigatePrevious, &QPushButton::clicked, this, &PrepareTab::navigatePrevious_clicked );
    QObject::connect( _navigateNext,     &QPushButton::clicked, this, &PrepareTab::navigateNext_clicked     );
    QObject::connect( _navigateLast,     &QPushButton::clicked, this, &PrepareTab::navigateLast_clicked     );


    _navigateCurrentLabel->setAlignment( Qt::AlignCenter );
    _navigateCurrentLabel->setText( "0/0" );

    _navigationLayout = WrapWidgetsInHBox( nullptr, _navigateFirst, _navigatePrevious, _navigateCurrentLabel, _navigateNext, _navigateLast, nullptr );
    _navigationLayout->setAlignment( Qt::AlignCenter );

    _setNavigationButtonsEnabled( false );

    _currentLayerLayout = WrapWidgetsInVBox(
        _currentLayerImage,
        _navigationLayout
    );
    _currentLayerLayout->setAlignment( Qt::AlignTop | Qt::AlignHCenter );

    _currentLayerGroup->setTitle( "Current layer" );
    _currentLayerGroup->setMinimumSize( MaximalRightHandPaneSize );
    _currentLayerGroup->setLayout( _currentLayerLayout );

    _layout->setContentsMargins( { } );
    _layout->addWidget( _optionsContainer,  0, 0, 1, 1 );
    _layout->addWidget( _sliceButton,       1, 0, 1, 1 );
    //_layout->addWidget( _copyToUSBButton,   2, 0, 1, 1 );
    _layout->addWidget( _currentLayerGroup, 0, 1, /* ^ 3*/ 2, 1 );
    _layout->setRowStretch( 0, 4 );
    _layout->setRowStretch( 1, 1 );

    setLayout( _layout );
}

PrepareTab::~PrepareTab( ) {
    /*empty*/
}

void PrepareTab::_connectPrintManager( ) {
    if ( _printManager ) {
        QObject::connect( _printManager, &PrintManager::lampStatusChange, this, &PrepareTab::printManager_lampStatusChange );
    }
}

void PrepareTab::_connectShepherd( ) {
    if ( _shepherd ) {
        QObject::connect( _shepherd, &Shepherd::printer_online,            this, &PrepareTab::printer_online            );
        QObject::connect( _shepherd, &Shepherd::printer_offline,           this, &PrepareTab::printer_offline           );
        QObject::connect( _shepherd, &Shepherd::printer_temperatureReport, this, &PrepareTab::printer_temperatureReport );
    }
}

void PrepareTab::_initialShowEvent( QShowEvent* event ) {
    _currentLayerImage->setFixedSize( _currentLayerImage->width( ), _currentLayerImage->width( ) / AspectRatio16to10 + 0.5 );
    update( );

    event->accept( );
}

void PrepareTab::_connectUsbMountManager( ) {
    QObject::connect( _usbMountManager, &UsbMountManager::filesystemMounted,   this, &PrepareTab::usbMountManager_filesystemMounted   );
    QObject::connect( _usbMountManager, &UsbMountManager::filesystemUnmounted, this, &PrepareTab::usbMountManager_filesystemUnmounted );
}

bool PrepareTab::_checkPreSlicedFiles( ) {
    debug( "+ PrepareTab::_checkPreSlicedFiles\n" );

    // check that the sliced SVG file is newer than the STL file
    auto modelFile     = QFileInfo { _printJob->modelFileName                                   };
    auto slicedSvgFile = QFileInfo { _printJob->jobWorkingDirectory + Slash + SlicedSvgFileName };
    if ( !modelFile.exists( ) ) {
        debug( "  + Fail: model file does not exist\n" );
        return false;
    }
    if ( !slicedSvgFile.exists( ) ) {
        debug( "  + Fail: sliced SVG file does not exist\n" );
        return false;
    }

    auto slicedSvgFileLastModified = slicedSvgFile.lastModified( );
    if ( modelFile.lastModified( ) > slicedSvgFileLastModified ) {
        debug( "  + Fail: model file is newer than sliced SVG file\n" );
        return false;
    }

    int layerNumber     = -1;
    int prevLayerNumber = -1;

    auto jobDir = QDir { _printJob->jobWorkingDirectory };
    jobDir.setSorting( QDir::Name );
    jobDir.setNameFilters( { "[0-9]?????.svg" } );

    // check that the layer SVG files are newer than the sliced SVG file,
    //   and that the layer PNG files are newer than the layer SVG files,
    //   and that there are no gaps in the numbering.
    for ( auto entry : jobDir.entryInfoList( ) ) {
        if ( slicedSvgFileLastModified > entry.lastModified( ) ) {
            debug( "  + Fail: sliced SVG file is newer than layer SVG file %s\n", entry.fileName( ).toUtf8( ).data( ) );
            return false;
        }

        auto layerPngFile = QFileInfo { entry.path( ) + Slash + entry.completeBaseName( ) + QString( ".png" ) };
        if ( !layerPngFile.exists( ) ) {
            debug( "  + Fail: layer PNG file %s does not exist\n", layerPngFile.fileName( ).toUtf8( ).data( ) );
            return false;
        }
        if ( entry.lastModified( ) > layerPngFile.lastModified( ) ) {
            debug( "  + Fail: layer SVG file %s is newer than layer PNG file %s\n", entry.fileName( ).toUtf8( ).data( ), layerPngFile.fileName( ).toUtf8( ).data( ) );
            return false;
        }

        layerNumber = RemoveFileExtension( entry.baseName( ) ).toInt( );
        if ( layerNumber != ( prevLayerNumber + 1 ) ) {
            debug( "  + Fail: gap in layer numbers between %d and %d\n", prevLayerNumber, layerNumber );
            return false;
        }
        prevLayerNumber = layerNumber;
    }

    _printJob->layerCount = layerNumber + 1;
    debug( "  + Success: %d layers\n", _printJob->layerCount );

    return true;
}

bool PrepareTab::_checkJobDirectory( ) {
    _printJob->jobWorkingDirectory = JobWorkingDirectoryPath + Slash + _printJob->modelHash + QString( "-%1" ).arg( _printJob->layerThickness );

    debug(
        "  + model filename:        '%s'\n"
        "  + job working directory: '%s'\n"
        "",
        _printJob->modelFileName.toUtf8( ).data( ),
        _printJob->jobWorkingDirectory.toUtf8( ).data( )
    );

    QDir jobDir { _printJob->jobWorkingDirectory };
    bool preSliced;

    if ( jobDir.exists( ) ) {
        debug( "  + job directory already exists, checking sliced model\n" );
        preSliced = _checkPreSlicedFiles( );
        if ( preSliced ) {
            debug( "  + pre-sliced model is good\n" );
        } else {
            debug( "  + pre-sliced model is NOT good\n" );
            jobDir.removeRecursively( );
        }
    } else {
        debug( "  + job directory does not exist; will create later\n" );
        preSliced = false;
    }

    _setNavigationButtonsEnabled( preSliced );
    _setSliceControlsEnabled( true );
    if ( preSliced ) {
        _navigateCurrentLabel->setText( QString( "1/%1" ).arg( _printJob->layerCount ) );
        _sliceButton->setText( "Reslice" );
        _copyToUSBButton->setEnabled( true );
    } else {
        _navigateCurrentLabel->setText( "0/0" );
        _sliceButton->setText( "Slice" );
        _copyToUSBButton->setEnabled( false );
    }

    update( );
    return preSliced;
}

void PrepareTab::layerThickness50Button_clicked( bool ) {
    debug( "+ PrepareTab::layerThickness50Button_clicked\n" );
    _printJob->layerThickness = 50;

    _checkJobDirectory( );
}

void PrepareTab::layerThickness100Button_clicked( bool ) {
    debug( "+ PrepareTab::layerThickness100Button_clicked\n" );
    _printJob->layerThickness = 100;

    _checkJobDirectory( );
}

void PrepareTab::_setNavigationButtonsEnabled( bool const enabled ) {
    _navigateFirst   ->setEnabled( enabled && ( _visibleLayer > 0 ) );
    _navigatePrevious->setEnabled( enabled && ( _visibleLayer > 0 ) );
    _navigateNext    ->setEnabled( enabled && ( _printJob && ( _visibleLayer + 1 < _printJob->layerCount ) ) );
    _navigateLast    ->setEnabled( enabled && ( _printJob && ( _visibleLayer + 1 < _printJob->layerCount ) ) );

    update( );
}

void PrepareTab::_showLayerImage( int const layer ) {
    auto pixmap = QPixmap( _printJob->jobWorkingDirectory + QString( "/%2.png" ).arg( layer, 6, 10, DigitZero ) );
    if ( ( pixmap.width( ) > _currentLayerImage->width( ) ) || ( pixmap.height( ) > _currentLayerImage->height( ) ) ) {
        pixmap = pixmap.scaled( _currentLayerImage->size( ), Qt::KeepAspectRatio, Qt::SmoothTransformation );
    }

    _currentLayerImage->setPixmap( pixmap );
    _navigateCurrentLabel->setText( QString( "%1/%2" ).arg( layer + 1 ).arg( _printJob->layerCount ) );

    update( );
}

void PrepareTab::_setSliceControlsEnabled( bool const enabled ) {
    _sliceButton->setEnabled( enabled );
    _layerThicknessLabel->setEnabled( enabled );
    _layerThickness100Button->setEnabled( enabled );
    _layerThickness50Button->setEnabled( enabled );

    update( );
}

void PrepareTab::_updatePrepareButtonState( ) {
    _prepareButton->setEnabled( _isPrinterOnline && _isPrinterAvailable );

    update( );
}

void PrepareTab::navigateFirst_clicked( bool ) {
    _visibleLayer = 0;
    _setNavigationButtonsEnabled( true );
    _showLayerImage( _visibleLayer );

    update( );
}

void PrepareTab::navigatePrevious_clicked( bool ) {
    if ( _visibleLayer > 0 ) {
        --_visibleLayer;
    }
    _setNavigationButtonsEnabled( true );
    _showLayerImage( _visibleLayer );

    update( );
}

void PrepareTab::navigateNext_clicked( bool ) {
    if ( _visibleLayer + 1 < _printJob->layerCount ) {
        ++_visibleLayer;
    }
    _setNavigationButtonsEnabled( true );
    _showLayerImage( _visibleLayer );

    update( );
}

void PrepareTab::navigateLast_clicked( bool ) {
    _visibleLayer = _printJob->layerCount - 1;
    _setNavigationButtonsEnabled( true );
    _showLayerImage( _visibleLayer );

    update( );
}

void PrepareTab::sliceButton_clicked( bool ) {
    debug( "+ PrepareTab::sliceButton_clicked\n" );

    QDir jobDir { _printJob->jobWorkingDirectory };
    jobDir.removeRecursively( );
    jobDir.mkdir( _printJob->jobWorkingDirectory );

    _sliceStatus->setText( "starting" );
    _imageGeneratorStatus->setText( "waiting" );

    TimingLogger::startTiming( TimingId::SlicingSvg, GetFileBaseName( _printJob->modelFileName ) );
    _slicerProcess = new QProcess( this );
    QObject::connect( _slicerProcess, &QProcess::errorOccurred,                                        this, &PrepareTab::slicerProcess_errorOccurred );
    QObject::connect( _slicerProcess, &QProcess::started,                                              this, &PrepareTab::slicerProcess_started       );
    QObject::connect( _slicerProcess, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), this, &PrepareTab::slicerProcess_finished      );
    _slicerProcess->start(
        { "slic3r" },
        {
            _printJob->modelFileName,
            "--threads",            QString { "%1" }.arg( get_nprocs( ) ),
            "--export-svg",
            "--first-layer-height", QString { "%1" }.arg( _printJob->layerThickness / 1000.0 ),
            "--layer-height",       QString { "%1" }.arg( _printJob->layerThickness / 1000.0 ),
            "--output",             _printJob->jobWorkingDirectory + Slash + SlicedSvgFileName
        }
    );

    _setSliceControlsEnabled( false );
    emit uiStateChanged( TabIndex::Prepare, UiState::SliceStarted );

    update( );
}

void PrepareTab::hasher_resultReady( QString const hash ) {
    debug(
        "+ PrepareTab::hasher_resultReady:\n"
        "  + result hash:           '%s'\n"
        "",
        hash.toUtf8( ).data( )
    );

    _printJob->modelHash = hash.isEmpty( ) ? QString( "%1-%2" ).arg( time( nullptr ) ).arg( getpid( ) ) : hash;

    _sliceStatus->setText( "idle" );
    _hasher = nullptr;

    bool goodJobDir = _checkJobDirectory( );
    emit slicingNeeded( !goodJobDir );

    update( );
}

void PrepareTab::slicerProcess_errorOccurred( QProcess::ProcessError error ) {
    debug( "+ PrepareTab::slicerProcess_errorOccurred: error %s [%d]\n", ToString( error ), error );

    if ( QProcess::FailedToStart == error ) {
        debug( "  + slicer process failed to start\n" );
        _sliceStatus->setText( "failed to start" );
    } else if ( QProcess::Crashed == error ) {
        debug( "  + slicer process crashed? state is %s [%d]\n", ToString( _slicerProcess->state( ) ), _slicerProcess->state( ) );
        if ( _slicerProcess->state( ) != QProcess::NotRunning ) {
            _slicerProcess->kill( );
            debug( "  + slicer terminated\n" );
        }
        _sliceStatus->setText( "crashed" );
    }

    update( );
}

void PrepareTab::slicerProcess_started( ) {
    debug( "+ PrepareTab::slicerProcess_started\n" );
    _sliceStatus->setText( "started" );

    update( );
}

void PrepareTab::slicerProcess_finished( int exitCode, QProcess::ExitStatus exitStatus ) {
    TimingLogger::stopTiming( TimingId::SlicingSvg );
    debug( "+ PrepareTab::slicerProcess_finished: exitCode: %d, exitStatus: %s [%d]\n", exitCode, ToString( exitStatus ), exitStatus );

    _slicerProcess->deleteLater( );
    _slicerProcess = nullptr;

    if ( exitStatus == QProcess::CrashExit ) {
        debug( "  + slicer process crashed?\n" );
        _sliceStatus->setText( "crashed" );
        emit uiStateChanged( TabIndex::Prepare, UiState::SelectCompleted );

        update( );
        return;
    } else if ( ( exitStatus == QProcess::NormalExit ) && ( exitCode != 0 ) ) {
        debug( "  + slicer process failed\n" );
        _sliceStatus->setText( "failed" );
        emit uiStateChanged( TabIndex::Prepare, UiState::SelectCompleted );

        update( );
        return;
    }

    _sliceStatus->setText( "finished" );
    _imageGeneratorStatus->setText( "starting" );
    _copyToUSBButton->setEnabled(true);

    update( );

    _svgRenderer = new SvgRenderer;
    QObject::connect( _svgRenderer, &SvgRenderer::layerCount,    this, &PrepareTab::svgRenderer_layerCount    );
    QObject::connect( _svgRenderer, &SvgRenderer::layerComplete, this, &PrepareTab::svgRenderer_layerComplete );
    QObject::connect( _svgRenderer, &SvgRenderer::done,          this, &PrepareTab::svgRenderer_done          );
    _svgRenderer->startRender( _printJob->jobWorkingDirectory + Slash + SlicedSvgFileName, _printJob->jobWorkingDirectory );
}

void PrepareTab::svgRenderer_layerCount( int const totalLayers ) {
    debug( "+ PrepareTab::svgRenderer_layerCount: totalLayers %d\n", totalLayers );
    _printJob->layerCount = totalLayers;

    _navigateCurrentLabel->setText( QString( "1/%1" ).arg( _printJob->layerCount ) );
    update( );
}

void PrepareTab::svgRenderer_layerComplete( int const currentLayer ) {
    _renderedLayers = currentLayer;
    _imageGeneratorStatus->setText( QString( "layer %1" ).arg( currentLayer + 1 ) );

    if ( ( 0 == currentLayer ) || ( 0 == ( ( currentLayer + 1 ) % 5 ) ) || ( ( _printJob->layerCount - 1 ) == currentLayer ) ) {
        _visibleLayer = currentLayer;
        _showLayerImage( _visibleLayer );
    }
    update( );
}

void PrepareTab::svgRenderer_done( bool const success ) {
    _imageGeneratorStatus->setText( success ? "finished" : "failed" );

    _svgRenderer->deleteLater( );
    _svgRenderer = nullptr;

    _setNavigationButtonsEnabled( success );
    _sliceButton->setText( success ? "Reslice" : "Slice" );

    emit uiStateChanged( TabIndex::Prepare, success ? UiState::SliceCompleted : UiState::SelectCompleted );

    update( );
}

void PrepareTab::_handlePrepareFailed( ) {
    _prepareMessage->setText( "Preparation failed." );

    QObject::connect( _prepareButton, &QPushButton::clicked, this, &PrepareTab::prepareButton_clicked );

    _prepareButton->setText( "Retry" );
    _prepareButton->setEnabled( true );

    setPrinterAvailable( true );
    emit printerAvailabilityChanged( true );
    emit preparePrinterComplete( false );

    update( );
}

void PrepareTab::prepareButton_clicked( bool ) {
    debug( "+ PrepareTab::prepareButton_clicked\n" );

    QObject::disconnect( _prepareButton, &QPushButton::clicked, this, nullptr );

    _prepareMessage->setText( "Moving the build platform to its home location…" );
    _prepareProgress->show( );

    _prepareButton->setText( "Continue" );
    _prepareButton->setEnabled( false );

    QObject::connect( _shepherd, &Shepherd::action_homeComplete, this, &PrepareTab::shepherd_homeComplete );
    _shepherd->doHome( );

    setPrinterAvailable( false );
    emit printerAvailabilityChanged( false );
    emit preparePrinterStarted( );

    update( );
}

void PrepareTab::shepherd_homeComplete( bool const success ) {
    debug( "+ PrepareTab::shepherd_homeComplete: success: %s\n", success ? "true" : "false" );

    QObject::disconnect( _shepherd, nullptr, this, nullptr );
    _prepareProgress->hide( );

    if ( !success ) {
        _handlePrepareFailed( );
        return;
    }

    _prepareMessage->setText( "Adjust the build platform position, then tap <b>Continue</b>." );

    QObject::disconnect( _prepareButton, &QPushButton::clicked, this, nullptr                                   );
    QObject::connect   ( _prepareButton, &QPushButton::clicked, this, &PrepareTab::adjustBuildPlatform_complete );
    _prepareButton->setEnabled( true );

    update( );
}

void PrepareTab::adjustBuildPlatform_complete( bool ) {
    debug( "+ PrepareTab::adjustBuildPlatform_complete\n" );

    QObject::disconnect( _prepareButton, &QPushButton::clicked, this, nullptr );
    _prepareButton->setEnabled( false );

    _prepareMessage->setText( "Raising the build platform…" );
    _prepareProgress->show( );

    QObject::connect( _shepherd, &Shepherd::action_moveAbsoluteComplete, this, &PrepareTab::shepherd_raiseBuildPlatformMoveToComplete );
    _shepherd->doMoveAbsolute( PrinterRaiseToMaximumZ, PrinterDefaultHighSpeed );

    update( );
}

void PrepareTab::shepherd_raiseBuildPlatformMoveToComplete( bool const success ) {
    debug( "+ PrepareTab::shepherd_raiseBuildPlatformMoveToComplete: success: %s\n", success ? "true" : "false" );

    QObject::disconnect( _shepherd, nullptr, this, nullptr );
    _prepareProgress->hide( );

    QObject::connect( _prepareButton, &QPushButton::clicked, this, &PrepareTab::prepareButton_clicked );
    _prepareButton->setEnabled( true );

    if ( !success ) {
        _handlePrepareFailed( );
        return;
    }

    _prepareMessage->setText( "Preparation completed." );
    _prepareButton->setText( "Prepare" );

    setPrinterAvailable( true );
    emit printerAvailabilityChanged( true );
    emit preparePrinterComplete( true );

    update( );
}

void PrepareTab::tab_uiStateChanged( TabIndex const sender, UiState const state ) {
    debug( "+ PrepareTab::tab_uiStateChanged: from %sTab: %s => %s\n", ToString( sender ), ToString( _uiState ), ToString( state ) );
    _uiState = state;

    switch ( _uiState ) {
        case UiState::SelectStarted:
            _setSliceControlsEnabled( false );
            break;

        case UiState::SelectCompleted:
            _setSliceControlsEnabled( false );

            _sliceStatus->setText( "idle" );
            _imageGeneratorStatus->setText( "idle" );
            _currentLayerImage->clear( );
            _navigateCurrentLabel->setText( "0/0" );
            _setNavigationButtonsEnabled( false );

            if ( _hasher ) {
                _hasher->deleteLater( );
            }
            _hasher = new Hasher;
            QObject::connect( _hasher, &Hasher::resultReady, this, &PrepareTab::hasher_resultReady, Qt::QueuedConnection );
            _hasher->hash( _printJob->modelFileName, QCryptographicHash::Md5 );
            break;

        case UiState::SliceStarted:
            _setSliceControlsEnabled( false );
            break;

        case UiState::SliceCompleted:
            _setSliceControlsEnabled( true );
            break;

        case UiState::PrintStarted:
            _setSliceControlsEnabled( false );
            setPrinterAvailable( false );
            emit printerAvailabilityChanged( false );
            break;

        case UiState::PrintCompleted:
            _setSliceControlsEnabled( true );
            setPrinterAvailable( true );
            emit printerAvailabilityChanged( true );
            break;
        case UiState::SelectedDirectory:
            _setSliceControlsEnabled( false );
            slicerProcess_finished(0, QProcess::NormalExit);
            break;
    }

    update( );
}

void PrepareTab::printer_online( ) {
    _isPrinterOnline = true;
    debug( "+ PrepareTab::printer_online: PO? %s PA? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ) );

    _updatePrepareButtonState( );
}

void PrepareTab::printer_offline( ) {
    _isPrinterOnline = false;
    debug( "+ PrepareTab::printer_offline: PO? %s PA? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ) );

    _updatePrepareButtonState( );
}

void PrepareTab::printer_temperatureReport( double const bedCurrentTemperature, double const, int const ) {
    if ( bedCurrentTemperature >= 30.0 ) {
        _warningHotLabel->setPixmap( *_warningHotImage );
    } else {
        _warningHotLabel->clear( );
    }

    update( );
}

void PrepareTab::printManager_lampStatusChange( bool const on ) {
    if ( on ) {
        _warningUvLabel->setPixmap( *_warningUvImage );
    } else {
        _warningUvLabel->clear( );
    }

    update( );
}

void PrepareTab::setPrinterAvailable( bool const value ) {
    _isPrinterAvailable = value;
    debug( "+ PrepareTab::setPrinterAvailable: PO? %s PA? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ) );

    _updatePrepareButtonState( );
}

void PrepareTab::copyToUSB_clicked( bool ) {
    debug( "+ PrepareTab::copyToUSB_clicked\n" );

    QDir jobDir  { _printJob->jobWorkingDirectory };
    QDir _mediaDir { _usbPath };

    _mediaDir.mkdir(jobDir.dirName());
    QDirIterator it(_printJob->jobWorkingDirectory, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString fileName = it.next();
        QFileInfo fileInfo(fileName);

        QString dest = _usbPath + Slash + jobDir.dirName() + Slash + fileInfo.fileName();

        QFile::copy(fileName,  dest);

        std::cout << dest.toStdString() << std::endl;
    }

    update( );
}

void PrepareTab::usbMountManager_filesystemMounted( QString const& mountPoint ) {
    debug( "+ PrepareTab::usbMountManager_filesystemMounted: mount point '%s'\n", mountPoint.toUtf8( ).data( ) );

    if ( !_usbPath.isEmpty( ) ) {
        debug( "  + We already have a USB storage device at '%s' mounted; ignoring new mount\n", _usbPath.toUtf8( ).data( ) );
        return;
    }

    QFileInfo usbPathInfo { mountPoint };
    if ( !usbPathInfo.isReadable( ) || !usbPathInfo.isExecutable( ) ) {
        debug( "  + Unable to access mount point '%s' (uid: %u; gid: %u; mode: 0%03o)\n", _usbPath.toUtf8( ).data( ), usbPathInfo.ownerId( ), usbPathInfo.groupId( ), usbPathInfo.permissions( ) & 07777 );
        return;
    }

    _usbPath = mountPoint;
    _createUsbFsModel( );

    update( );
}

void PrepareTab::usbMountManager_filesystemUnmounted( QString const& mountPoint ) {
    debug( "+ PrepareTab::usbMountManager_filesystemUnmounted: mount point '%s'\n", mountPoint.toUtf8( ).data( ) );

    if ( mountPoint != _usbPath ) {
        debug( "  + not our filesystem; ignoring\n", _usbPath.toUtf8( ).data( ) );
        return;
    }

    _usbPath.clear( );
    _destroyUsbFsModel( );
    _copyToUSBButton->setEnabled( false );

    update( );
}

void PrepareTab::_destroyUsbFsModel( ) {
    if ( _usbFsModel ) {
        QObject::disconnect( _usbFsModel );
        _usbFsModel->deleteLater( );
        _usbFsModel = nullptr;
    }
}

void PrepareTab::_createUsbFsModel( ) {
    _destroyUsbFsModel( );

    _usbFsModel = new QFileSystemModel;
    _usbFsModel->setFilter( QDir::Dirs );
    _usbFsModel->setNameFilterDisables( false );
    _usbFsModel->setNameFilters( { { "*-50" }, {"-100"} } );
    (void) QObject::connect( _usbFsModel, &QFileSystemModel::directoryLoaded, this, &PrepareTab::usbFsModel_directoryLoaded );
    _usbFsModel->setRootPath( _usbPath );
}

void PrepareTab::usbFsModel_directoryLoaded( QString const& /*name*/ ) {
    debug( "+ PrepareTab::usbFsModel_directoryLoaded\n" );

    if(!_sliceButton->isEnabled())
        _copyToUSBButton->setEnabled(true);

    update( );
}

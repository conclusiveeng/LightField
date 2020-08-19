#include "pch.h"

#include "printmanager.h"
#include "movementsequencer.h"
#include "ordermanifestmanager.h"
#include "pngdisplayer.h"
#include "printjob.h"
#include "processrunner.h"
#include "firmwarecontroller.h"
#include "timinglogger.h"

// ================================
// == Section A: Before printing ==
// ================================
//
// A1. Raise the build platform to maximum Z at high speed.
// A2. Prompt user to dispense recommended volume of print solution.
// A3. Lower to high-speed threshold Z at high speed, then first layer height at low speed, and wait for ${PauseAfterPrintSolutionDispensed} ms.
//
// ====================================
// == Section B: For each base layer ==
// ====================================
//
// B1. Start projection: "set-projector-power ${printJob.powerLevel}".
// B2. Pause for layer projection time.
//     -> if current job has multi-tiled elements
//     B2a. change image - loop over tiled variants
//     -> end if
// B3. Stop projection: "set-projector-power 0".
// B4a1. Pause before "pumping" manoeuvre.
// B4a2. Perform the "pumping" manoeuvre.
// B4b1. Pause before projection.
// TODO what about pause??
// --- pause is disabled
// --- pause is re-enabled
//
// ====================================
// == Section C: For each body layer ==
// ====================================
//
// C1. Start projection: "set-projector-power ${printJob.powerLevel}".
// C2. Pause for layer projection time.
//     -> if current job has multi-tiled elements
//     C2a. change image - loop over tiled variants
//     -> end if
// C3. Stop projection: "set-projector-power 0".
// C4a1. Pause before "pumping" manoeuvre.
// C4a2. Perform the "pumping" manoeuvre.
// C4b1. Pause before projection.
// TODO what about pause??
// --- pause is disabled
// --- pause is re-enabled
//
// ===============================
// == Section D: After printing ==
// ===============================
//
// D1. Raise the build platform to maximum Z, first at low speed to the high-speed threshold Z, then high speed.
//
// =================================
// == Section E: Pause and resume ==
// =================================
//
// E1. Raise the build platform to the maximum Z position, first at low speed to the high-speed threshold Z, then high speed.
// --- wait for Resume to be pressed
// E2. Lower the build platform to the paused Z position, first at high speed to the high-speed threshold Z, then low speed.
//

#undef ICEBUG

namespace {

#if defined ICEBUG
    auto const PauseAfterPrintSolutionDispensed =  250; // ms
    auto const PauseBeforeProject               =  250; // ms
    auto const PauseBeforeLift                  =  250; // ms
#else
    auto const PauseAfterPrintSolutionDispensed = 4000; // ms
    auto const PauseBeforeProject               = 4000; // ms
    auto const PauseBeforeLift                  = 2000; // ms
#endif // defined ICEBUG

    char const* PrintStepStrings[] {
        "none",
        "A1", "A2", "A3",
        "B1", "B2", "B3",
            "B4a1", "B4a2",
            "B4b1",
        "C1", "C2", "C3",
            "C4a1", "C4a2",
            "C4b1",
        "D1",
        "E1", "E2",
    };

    bool IsBadPrintResult( PrintResult const printResult ) {
        return printResult < PrintResult::None;
    }

    char const* ToString( PrintStep const value ) {
#if defined _DEBUG
        if ( ( value >= PrintStep::none ) && ( value <= PrintStep::C1 ) ) {
#endif
            return PrintStepStrings[static_cast<int>( value )];
#if defined _DEBUG
        } else {
            return nullptr;
        }
#endif
    }


}

PrintManager::PrintManager(FirmwareController* controller, QObject* parent):
    QObject   (parent),
    _firmwareController (controller)
{
    _movementSequencer        = new MovementSequencer { controller, this };
    _setProjectorPowerProcess = new ProcessRunner     { this };

    QObject::connect(_firmwareController, &FirmwareController::printerPositionReport, this,
        &PrintManager::printer_positionReport);
}

PrintManager::~PrintManager( ) {
    /*empty*/
}

QTimer* PrintManager::_makeAndStartTimer( int const interval, void ( PrintManager::*func )( ) ) {
    auto timer = new QTimer( this );
    QObject::connect( timer, &QTimer::timeout, this, func );
    timer->setInterval( interval );
    timer->setSingleShot( true );
    timer->setTimerType( Qt::PreciseTimer );
    timer->start( );
    return timer;
}

void PrintManager::_stopAndCleanUpTimer( QTimer*& timer ) {
    if ( !timer ) {
        return;
    }

    QObject::disconnect( timer, nullptr, this, nullptr );
    timer->stop( );
    timer->deleteLater( );
    timer = nullptr;
}

void PrintManager::_pausePrinting( ) {
    debug( "+ PrintManager::_pausePrinting: step: %s; position: %.2f mm\n", ToString( _step ), _position );
    _pausedPosition = _position;
    _pausedStep     = _step;
    stepE1_start( );
}

void PrintManager::_cleanUp( ) {
    QObject::disconnect( this );

    _step = PrintStep::none;

    _stopAndCleanUpTimer( _preProjectionTimer );
    _stopAndCleanUpTimer( _layerExposureTimer );
    _stopAndCleanUpTimer( _preLiftTimer       );

    if ( _setProjectorPowerProcess ) {
        if ( _setProjectorPowerProcess->state( ) != QProcess::NotRunning ) {
            _setProjectorPowerProcess->kill( );
        }
        _setProjectorPowerProcess->deleteLater( );
        _setProjectorPowerProcess = nullptr;
    }
}

// ================================
// == Section A: Before printing ==
// ================================

// A1. Raise the build platform to maximum Z at high speed.
void PrintManager::stepA1_start( ) {
    _step = PrintStep::A1;

    debug( "+ PrintManager::stepA1_start: raising build platform to %.2f mm\n", PrinterRaiseToMaximumZ );

    QObject::connect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepA1_completed );

    _movementSequencer->setMovements( _stepA1_movements );
    _movementSequencer->execute( );
}

void PrintManager::stepA1_completed( bool const success ) {
    debug( "+ PrintManager::stepA1_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepA1_completed );

    if ( ( _printResult != PrintResult::Abort ) && !success ) {
        _printResult = PrintResult::Failure;
    }
    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    stepA2_start( );
}

// A2. Prompt user to dispense recommended volume of print solution.
void PrintManager::stepA2_start( ) {
    _step = PrintStep::A2;

    debug( "+ PrintManager::stepA2_start: waiting for user to dispense print solution\n" );

    emit requestDispensePrintSolution( );
}

void PrintManager::stepA2_completed( ) {
    debug( "+ PrintManager::stepA2_completed\n" );

    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    stepA3_start( );
}

// A3. Lower to high-speed threshold Z at high speed, then first layer height at low speed, and wait for ${PauseAfterPrintSolutionDispensed} ms.
void PrintManager::stepA3_start( ) {
    _step = PrintStep::A3;

    debug( "+ PrintManager::stepA3_start: lowering build platform to %.2f mm\n", PrinterHighSpeedThresholdZ );

    QObject::connect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepA3_completed );

    _movementSequencer->setMovements( _stepA3_movements );
    _movementSequencer->execute( );
}

void PrintManager::stepA3_completed( bool const success ) {
    debug( "+ PrintManager::stepA3_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepA3_completed );

    if ( !success && ( _printResult != PrintResult::Abort ) ) {
        _printResult = PrintResult::Failure;
    }
    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    emit printPausable(true);

    if ( printJob.hasBaseLayers() ) {
        stepB1_start( );
    } else if ( printJob.totalLayerCount() > 0 ) {
        stepC1_start( );
    } else {
        // this should never happen
        debug( "+ PrintManager::stepA3_completed: no base OR body layers to print?!\n" );
        stepD1_start( );
    }
}

// ====================================
// == Section B: For each base layer ==
// ====================================

// B1. Start projection: "set-projector-power ${printJob.powerLevel}".
void PrintManager::stepB1_start( ) {
    _step = PrintStep::B1;
    if ( _paused ) {
        _pausePrinting( );
        return;
    }

    auto powerLevel = PercentagePowerLevelToRawLevel(printJob.baseLayerParameters().powerLevel());
    debug( "+ PrintManager::stepB1_start: running 'set-projector-power %d'\n", powerLevel );

    QString pngFileName = printJob.getLayerPath( _currentLayer );
    if ( !_pngDisplayer->loadImageFile( pngFileName ) ) {
        debug( "+ PrintManager::stepB1_start: PngDisplayer::loadImageFile failed for file %s\n", pngFileName.toUtf8( ).data( ) );
        this->abort( );
        return;
    }

    QObject::connect( _setProjectorPowerProcess, &ProcessRunner::succeeded, this, &PrintManager::stepB1_completed );
    QObject::connect( _setProjectorPowerProcess, &ProcessRunner::failed,    this, &PrintManager::stepB1_failed    );
    _setProjectorPowerProcess->start( SetProjectorPowerCommand, { QString( "%1" ).arg( powerLevel ) } );

    emit startingLayer( _currentLayer );
}

void PrintManager::stepB1_completed( ) {
    debug( "+ PrintManager::stepB1_completed\n" );

    QObject::disconnect( _setProjectorPowerProcess, nullptr, this, nullptr );

    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    _lampOn = true;
    emit lampStatusChange( true );

    stepB2_start( );
}

void PrintManager::stepB1_failed( int const exitCode, QProcess::ProcessError const error ) {
    debug( "+ PrintManager::stepB1_completed: exitCode: %d, error: %s [%d]\n", exitCode, ToString( error ), static_cast<int>( error ) );
    stepB1_completed( );
}

// B2. Pause for layer projection time.
void PrintManager::stepB2_start( ) {
    debug( "+ PrintManager::stepB2_start\n" );
    _step = PrintStep::B2;

    int layerExposureTime;

    if (_isTiled) {
        layerExposureTime = 1000.0 * printJob.getTimeForElementAt(currentLayer());
        _duringTiledLayer = true;
    } else {
        layerExposureTime = printJob.baseLayerParameters().layerExposureTime();
    }

    debug( "+ PrintManager::stepB2_start: pausing for %d ms\n", layerExposureTime );

    _layerExposureTimer = _makeAndStartTimer( layerExposureTime, &PrintManager::stepB2_completed );
}

void PrintManager::stepB2_completed( ) {
    debug( "+ PrintManager::stepB2_completed\n" );

    _stopAndCleanUpTimer( _layerExposureTimer );


    if ( IsBadPrintResult( _printResult ) && !_isTiled ) {
        stepD1_start( );

        return;
    }
    if(_isTiled) {
     stepB2a_start();
    }else{
     stepB3_start( );
    }


}

void PrintManager::stepB2a_start( ){
    debug( "+ PrintManager::stepB2a_start\n" );

    _step = PrintStep::B2a;

    // abort would be serviced during B3_completed

    if(_hasLayerMoreElements()) {
        debug( "+ PrintManager::stepB2a_start: current layer has still more tiled elements to loop over'\n" );
        _currentLayer++;
        _pngDisplayer->clear( );
        emit startingLayer( _currentLayer );
        QString pngFileName = printJob.getLayerPath( _currentLayer );
        if ( !_pngDisplayer->loadImageFile( pngFileName ) ) {
            debug( "+ PrintManager::stepB2a_start: PngDisplayer::loadImageFile failed for file %s\n", pngFileName.toUtf8( ).data( ) );
            this->abort( );
            return;
        }
        stepB2_start( );
    } else {
        debug( "+ PrintManager::stepB2a_start: current layer has no more tiled elements'\n" );
        _duringTiledLayer = false;
        stepB3_start( );
    }

}

// B3. Stop projection: "set-projector-power 0".
void PrintManager::stepB3_start( ) {
    debug( "+ PrintManager::stepB3_start\n" );
    _step = PrintStep::B3;

    debug( "+ PrintManager::stepB3_start: running 'set-projector-power 0'\n" );

    _pngDisplayer->clear( );

    QObject::connect( _setProjectorPowerProcess, &ProcessRunner::succeeded, this, &PrintManager::stepB3_completed );
    QObject::connect( _setProjectorPowerProcess, &ProcessRunner::failed,    this, &PrintManager::stepB3_failed    );
    _setProjectorPowerProcess->start( SetProjectorPowerCommand, { "0" } );
}

void PrintManager::stepB3_completed( ) {
    debug( "+ PrintManager::stepB3_completed\n" );

    QObject::disconnect( _setProjectorPowerProcess, nullptr, this, nullptr );

    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    _lampOn = false;
    emit lampStatusChange( false );

    if ( printJob.baseLayerParameters().isPumpingEnabled( ) ) {
        stepB4a1_start( );
    } else {
        stepB4b1_start( );
    }
}

void PrintManager::stepB3_failed( int const, QProcess::ProcessError const ) {
    stepB3_completed( );
}

// B4a1. Pause before "pumping" manoeuvre.
void PrintManager::stepB4a1_start( ) {
    debug( "+ PrintManager::stepB4a1_start\n" );

    _step = PrintStep::B4a1;

    debug( "+ PrintManager::stepB4a1_start: pausing for %d ms before raising build platform\n", PauseBeforeLift );

    _preLiftTimer = _makeAndStartTimer( PauseBeforeLift, &PrintManager::stepB4a1_completed );
}

void PrintManager::stepB4a1_completed( ) {
    debug( "+ PrintManager::stepB4a1_completed\n" );

    _stopAndCleanUpTimer( _preLiftTimer );

    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    stepB4a2_start( );
}

// B4a2. Perform the "pumping" manoeuvre.
void PrintManager::stepB4a2_start()
{
    debug( "+ PrintManager::stepB4a2_start\n" );
    _step = PrintStep::B4a2;

    ++_currentLayer;
    ++_currentBaseLayer;
    if (_currentLayer == printJob.totalLayerCount()) {
        debug( "+ PrintManager::stepB4a2_start: print complete\n" );

        _printResult = PrintResult::Success;

        stepD1_start();
        return;
    }

    debug( "+ PrintManager::stepB4a2_start: performing 'pumping' manoeuvre\n" );

    QObject::connect(_movementSequencer, &MovementSequencer::movementComplete, this,
         &PrintManager::stepB4a2_completed);

    const auto& baseParameters = printJob.baseLayerParameters();

    QList<MovementInfo> movements = {
        {
            MoveType::Relative,
            baseParameters.pumpUpDistance(),
            baseParameters.pumpUpVelocity_Effective()
        },
        {
            baseParameters.pumpUpPause()
        },
        {
            MoveType::Relative,
            -baseParameters.pumpDownDistance_Effective() +
                (printJob.getLayerThicknessAt(_currentLayer - _elementsOnLayer) / 1000.0),
            baseParameters.pumpDownVelocity_Effective()
        },
        {
            baseParameters.pumpDownPause()
        }
    };

    _movementSequencer->setMovements(movements);
    _movementSequencer->execute( );
}

void PrintManager::stepB4a2_completed( bool const success ) {
    debug( "+ PrintManager::stepB4a2_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepB4a2_completed );

    if ( !success && ( _printResult != PrintResult::Abort ) ) {
        _printResult = PrintResult::Failure;
    }
    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    if ( _currentBaseLayer == printJob.getBaseLayerCount() ) {
        stepC1_start( );
    } else {
        stepB1_start( );
    }
}

// B4b1. Pause before projection.
void PrintManager::stepB4b1_start( ) {
    _step = PrintStep::B4b1;
    if ( _paused ) {
        _pausePrinting( );
        return;
    }

    ++_currentLayer;
    ++_currentBaseLayer;
    if ( _currentLayer == printJob.totalLayerCount() ) {
        debug( "+ PrintManager::stepB4b1_start: print complete\n" );

        _printResult = PrintResult::Success;
        stepD1_start( );
        return;
    }

    debug( "+ PrintManager::stepB4b1_start: pausing for %d ms before projecting layer\n", PauseBeforeProject );
    _preProjectionTimer = _makeAndStartTimer( PauseBeforeProject, &PrintManager::stepB4b1_completed );
}

void PrintManager::stepB4b1_completed( ) {
    debug( "+ PrintManager::stepB4b1_completed\n" );

    _stopAndCleanUpTimer( _preProjectionTimer );

    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    stepB4b2_start();
}

// B4b2. Move one layer up
void PrintManager::stepB4b2_start( ) {
    _step = PrintStep::B4b2;

    debug( "+ PrintManager::stepB4b2_start: moving one layer up\n" );

    QObject::connect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepB4b2_completed );

    _movementSequencer->setMovements( _stepB4b2_movements );
    _movementSequencer->execute( );
}

void PrintManager::stepB4b2_completed( bool const success ) {
    debug( "+ PrintManager::stepB4b2_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepB4b2_completed );

    if ( !success && ( _printResult != PrintResult::Abort ) ) {
        _printResult = PrintResult::Failure;
    }
    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }


    if ( _currentBaseLayer == printJob.getBaseLayerCount()) {
        stepC1_start( );
    } else {
        stepB1_start( );
    }
}

// ====================================
// == Section C: For each body layer ==
// ====================================

// C1. Start projection: "set-projector-power ${printJob.powerLevel}".
void PrintManager::stepC1_start( ) {
    _step = PrintStep::C1;
    if ( _paused ) {
        _pausePrinting( );
        return;
    }

    auto powerLevel = PercentagePowerLevelToRawLevel(printJob.bodyLayerParameters().powerLevel());
    debug( "+ PrintManager::stepC1_start: running 'set-projector-power %d'\n", powerLevel );

    QString pngFileName = printJob.getLayerPath( _currentLayer );
    if ( !_pngDisplayer->loadImageFile( pngFileName ) ) {
        debug( "+ PrintManager::stepC1_start: PngDisplayer::loadImageFile failed for file %s\n", pngFileName.toUtf8( ).data( ) );
        this->abort( );
        return;
    }

    QObject::connect( _setProjectorPowerProcess, &ProcessRunner::succeeded, this, &PrintManager::stepC1_completed );
    QObject::connect( _setProjectorPowerProcess, &ProcessRunner::failed,    this, &PrintManager::stepC1_failed    );
    _setProjectorPowerProcess->start( SetProjectorPowerCommand, { QString( "%1" ).arg( powerLevel ) } );

    emit startingLayer( _currentLayer );
}

void PrintManager::stepC1_completed( ) {
    debug( "+ PrintManager::stepC1_completed\n" );

    QObject::disconnect( _setProjectorPowerProcess, nullptr, this, nullptr );

    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    _lampOn = true;
    emit lampStatusChange( true );

    stepC2_start( );
}

void PrintManager::stepC1_failed( int const exitCode, QProcess::ProcessError const error ) {
    debug( "+ PrintManager::stepC1_failed: exitCode: %d, error: %s [%d]\n", exitCode, ToString( error ), static_cast<int>( error ) );
    stepC1_completed( );
}

// C2. Pause for layer projection time.
void PrintManager::stepC2_start( ) {
    _step = PrintStep::C2;

    int layerExposureTime;

    if (_isTiled) {
        layerExposureTime = 1000.0 * printJob.getTimeForElementAt(currentLayer());
        _duringTiledLayer = true;
    } else {
        if (printJob.hasExposureControlsEnabled())
            layerExposureTime = printJob.bodyLayerParameters().layerExposureTime();
    }

    debug( "+ PrintManager::stepC2_start: pausing for %d ms\n", layerExposureTime );

    _layerExposureTimer = _makeAndStartTimer( layerExposureTime, &PrintManager::stepC2_completed );
}

void PrintManager::stepC2_completed( ) {
    debug( "+ PrintManager::stepC2_completed\n" );

    _stopAndCleanUpTimer( _layerExposureTimer );

    if ( IsBadPrintResult( _printResult ) && !_isTiled ) {
        stepD1_start( );
        return;
    }

    if(_isTiled) {
        stepC2a_start();
    }else{
        stepC3_start( );
    }
}


void PrintManager::stepC2a_start( ){
    _step = PrintStep::C2a;

    // abort would be serviced during C3_completed

    if(_hasLayerMoreElements()){
        debug( "+ PrintManager::stepC2a_start: current layer has still more tiled elements to loop over'\n" );
        _currentLayer++;
        _pngDisplayer->clear( );
        emit startingLayer( _currentLayer );
        QString pngFileName = printJob.getLayerPath( _currentLayer );
        if ( !_pngDisplayer->loadImageFile( pngFileName ) ) {
            debug( "+ PrintManager::stepC2a_start: PngDisplayer::loadImageFile failed for file %s\n", pngFileName.toUtf8( ).data( ) );
            this->abort( );
            return;
        }
        stepC2_start( );
    }else{
        debug( "+ PrintManager::stepC2a_start: current layer has no more tiled elements'\n" );
        _duringTiledLayer = false;
        stepC3_start( );
    }

}

// C3. Stop projection: "set-projector-power 0".
void PrintManager::stepC3_start( ) {
    _step = PrintStep::C3;

    debug( "+ PrintManager::stepC3_start: running 'set-projector-power 0'\n" );

    _pngDisplayer->clear( );

    QObject::connect( _setProjectorPowerProcess, &ProcessRunner::succeeded, this, &PrintManager::stepC3_completed );
    QObject::connect( _setProjectorPowerProcess, &ProcessRunner::failed,    this, &PrintManager::stepC3_failed    );
    _setProjectorPowerProcess->start( SetProjectorPowerCommand, { "0" } );
}

void PrintManager::stepC3_completed( ) {
    debug( "+ PrintManager::stepC3_completed\n" );

    QObject::disconnect( _setProjectorPowerProcess, nullptr, this, nullptr );

    if (IsBadPrintResult(_printResult)) {
        stepD1_start();
        return;
    }

    _lampOn = false;
    emit lampStatusChange(false);

    if (printJob.bodyLayerParameters().isPumpingEnabled())
        stepC4a1_start();
    else
        stepC4b1_start();
}

void PrintManager::stepC3_failed( int const, QProcess::ProcessError const ) {
    stepC3_completed( );
}

// C4a1. Pause before "pumping" manoeuvre.
void PrintManager::stepC4a1_start( ) {
    _step = PrintStep::C4a1;

    debug( "+ PrintManager::stepC4a1_start: pausing for %d ms before raising build platform\n", PauseBeforeLift );

    _preLiftTimer = _makeAndStartTimer( PauseBeforeLift, &PrintManager::stepC4a1_completed );
}

void PrintManager::stepC4a1_completed( ) {
    debug( "+ PrintManager::stepC4a1_completed\n" );

    _stopAndCleanUpTimer( _preLiftTimer );

    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    stepC4a2_start( );
}

// C4a2. Perform the "pumping" manoeuvre.
void PrintManager::stepC4a2_start( )
{
    const auto& bodyParameters = printJob.bodyLayerParameters();
    double pumpDownDistance = -bodyParameters.pumpDownDistance_Effective() +
        (printJob.getLayerThicknessAt(_currentLayer - _elementsOnLayer + 1) / 1000.0);

    QList<MovementInfo> movements = {
        { MoveType::Relative, bodyParameters.pumpUpDistance(), bodyParameters.pumpUpVelocity_Effective() },
        { bodyParameters.pumpUpPause() },
        { MoveType::Relative, pumpDownDistance, bodyParameters.pumpDownVelocity_Effective() },
        {  bodyParameters.pumpDownPause() }
    };

    _step = PrintStep::C4a2;
    ++_currentLayer;

    if ( _currentLayer == printJob.totalLayerCount() ) {
        debug( "+ PrintManager::stepC4a2_start: print complete\n" );

        _printResult = PrintResult::Success;

        stepD1_start( );
        return;
    }

    debug( "+ PrintManager::stepC4a2_start: performing 'pumping' manoeuvre\n" );

    QObject::connect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepC4a2_completed );

    _movementSequencer->setMovements(movements);
    _movementSequencer->execute();
}

void PrintManager::stepC4a2_completed( bool const success ) {
    debug( "+ PrintManager::stepC4a2_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepC4a2_completed );

    if ( !success && ( _printResult != PrintResult::Abort ) ) {
        _printResult = PrintResult::Failure;
    }
    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    stepC1_start( );
}

// C4b1. Pause before projection.
void PrintManager::stepC4b1_start( ) {
    _step = PrintStep::C4b1;
    if ( _paused ) {
        _pausePrinting( );
        return;
    }

    ++_currentLayer;
    if ( _currentLayer == printJob.totalLayerCount() ) {
        debug( "+ PrintManager::stepC4b1_start: print complete\n" );

        _printResult = PrintResult::Success;
        stepD1_start( );
        return;
    }

    debug( "+ PrintManager::stepC4b1_start: pausing for %d ms before projecting layer\n", PauseBeforeProject );
    _preProjectionTimer = _makeAndStartTimer( PauseBeforeProject, &PrintManager::stepC4b1_completed );
}

void PrintManager::stepC4b1_completed( ) {
    debug( "+ PrintManager::stepC4b1_completed\n" );

    _stopAndCleanUpTimer( _preProjectionTimer );

    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    stepC4b2_start( );
}

// C4b2. Move one layer up
void PrintManager::stepC4b2_start()
{
    _step = PrintStep::C4b2;
    QList<MovementInfo> movements = {
        {
            MoveType::Relative,
            printJob.getLayerThicknessAt(_currentLayer) / 1000.0,
            PrinterDefaultLowSpeed
        }
    };

    debug("+ PrintManager::stepC4b2_start: moving one layer up\n");

    QObject::connect(_movementSequencer, &MovementSequencer::movementComplete, this,
        &PrintManager::stepC4b2_completed);
    _movementSequencer->setMovements(movements);
    _movementSequencer->execute( );
}

void PrintManager::stepC4b2_completed( bool const success ) {
    debug( "+ PrintManager::stepC4b2_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepC4b2_completed );

    if ( !success && ( _printResult != PrintResult::Abort ) ) {
        _printResult = PrintResult::Failure;
    }
    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    stepC1_start( );
}

// ===============================
// == Section D: After printing ==
// ===============================

// D1. Raise the build platform to maximum Z, first at low speed to the high-speed threshold Z, then high speed.
void PrintManager::stepD1_start()
{
    _step = PrintStep::D1;
    emit printPausable( false );

    if ( _lampOn ) {
        debug( "+ PrintManager::stepD1_start: Turning off lamp\n" );

        QProcess::startDetached( SetProjectorPowerCommand, { "0" } );
        _lampOn = false;
        emit lampStatusChange( false );
    }

    _pngDisplayer->clear( );

    debug( "+ PrintManager::stepD1_start: raising build platform to maximum Z\n" );

    QObject::connect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepD1_completed );

    const auto& parameters = printJob.isBaseLayer(_currentLayer)
        ? printJob.baseLayerParameters()
        : printJob.bodyLayerParameters();

    _movementSequencer->setMovements({
        { MoveType::Absolute, _threshold, parameters.noPumpUpVelocity() },
        { MoveType::Absolute, PrinterMaximumZ, PrinterDefaultHighSpeed }
    });

    _movementSequencer->execute();
}

void PrintManager::stepD1_completed(bool  success)
{
    TimingLogger::stopTiming( TimingId::Printing );
    debug( "+ PrintManager::stepD1_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepD1_completed );

    _running = false;

    if ( PrintResult::None == _printResult ) {
        _printResult = PrintResult::Success;
    }

    if ( PrintResult::Abort == _printResult ) {
        emit printAborted( );
    } else {
        emit printComplete( _printResult == PrintResult::Success );
    }

    _cleanUp( );
}

// =================================
// == Section E: Pause and resume ==
// =================================

// E1. Raise the build platform to the maximum Z position, first at low speed to the high-speed threshold Z, then high speed.
void PrintManager::stepE1_start( ) {
    _step = PrintStep::E1;

    debug( "+ PrintManager::stepE1_start: raising build platform to maximum Z\n" );

    QObject::connect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepE1_completed );

    const auto& parameters = printJob.isBaseLayer(_currentLayer)
        ? printJob.baseLayerParameters()
        : printJob.bodyLayerParameters();

    _movementSequencer->setMovements({
        { MoveType::Absolute, _threshold, parameters.noPumpUpVelocity() },
        { MoveType::Absolute, PrinterMaximumZ, PrinterDefaultHighSpeed }
    });

    _movementSequencer->execute();
}

void PrintManager::stepE1_completed(bool success)
{
    debug( "+ PrintManager::stepE1_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect(_movementSequencer, &MovementSequencer::movementComplete, this,
        &PrintManager::stepE1_completed );

    if ( !success && ( _printResult != PrintResult::Abort ) ) {
        _printResult = PrintResult::Failure;
    }
    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    emit printPaused( );
    debug( "+ PrintManager::stepE1_completed: waiting to resume\n" );
}

// E2. Lower the build platform to the paused Z position, first at high speed to the high-speed threshold Z, then low speed.
void PrintManager::stepE2_start( ) {
    _step = PrintStep::E2;

    debug( "+ PrintManager::stepE2_start: lowering build platform to paused Z position\n" );

    QObject::connect(_movementSequencer, &MovementSequencer::movementComplete, this,
        &PrintManager::stepE2_completed);

    const auto& parameters = printJob.isBaseLayer(_currentLayer)
        ? printJob.baseLayerParameters()
        : printJob.bodyLayerParameters();

    _movementSequencer->setMovements({
        { MoveType::Absolute, _threshold, PrinterDefaultHighSpeed },
        { MoveType::Absolute, _pausedPosition, parameters.noPumpUpVelocity() }
    });

    _movementSequencer->execute();
}

void PrintManager::stepE2_completed( bool const success ) {
    debug( "+ PrintManager::stepE2_completed: action %s\n", SucceededString( success ) );

    QObject::disconnect( _movementSequencer, &MovementSequencer::movementComplete, this, &PrintManager::stepE2_completed );

    if ( !success && ( _printResult != PrintResult::Abort ) ) {
        _printResult = PrintResult::Failure;
    }
    if ( IsBadPrintResult( _printResult ) ) {
        stepD1_start( );
        return;
    }

    emit printResumed( );
    switch ( _pausedStep ) {
        case PrintStep::B1:   stepB1_start( );   break;
        case PrintStep::B4b1: stepB4b1_start( ); break;
        case PrintStep::C1:   stepC1_start( );   break;
        case PrintStep::C4b1: stepC4b1_start( ); break;

        default:
            debug( "+ PrintManager::stepE4_completed: paused at invalid step %s?\n", ToString( _pausedStep ) );
            this->abort( );
            return;
    }
}

void PrintManager::print()
{
    if (printJob.isTiled()) {
        Q_ASSERT(printJob.getSelectedBaseLayerThickness() == -1);
        Q_ASSERT(printJob.getSelectedBodyLayerThickness() == -1);
    } else {
        if (printJob.hasBaseLayers()) {
            Q_ASSERT(printJob.getBaseLayerCount() > 0);
            Q_ASSERT(printJob.getSelectedBaseLayerThickness() > 0);
        }

        Q_ASSERT(printJob.getSelectedBodyLayerThickness() > 0);
    }

    _isTiled = printJob.isTiled();
    _elementsOnLayer = printJob.tilingCount();

    _pngDisplayer->clear();

    const auto& baseParameters = printJob.baseLayerParameters();
    const auto& bodyParameters = printJob.bodyLayerParameters();
    const auto& firstParameters = printJob.hasBaseLayers() ? baseParameters : bodyParameters;
    const auto firstLayerHeight = printJob.getBuildPlatformOffset() / 1000.0;

    _stepA1_movements.push_back({MoveType::Absolute, PrinterRaiseToMaximumZ, PrinterDefaultHighSpeed});
    _stepA3_movements.push_back({MoveType::Absolute, PrinterHighSpeedThresholdZ, PrinterDefaultHighSpeed});
    _stepA3_movements.push_back({MoveType::Absolute, firstLayerHeight, firstParameters.noPumpDownVelocity_Effective()});
    _stepA3_movements.push_back({PauseAfterPrintSolutionDispensed});

    _stepB4b2_movements.push_back({MoveType::Relative, (printJob.getSelectedBaseLayerThickness() / 1000.0), PrinterDefaultLowSpeed});

    TimingLogger::startTiming( TimingId::Printing, GetFileBaseName( printJob.getModelFilename() ) );
    _printResult = PrintResult::None;
    _currentBaseLayer = 0;
    _running = true;
    emit printStarting( );
    stepA1_start( );
}

void PrintManager::pause( ) {
    debug( "+ PrintManager::pause\n" );
    _paused = true;
}

void PrintManager::resume( ) {
    debug( "+ PrintManager::resume\n" );
    if ( !_paused ) {
        return;
    }

    _paused = false;
    stepE2_start( );
}

void PrintManager::terminate( ) {
    debug( "+ PrintManager::terminate\n" );
    _cleanUp( );
}

void PrintManager::abort( ) {
    debug( "+ PrintManager::abort: current step: %s; paused? %s\n", ToString( _step ), YesNoString( _paused ) );

    _printResult = PrintResult::Abort;

    if (_paused) {
        debug( "  + Printer is paused, going directly to step D1\n" );
        _paused = false;
        emit printResumed();
        stepD1_start();
        return;
    }

    if ( _movementSequencer->isActive( ) ) {
        debug( "  + Aborting movement sequencer and waiting for step to stop\n" );
        _movementSequencer->abort( );
        return;
    }

    switch ( _step ) {
        case PrintStep::none:
            debug( "  + Going directly to step D1\n" );
            stepD1_start( );
            break;

        case PrintStep::A2:
            // TODO is this still necessary?
            debug( "  + Aborting print solution dispense prompt\n" );
            stepA2_completed( );
            break;

        case PrintStep::B4a1:
            // TODO is this still necessary?
            debug( "  + Interrupting base pre-lift timer\n" );
            _stopAndCleanUpTimer( _preLiftTimer );
            stepB4a1_completed( );
            break;

        case PrintStep::C4a1:
            // TODO is this still necessary?
            debug( "  + Interrupting body pre-lift timer\n" );
            _stopAndCleanUpTimer( _preLiftTimer );
            stepC4a1_completed( );
            break;

        default:
            debug( "  + Waiting on current step to stop\n" );
            break;
    }
}

void PrintManager::setPngDisplayer( PngDisplayer* pngDisplayer ) {
    _pngDisplayer = pngDisplayer;
}

void PrintManager::printSolutionDispensed( ) {
    stepA2_completed( );
}

void PrintManager::printer_positionReport( double px, int /*cx*/ ) {
    _position  = px;
    _threshold = std::min( PrinterRaiseToMaximumZ, PrinterHighSpeedThresholdZ + _position );
    debug( "+ PrintManager::printer_positionReport: new position %.2f mm, new threshold %.2f mm\n", _position, _threshold );
}

bool PrintManager::_hasLayerMoreElements() {
    if (_currentLayer+1 == printJob.totalLayerCount()) {
    return false;
    }

    if(_currentLayer == 0){
      return (_elementsOnLayer>1) ? true : false;
    }else{
      return (_currentLayer+1) % _elementsOnLayer;
    }
}

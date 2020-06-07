#ifndef __PRINT_MANAGER_H__
#define __PRINT_MANAGER_H__

#include <QtCore>
#include "constants.h"

class MovementInfo;
class MovementSequencer;
class PngDisplayer;
class PrintJob;
class ProcessRunner;
class Shepherd;
class OrderManifestManager;

enum class PrintResult {
    Abort   = -2,
    Failure = -1,
    None    =  0,
    Success =  1,
};

enum class PrintStep {
    none,
    A1, A2, A3,
    C1, C2, C2a, C3,
        C4, C5,

    D1,
    E1, E2,
};

class PrintManager: public QObject {

    Q_OBJECT

public:

    PrintManager(Shepherd* shepherd, QObject* parent = nullptr);
    virtual ~PrintManager() override;

    int currentLayer() const
    {
        return _currentLayer;
    }

    bool isPaused() const
    {
        return _paused;
    }

    QString& currentLayerImage();

private:
    Shepherd*           _shepherd                 { };
    MovementSequencer*  _movementSequencer        { };
    QSharedPointer<PrintJob> _printJob;
    PngDisplayer*       _pngDisplayer             { };
    ProcessRunner*      _setProjectorPowerProcess { };
    PrintResult         _printResult              { };

    bool                _lampOn                   { };
    bool                _duringTiledLayer         {false};
    PrintStep           _step                     { };
    PrintStep           _pausedStep               { };
    int                 _currentLayer             { };
    int                 _elementsOnLayer          { };
    bool                _paused                   { false };
    double              _position                 { };
    double              _pausedPosition           { };
    double              _threshold                { PrinterHighSpeedThresholdZ };

    QTimer*             _preProjectionTimer       { };
    QTimer*             _layerExposureTimer       { };
    QTimer*             _preLiftTimer             { };

    QList<MovementInfo> _stepA1_movements;
    QList<MovementInfo> _stepA3_movements;
    QList<MovementInfo> _stepB4b2_movements;
    QList<MovementInfo> _stepC4b2_movements;

    QTimer* _makeAndStartTimer( int const duration, void ( PrintManager::*func )( ) );
    void    _stopAndCleanUpTimer( QTimer*& timer );
    void    _pausePrinting( );
    void    _cleanUp( );
    bool    _hasLayerMoreElements();

signals:

    void requestDispensePrintSolution( );

    void printStarting( );
    void printComplete( bool const success );
    void printAborted( );
    void printPausable( bool const pausable );
    void printPaused( );
    void printResumed( );
    void startingLayer( int const layer );
    void lampStatusChange( bool const on );

public slots:
    void setPngDisplayer(PngDisplayer* pngDisplayer);
    void print(QSharedPointer<PrintJob> printJob);
    void pause( );
    void resume( );
    void terminate( );
    void abort( );

    void printSolutionDispensed( );

    void printer_positionReport( double px, int cx );

private slots:
    void stepA1_start( );
    void stepA1_completed( bool const success );

    void stepA2_start( );
    void stepA2_completed( );

    void stepA3_start( );
    void stepA3_completed( bool const success );

    void stepC1_start( );
    void stepC1_completed( );
    void stepC1_failed( int const exitCode, QProcess::ProcessError const error );

    void stepC2_start( );
    void stepC2_completed( );

    void stepC2a_start( );

    void stepC3_start( );
    void stepC3_completed( );
    void stepC3_failed( int const exitCode, QProcess::ProcessError const error );

    void stepC4_start( );
    void stepC4_completed( );

    void stepC5_start( );
    void stepC5_completed( bool const success );

    void stepD1_start( );
    void stepD1_completed( bool const success );


    void stepE1_start( );
    void stepE1_completed( bool const success );

    void stepE2_start( );
    void stepE2_completed( bool const success );

};

#endif // __PRINT_MANAGER_H__

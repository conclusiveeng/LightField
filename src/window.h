#ifndef __WINDOW_H__
#define __WINDOW_H__

#include "coordinate.h"
#include "tabbase.h"
#include "uistate.h"

class ModelSelectionInfo;
class Shepherd;
class PrintManager;
class PrintJob;

class FileTab;
class PrepareTab;
class PrintTab;
class StatusTab;
class AdvancedTab;
class MaintenanceTab;

enum class TabIndex {
    File,
    Prepare,
    Print,
    Status,
    Advanced,
    Maintenance,
};

inline int operator+( TabIndex value ) {
    return static_cast<int>( value );
}

class Window: public QMainWindow/*InitialShowEventMixin<Window, QMainWindow>*/ {

    Q_OBJECT

public:

    Window( QWidget* parent = nullptr );
    virtual ~Window( ) override;

protected:

    virtual void closeEvent( QCloseEvent* event ) override;

private:

    Shepherd*              _shepherd             { };
    PrintJob*              _printJob             { };
    PrintManager*          _printManager         { };
    UiState                _uiState              { };

    QTabWidget*            _tabWidget            { new QTabWidget };

    FileTab*               _fileTab;
    PrepareTab*            _prepareTab;
    PrintTab*              _printTab;
    StatusTab*             _statusTab;
    AdvancedTab*           _advancedTab;
    MaintenanceTab*        _maintenanceTab;

    bool                   _isPrinterPrepared    { };
    bool                   _isModelRendered      { };

    std::vector<TabBase*>  _tabs;

    //virtual void _initialShowEvent( QShowEvent* event ) override;

    void _startPrinting( );

signals:

    void printJobChanged( PrintJob* printJob );
    void printManagerChanged( PrintManager* printManager );
    void shepherdChanged( Shepherd* shepherd );

public slots:

protected slots:

private slots:

    void uiStateChanged( UiState const state );

    void shepherd_started( );
    void shepherd_startFailed( );
    void shepherd_terminated( bool const expected, bool const cleanExit );

    void tabs_currentChanged( int index );

    void fileTab_modelSelected( ModelSelectionInfo* modelSelection );
    void fileTab_modelSelectionFailed( );

    void prepareTab_slicingNeeded( bool const needed );
    void prepareTab_sliceStarted( );
    void prepareTab_sliceComplete( bool const success );
    void prepareTab_renderStarted( );
    void prepareTab_renderComplete( bool const success );
    void prepareTab_preparePrinterStarted( );
    void prepareTab_preparePrinterComplete( bool const success );

    void printTab_printButtonClicked( );

    void statusTab_stopButtonClicked( );
    void statusTab_reprintButtonClicked( );
    void statusTab_cleanUpAfterPrint( );

    void signalHandler_signalReceived( int const signalNumber );

};

char const* ToString( TabIndex const index );

#endif // __WINDOW_H__

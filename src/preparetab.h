#ifndef __PREPARETAB_H__
#define __PREPARETAB_H__

#include <QtCore>
#include <QtWidgets>
#include "tabbase.h"
#include "ordermanifestmanager.h"
#include "printprofile.h"
#include "printjob.h"

class Hasher;
class SvgRenderer;


class PrepareTab: public InitialShowEventMixinTab<PrepareTab, TabBase> {

    Q_OBJECT

public:

    PrepareTab(QSharedPointer<PrintJob>& printJob, QWidget* parent = nullptr );
    virtual ~PrepareTab( ) override;

    bool             isPrepareButtonEnabled( )              const          { return _prepareButton->isEnabled( ); }
    bool             isSliceButtonEnabled( )                const          { return _sliceButton->isEnabled( );   }

    virtual TabIndex tabIndex( )                            const override { return TabIndex::Prepare;            }


protected:

    virtual void     _connectPrintManager( )                      override;
    virtual void     _connectShepherd( )                          override;
    virtual void     _initialShowEvent( QShowEvent* event )       override;
    virtual void     _connectUsbMountManager( )                   override;

private:

    QThreadPool       _threadPool;
    SvgRenderer*      _svgRenderer                 { };
    Hasher*           _hasher                      { };
    int               _visibleLayer                { };
    int               _renderedLayers              { };
    bool              _isPrinterOnline             { false };
    bool              _isPrinterAvailable          { true  };
    bool              _reslice                     { false };
    bool              _initAfterSelect             { true  };

    QLabel*           _layerThicknessLabel         { new QLabel           };
    QRadioButton*     _layerThickness100Button     { new QRadioButton     };
    QRadioButton*     _layerThickness50Button      { new QRadioButton     };
#if defined EXPERIMENTAL
    QRadioButton*     _layerThickness20Button      { new QRadioButton     };
#endif
    QRadioButton*     _layerThicknessCustomButton  { new QRadioButton     };

    QLabel*           _sliceStatusLabel            { new QLabel           };
    QLabel*           _sliceStatus                 { new QLabel           };
    QLabel*           _imageGeneratorStatusLabel   { new QLabel           };
    QLabel*           _imageGeneratorStatus        { new QLabel           };

    QGroupBox*        _prepareGroup                { new QGroupBox        };
    QLabel*           _prepareMessage              { new QLabel           };
    QProgressBar*     _prepareProgress             { new QProgressBar     };
    QPushButton*      _prepareButton               { new QPushButton      };
    //QPushButton*    _copyToUSBButton             { new QPushButton      };
    QString           _usbPath;
    QFileSystemModel* _libraryFsModel              { new QFileSystemModel };
    QFileSystemModel* _usbFsModel                  {                      };

    QPixmap*          _warningHotImage             {                      };
    QLabel*           _warningHotLabel             { new QLabel           };

    QPixmap*          _warningUvImage              {                      };
    QLabel*           _warningUvLabel              { new QLabel           };

    QWidget*          _optionsContainer            { new QWidget          };
    QPushButton*      _sliceButton                 { new QPushButton      };
    QPushButton*      _orderButton                 { new QPushButton      };

    QGroupBox*        _currentLayerGroup           { new QGroupBox        };
    QLabel*           _currentLayerImage           { new QLabel           };
    QVBoxLayout*      _currentLayerLayout          { new QVBoxLayout      };

    QPushButton*      _navigateFirst               { new QPushButton      };
    QPushButton*      _navigatePrevious            { new QPushButton      };
    QLabel*           _navigateCurrentLabel        { new QLabel           };
    QPushButton*      _navigateNext                { new QPushButton      };
    QPushButton*      _navigateLast                { new QPushButton      };
    QHBoxLayout*      _navigationLayout            {                      };

    QGridLayout*      _layout                      { new QGridLayout      };



    bool _checkPreSlicedFiles(const QString &directory, bool isBody);
    bool _checkOneSliceDirectory(const QString &directory, bool isBody);
    bool _checkSliceDirectories( );
    void _setNavigationButtonsEnabled( bool const enabled );
    void _showLayerImage(int const layer);
    void _showLayerImage(const QString &path);
    void _setSliceControlsEnabled( bool const enabled );
    void _updateSliceControls();
    void _updatePrepareButtonState( );
    void _showWarning(const QString& content);
    void _handlePrepareFailed( );
    void _loadDirectoryManifest();
    void _restartPreview();

signals:
    void slicingNeeded(bool const needed);
    void preparePrinterStarted();
    void preparePrinterComplete(bool const success);
    void printerAvailabilityChanged(bool const available);

public slots:
    virtual void tab_uiStateChanged( TabIndex const sender, UiState const state ) override;
    virtual void printJobChanged() override;

    void setPrinterAvailable( bool const value );

private slots:
    void usbMountManager_filesystemMounted( QString const& mountPoint );
    void usbMountManager_filesystemUnmounted( QString const& mountPoint );

    void printer_online( );
    void printer_offline( );
    void printer_temperatureReport( double const bedCurrentTemperature, double const bedTargetTemperature, int const bedPwm );
    void printManager_lampStatusChange( bool const on );

    void layerThickness100Button_clicked( bool );
    void layerThickness50Button_clicked( bool );
    void layerThicknessCustomButton_clicked( bool );
#if defined EXPERIMENTAL
    void layerThickness20Button_clicked( bool );
#endif

    void navigateFirst_clicked( bool );
    void navigatePrevious_clicked( bool );
    void navigateNext_clicked( bool );
    void navigateLast_clicked( bool );

    void sliceButton_clicked( bool );

    void orderButton_clicked( bool );

    void hasher_resultReady( QString const hash );

    void slicingStatusUpdate(const QString &status);
    void renderingStatusUpdate(const QString &status);
    void layerCountUpdate(int count);
    void layerDoneUpdate(int layer, QString path);
    void slicingDone(bool success);

    void prepareButton_clicked( bool );
    void shepherd_homeComplete( bool const success );
    void adjustBuildPlatform_complete( bool );
    void shepherd_raiseBuildPlatformMoveToComplete( bool const success );

    //void copyToUSB_clicked( bool );

};

#endif // __PREPARETAB_H__

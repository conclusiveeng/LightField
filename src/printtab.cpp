#include "pch.h"

#include "printtab.h"

#include "printjob.h"
#include "printmanager.h"
#include "shepherd.h"
#include "ordermanifestmanager.h"
#include "spoiler.h"

namespace {

    QString const RaiseBuildPlatformText { "Raise build platform" };
    QString const LowerBuildPlatformText { "Lower build platform" };

}

PrintTab::PrintTab(QWidget* parent): InitialShowEventMixin<PrintTab, TabBase>(parent) {
#if defined _DEBUG
    _isPrinterPrepared = g_settings.pretendPrinterIsPrepared;
#endif // _DEBUG

    auto boldFont = ModifyFont( font( ), QFont::Bold );

    QObject::connect( _powerLevelSlider, &ParamSlider::valueChanged,   this, &PrintTab::powerLevelSlider_valueChanged   );

    connectBasicExpoTimeCallback( true );

    _powerLevelSlider->innerSlider()->setPageStep( 5 );
    _powerLevelSlider->innerSlider()->setSingleStep( 1 );
    _powerLevelSlider->innerSlider()->setTickInterval( 5 );

    _expoDisabledTilingWarning->setMinimumSize(400, 50);

    _optionsGroup->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    _optionsGroup->setTitle( "Print settings" );

    _printButton->setEnabled( false );
    _printButton->setFixedSize( MainButtonSize );
    _printButton->setFont( ModifyFont( _printButton->font( ), LargeFontSize ) );
    _printButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _printButton->setText( "Continue…" );
    QObject::connect( _printButton, &QPushButton::clicked, this, &PrintTab::printButton_clicked );

    _raiseOrLowerButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _raiseOrLowerButton->setText( RaiseBuildPlatformText );
    QObject::connect( _raiseOrLowerButton, &QPushButton::clicked, this, &PrintTab::raiseOrLowerButton_clicked );

    _homeButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _homeButton->setText( "Home" );
    QObject::connect( _homeButton, &QPushButton::clicked, this, &PrintTab::homeButton_clicked );

    _adjustmentsGroup->setFixedHeight( MainButtonSize.height( ) );
    _adjustmentsGroup->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    _adjustmentsGroup->setLayout( WrapWidgetsInHBox( nullptr, _homeButton, nullptr, _raiseOrLowerButton, nullptr ) );
    _adjustmentsGroup->setTitle( "Adjustments" );


    QScrollArea* advArea = new QScrollArea();

    _basicExpoTimeGroup = new Spoiler("Basic exposure time control");
    _advancedExpoTimeGroup = new Spoiler("Advanced exposure time control");

    QObject::connect(_basicExpoTimeGroup, &Spoiler::collapseStateChanged, [this](bool checked) {
        _advancedExpoTimeGroup->setCollapsed(checked);

        connectBasicExpoTimeCallback(checked);
        connectAdvanExpoTimeCallback(!checked);

        if(checked)
            basicExposureTime_update();
        else
            advancedExposureTime_update();

    });

    QObject::connect(_advancedExpoTimeGroup, &Spoiler::collapseStateChanged, [this](bool checked) {
        _basicExpoTimeGroup->setCollapsed(checked);

        connectBasicExpoTimeCallback(!checked);
        connectAdvanExpoTimeCallback(checked);

        if(!checked)
            basicExposureTime_update();
        else
            advancedExposureTime_update();
    });

    _basicExpoTimeGroup->setCollapsed(false);
    _basicExpoTimeGroup->setContentLayout(
        WrapWidgetsInVBox(
            _bodyExposureTimeSlider,
            _baseExposureTimeSlider
        )
    );

    auto boldFontBigger = ModifyFont( boldFont, 13);
    _advBodyLbl->setFixedWidth(82);
    _advBodyLbl->setFont(boldFontBigger);
    _advBaseLbl->setFixedWidth(82);
    _advBaseLbl->setFont(boldFontBigger);


    QLabel* addition1 {new QLabel("+")};
    QLabel* addition2 {new QLabel("+")};
    QLabel* eq1 {new QLabel("=")};
    QLabel* eq2 {new QLabel("=")};
    QFrame* hr {new QFrame};

    addition1->setFont(boldFontBigger);
    addition2->setFont(boldFontBigger);
    eq1->setFont(boldFontBigger);
    eq1->setFont(boldFontBigger);

    hr->setFrameShape(QFrame::HLine);
    hr->setFrameShadow(QFrame::Sunken);
    hr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hr->setFixedHeight(1);

    _advBodyExpoCorse->setCounterBold(false);
    _advBaseExpoCorse->setCounterBold(false);
    _advBodyExpoFine->setCounterBold(false);
    _advBaseExpoFine->setCounterBold(false);

    _advBodyExpoFine->setFixedWidth(340);
    _advBaseExpoFine->setFixedWidth(340);

    QVBoxLayout* container =
        WrapWidgetsInVBox(
                  WrapWidgetsInHBox(_advBodyExpoCorse, addition1, _advBodyExpoFine, eq1, _advBodyLbl),
                  hr,
                  WrapWidgetsInHBox(_advBaseExpoCorse, addition2, _advBaseExpoFine, eq2, _advBaseLbl)
        );

    _advancedExpoTimeGroup->setCollapsed(true);
    _advancedExpoTimeGroup->setContentLayout(
        container
    );

    _advancedExpoTimeGroup->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    _advancedExpoTimeGroup->setFixedWidth(1000);
    _basicExpoTimeGroup->setFixedWidth(1000);

    QWidget* widget = new QWidget(advArea);
    widget->setLayout( WrapWidgetsInVBox(_basicExpoTimeGroup, _advancedExpoTimeGroup, nullptr));
    widget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    widget->setFixedWidth(1000);

    advArea->setWidget(widget);
    _optionsGroup->setLayout(
        WrapWidgetsInVBox(
              advArea,
              _expoDisabledTilingWarning,
              nullptr
        )
    );

    setLayout(
        WrapWidgetsInVBox(
            _powerLevelSlider,
            _optionsGroup,
            nullptr,
            WrapWidgetsInHBox(_printButton, _adjustmentsGroup)
        )
    );
}

PrintTab::~PrintTab( ) {
    /*empty*/
}

void PrintTab::printJobChanged()
{
    debug( "+ PrintTab::printJobChanged\n" );

    syncFormWithPrintProfile();

    update( );
}

void PrintTab::_connectShepherd( ) {
    QObject::connect( _shepherd, &Shepherd::printer_online,  this, &PrintTab::printer_online  );
    QObject::connect( _shepherd, &Shepherd::printer_offline, this, &PrintTab::printer_offline );
}

void PrintTab::_updateUiState( ) {
    bool isEnabled = _isPrinterOnline && _isPrinterAvailable;

    _optionsGroup      ->setEnabled( isEnabled );
    _printButton       ->setEnabled( isEnabled && _isPrinterPrepared && _isModelRendered );
    _raiseOrLowerButton->setEnabled( isEnabled );
    _homeButton        ->setEnabled( isEnabled );

    update( );
}

void PrintTab::_initialShowEvent( QShowEvent* event ) {
    auto size = maxSize( _raiseOrLowerButton->size( ), _homeButton->size( ) ) + ButtonPadding;

    auto fm = _raiseOrLowerButton->fontMetrics( );
    auto raiseSize = fm.size( Qt::TextSingleLine | Qt::TextShowMnemonic, RaiseBuildPlatformText );
    auto lowerSize = fm.size( Qt::TextSingleLine | Qt::TextShowMnemonic, LowerBuildPlatformText );
    if ( lowerSize.width( ) > raiseSize.width( ) ) {
        size.setWidth( size.width( ) + lowerSize.width( ) - raiseSize.width( ) );
    }

    _raiseOrLowerButton->setFixedSize( size );
    _homeButton        ->setFixedSize( size );

    _baseExposureTimeSlider->setEnabled(printJob.hasExposureControlsEnabled());
    _bodyExposureTimeSlider->setEnabled(printJob.hasExposureControlsEnabled());
    _expoDisabledTilingWarning->setVisible(printJob.isTiled());

    event->accept( );

    update( );
}

void PrintTab::powerLevelSlider_valueChanged()
{
    double value = _powerLevelSlider->getValue();

    PrintParameters& baseParams = printJob.baseLayerParameters();
    PrintParameters& bodyParams = printJob.bodyLayerParameters();

    baseParams.setPowerLevel( value );
    bodyParams.setPowerLevel( value );

    //if value is greater than 80% then set font color to red
    if(value>80) {
        _powerLevelSlider->setFontColor("red");
    } else {
        _powerLevelSlider->setFontColor("white");
    }

    update();
}

void PrintTab::projectorPowerLevel_changed(int percentage)
{
    _powerLevelSlider->setValue(percentage);
    update();
}


void PrintTab::basicExposureTime_update( ) {
    auto& bodyParams = printJob.bodyLayerParameters();
    auto& baseParams = printJob.baseLayerParameters();

    int bodyExpoTime = _bodyExposureTimeSlider->getValue();
    int baseExpoMulpl = _baseExposureTimeSlider->getValue();
    int baseExpoTime = baseExpoMulpl * bodyExpoTime;

    _advBodyExpoCorse->setValue(bodyExpoTime - (bodyExpoTime % 1000));
    _advBodyExpoFine->setValue(bodyExpoTime % 1000);

    _advBaseExpoCorse->setValue(baseExpoTime - (baseExpoTime % 1000));
    _advBaseExpoFine->setValue(baseExpoTime % 1000);

    bodyParams.setLayerExposureTime(bodyExpoTime);
    baseParams.setLayerExposureTime(baseExpoTime);
    printJob.setAdvancedExposureControlsEnabled(false);
    update( );
}

void PrintTab::advancedExposureTime_update( ) {
    auto& bodyParams = printJob.bodyLayerParameters();
    auto& baseParams = printJob.baseLayerParameters();

    int bodyExpoTime = _advBodyExpoCorse->getValue() + _advBodyExpoFine->getValue();
    int baseExpoTime = _advBaseExpoCorse->getValue() + _advBaseExpoFine->getValue();

    _advBodyLbl->setText( QString("%1 s").arg(bodyExpoTime / 1000.0));
    _advBaseLbl->setText( QString("%1 s").arg(baseExpoTime / 1000.0));

    int bodyExpoTimeRounded = round((bodyExpoTime * 4)/1000) * 250;
    int baseMultiplier = baseExpoTime / bodyExpoTimeRounded;

    if(baseMultiplier > 5)
        baseMultiplier = 5;
    else if (baseMultiplier < 1)
        baseMultiplier = 1;

    _bodyExposureTimeSlider->setValue(bodyExpoTimeRounded);
    _baseExposureTimeSlider->setValue(baseMultiplier);

    bodyParams.setLayerExposureTime(bodyExpoTime);
    baseParams.setLayerExposureTime(baseExpoTime);
    printJob.setAdvancedExposureControlsEnabled(true);

    update( );
}

void PrintTab::connectBasicExpoTimeCallback(bool connect) {

    if(connect) {
        QObject::connect( _bodyExposureTimeSlider, &ParamSlider::valueChanged,   this, &PrintTab::basicExposureTime_update   );
        QObject::connect( _baseExposureTimeSlider, &ParamSlider::valueChanged,   this, &PrintTab::basicExposureTime_update   );
    } else {
        QObject::disconnect( _bodyExposureTimeSlider, &ParamSlider::valueChanged,   this, &PrintTab::basicExposureTime_update   );
        QObject::disconnect( _baseExposureTimeSlider, &ParamSlider::valueChanged,   this, &PrintTab::basicExposureTime_update   );
    }
}

void PrintTab::connectAdvanExpoTimeCallback(bool connect) {

    if(connect) {
        QObject::connect( _advBodyExpoCorse, &ParamSlider::valueChanged,   this, &PrintTab::advancedExposureTime_update   );
        QObject::connect( _advBodyExpoFine, &ParamSlider::valueChanged,   this, &PrintTab::advancedExposureTime_update   );
        QObject::connect( _advBaseExpoCorse, &ParamSlider::valueChanged,   this, &PrintTab::advancedExposureTime_update   );
        QObject::connect( _advBaseExpoFine, &ParamSlider::valueChanged,   this, &PrintTab::advancedExposureTime_update   );
    } else {
        QObject::disconnect( _advBodyExpoCorse, &ParamSlider::valueChanged,   this, &PrintTab::advancedExposureTime_update   );
        QObject::disconnect( _advBodyExpoFine, &ParamSlider::valueChanged,   this, &PrintTab::advancedExposureTime_update   );
        QObject::disconnect( _advBaseExpoCorse, &ParamSlider::valueChanged,   this, &PrintTab::advancedExposureTime_update   );
        QObject::disconnect( _advBaseExpoFine, &ParamSlider::valueChanged,   this, &PrintTab::advancedExposureTime_update   );
    }
}


#if defined ENABLE_SPEED_SETTING
void PrintTab::printSpeedSlider_valueChanged( int value ) {
    printJob.printSpeed = value;
    _printSpeedValue->setText( QString( "%1 mm/min" ).arg( value ) );

    update( );
}
#endif // defined ENABLE_SPEED_SETTING

void PrintTab::printButton_clicked( bool ) {
    debug( "+ PrintTab::printButton_clicked\n" );
    //exposureTime_update();
    emit printRequested( );
    emit uiStateChanged( TabIndex::Print, UiState::PrintStarted );

    update( );
}

void PrintTab::raiseOrLowerButton_clicked( bool ) {
    debug( "+ PrintTab::raiseOrLowerButton_clicked: build platform state %s [%d]\n", ToString( _buildPlatformState ), _buildPlatformState );

    switch ( _buildPlatformState ) {
        case BuildPlatformState::Lowered:
        case BuildPlatformState::Raising:
            _buildPlatformState = BuildPlatformState::Raising;

            QObject::connect( _shepherd, &Shepherd::action_moveAbsoluteComplete, this, &PrintTab::raiseBuildPlatform_moveAbsoluteComplete );
            _shepherd->doMoveAbsolute( PrinterRaiseToMaximumZ, PrinterDefaultHighSpeed );
            break;

        case BuildPlatformState::Raised:
        case BuildPlatformState::Lowering:
            _buildPlatformState = BuildPlatformState::Lowering;

            QObject::connect( _shepherd, &Shepherd::action_moveAbsoluteComplete, this, &PrintTab::lowerBuildPlatform_moveAbsoluteComplete );

            _shepherd->doMoveAbsolute(std::max(100, printJob.getLayerThicknessAt(0)) / 1000.0,
                PrinterDefaultHighSpeed);
            break;
    }

    setPrinterAvailable( false );
    emit printerAvailabilityChanged( false );

    update( );
}

void PrintTab::raiseBuildPlatform_moveAbsoluteComplete( bool const success ) {
    debug( "+ PrintTab::raiseBuildPlatform_moveAbsoluteComplete: %s\n", success ? "succeeded" : "failed" );
    QObject::disconnect( _shepherd, &Shepherd::action_moveAbsoluteComplete, this, &PrintTab::raiseBuildPlatform_moveAbsoluteComplete );

    if ( success ) {
        _buildPlatformState = BuildPlatformState::Raised;
        _raiseOrLowerButton->setText( LowerBuildPlatformText );
        _raiseOrLowerButton->setEnabled( true );
    } else {
        _buildPlatformState = BuildPlatformState::Lowered;
    }

    setPrinterAvailable( true );
    emit printerAvailabilityChanged( true );

    update( );
}

void PrintTab::lowerBuildPlatform_moveAbsoluteComplete( bool const success ) {
    debug( "+ PrintTab::lowerBuildPlatform_moveAbsoluteComplete: %s\n", success ? "succeeded" : "failed" );
    QObject::disconnect( _shepherd, &Shepherd::action_moveAbsoluteComplete, this, &PrintTab::lowerBuildPlatform_moveAbsoluteComplete );

    if ( success ) {
        _buildPlatformState = BuildPlatformState::Lowered;
        _raiseOrLowerButton->setText( RaiseBuildPlatformText );
        _raiseOrLowerButton->setEnabled( true );
    } else {
        _buildPlatformState = BuildPlatformState::Raised;
    }

    setPrinterAvailable( true );
    emit printerAvailabilityChanged( true );

    update( );
}

void PrintTab::homeButton_clicked( bool ) {
    debug( "+ PrintTab::homeButton_clicked\n" );

    QObject::connect( _shepherd, &Shepherd::action_homeComplete, this, &PrintTab::home_homeComplete );
    _shepherd->doHome( );

    setPrinterAvailable( false );
    emit printerAvailabilityChanged( false );

    update( );
}

void PrintTab::home_homeComplete( bool const success ) {
    debug( "+ PrintTab::home_homeComplete: %s\n", success ? "succeeded" : "failed" );

    setPrinterAvailable( true );
    emit printerAvailabilityChanged( true );

    update( );
}

void PrintTab::tab_uiStateChanged( TabIndex const sender, UiState const state ) {
    debug( "+ PrintTab::tab_uiStateChanged: from %sTab: %s => %s; PO? %s PA? %s PP? %s MR? %s\n", ToString( sender ), ToString( _uiState ), ToString( state ), YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ), YesNoString( _isPrinterPrepared ), YesNoString( _isModelRendered ) );
    _uiState = state;

    switch (_uiState) {
        case UiState::SelectCompleted:
            enableExpoTimeSliders(!printJob.isTiled());
            if(!printJob.isTiled())
                _expoDisabledTilingWarning->hide();
            else
                _expoDisabledTilingWarning->show();

            break;

        case UiState::PrintJobReady:
            enableExpoTimeSliders(!printJob.isTiled());
            syncFormWithPrintProfile();

            if(!printJob.isTiled()) {
                _expoDisabledTilingWarning->hide();
            } else {
                _expoDisabledTilingWarning->show();
            }

            break;

        case UiState::PrintStarted:
            setPrinterAvailable(false);
            emit printerAvailabilityChanged(false);
            break;

        case UiState::PrintCompleted:
            setPrinterAvailable(true);
            emit printerAvailabilityChanged(true);
            break;

        default:
            break;
    }

    update();
}

void PrintTab::changeExpoTimeSliders()
{
    bool enable = printJob.hasExposureControlsEnabled();
    _baseExposureTimeSlider->setEnabled(enable);
    _bodyExposureTimeSlider->setEnabled(enable);

    if(enable) {
        auto bodyParams = printJob.bodyLayerParameters();
        auto baseParams = printJob.baseLayerParameters();
        bodyParams.setLayerExposureTime(_bodyExposureTimeSlider->getValue());
        baseParams.setLayerExposureTime(_bodyExposureTimeSlider->getValue() *
            _baseExposureTimeSlider->getValue());
    }
}

void PrintTab::printer_online( ) {
    _isPrinterOnline = true;
    debug( "+ PrintTab::printer_online: PO? %s PA? %s PP? %s MR? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ), YesNoString( _isPrinterPrepared ), YesNoString( _isModelRendered ) );

    _updateUiState( );
}

void PrintTab::printer_offline( ) {
    _isPrinterOnline = false;
    debug( "+ PrintTab::printer_offline: PO? %s PA? %s PP? %s MR? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ), YesNoString( _isPrinterPrepared ), YesNoString( _isModelRendered ) );

    _updateUiState( );
}

void PrintTab::setModelRendered( bool const value ) {
    _isModelRendered = value;
    debug( "+ PrintTab::setModelRendered: PO? %s PA? %s PP? %s MR? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ), YesNoString( _isPrinterPrepared ), YesNoString( _isModelRendered ) );

    _updateUiState( );
}

void PrintTab::setPrinterPrepared( bool const value ) {
    _isPrinterPrepared = value;
    debug( "+ PrintTab::setPrinterPrepared: PO? %s PA? %s PP? %s MR? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ), YesNoString( _isPrinterPrepared ), YesNoString( _isModelRendered ) );

    _updateUiState( );
}

void PrintTab::setPrinterAvailable( bool const value ) {
    _isPrinterAvailable = value;
    debug( "+ PrintTab::setPrinterAvailable: PO? %s PA? %s PP? %s MR? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ), YesNoString( _isPrinterPrepared ), YesNoString( _isModelRendered ) );

    _updateUiState( );
}

void PrintTab::activeProfileChanged(QSharedPointer<PrintProfile> newProfile) {
    (void)newProfile;

    syncFormWithPrintProfile();
}

void PrintTab::syncFormWithPrintProfile() {
    int powerLevelValue = printJob.baseLayerParameters().powerLevel();

    _powerLevelSlider->setValue( powerLevelValue );

    if(printJob.isTiled()) {
        connectBasicExpoTimeCallback(false);
        connectAdvanExpoTimeCallback(false);

        _advancedExpoTimeGroup->setCollapsed(true);
        _basicExpoTimeGroup->setCollapsed(true);
    } else {
        auto& bodyParams = printJob.bodyLayerParameters();
        auto& baseParams = printJob.baseLayerParameters();

        int bodyExpoTime = bodyParams.layerExposureTime();
        int baseExpoTime = baseParams.layerExposureTime();

        if(printJob.getAdvancedExposureControlsEnabled()) {
            _advBodyExpoCorse->setValue(bodyExpoTime - (bodyExpoTime % 1000));
            _advBodyExpoFine->setValue(bodyExpoTime % 1000);

            _advBaseExpoCorse->setValue(baseExpoTime - (baseExpoTime % 1000));
            _advBaseExpoFine->setValue(baseExpoTime % 1000);

            _advancedExpoTimeGroup->setCollapsed(false);
            _basicExpoTimeGroup->setCollapsed(true);


            connectBasicExpoTimeCallback(false);
            connectAdvanExpoTimeCallback(true);

            advancedExposureTime_update();
        } else {
            _bodyExposureTimeSlider->setValue(bodyParams.layerExposureTime());
            _baseExposureTimeSlider->setValue(baseParams.layerExposureTime() /
                bodyParams.layerExposureTime());

            _advancedExpoTimeGroup->setCollapsed(true);
            _basicExpoTimeGroup->setCollapsed(false);

            connectBasicExpoTimeCallback(true);
            connectAdvanExpoTimeCallback(false);

            basicExposureTime_update();
        }
    }
}

void PrintTab::enableExpoTimeSliders(bool enable) {
    connectAdvanExpoTimeCallback(enable);
    connectBasicExpoTimeCallback(enable);

    _basicExpoTimeGroup->setEnabled(enable);
    _advancedExpoTimeGroup->setEnabled(enable);

    _baseExposureTimeSlider->setEnabled(enable);
    _bodyExposureTimeSlider->setEnabled(enable);
    _advBaseExpoFine->setEnabled(enable);
    _advBodyExpoFine->setEnabled(enable);
    _advBaseExpoCorse->setEnabled(enable);
    _advBodyExpoCorse->setEnabled(enable);
}

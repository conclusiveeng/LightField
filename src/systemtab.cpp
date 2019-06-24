#include "pch.h"

#include "systemtab.h"

#include "app.h"
#include "shepherd.h"
#include "strings.h"
#include "upgrademanager.h"
#include "upgradeselector.h"
#include "utils.h"
#include "window.h"

namespace {

    QString VersionMessage {
        "<span style='font-size: 22pt;'>%1</span><br>"
        "<span style='font-size: 16pt;'>Version %2</span><br>"
        "<span style='font-size: 12pt;'>Firmware version %3</span><br>"
        "<span>© 2019 %4</span>"
    };

}

SystemTab::SystemTab( QWidget* parent ): InitialShowEventMixin<SystemTab, TabBase>( parent ) {
    auto origFont = font( );
    auto font16pt = ModifyFont( origFont, 18.0 );
    auto font22pt = ModifyFont( origFont, LargeFontSize );

    //
    // Main content
    //

    _logoLabel->setAlignment( Qt::AlignCenter );
    _logoLabel->setContentsMargins( { } );
    _logoLabel->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::MinimumExpanding );
    _logoLabel->setPixmap( QPixmap { QString { ":images/transparent-dark-logo.png" } } );

    _versionLabel->setAlignment( Qt::AlignCenter );
    _versionLabel->setContentsMargins( { } );
    _versionLabel->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::MinimumExpanding );
    _versionLabel->setTextFormat( Qt::RichText );
    _versionLabel->setText(
        VersionMessage
        .arg( QCoreApplication::applicationName( )    )
        .arg( QCoreApplication::applicationVersion( ) )
        .arg( "(unknown)" )
        .arg( QCoreApplication::organizationName( )   )
    );

    auto versionInfoLayout = WrapWidgetsInHBox( { nullptr, _logoLabel, _versionLabel, nullptr } );
    versionInfoLayout->setContentsMargins( { } );


    _copyrightsLabel->setAlignment( Qt::AlignCenter );
    _copyrightsLabel->setTextFormat( Qt::RichText );
    _copyrightsLabel->setText( ReadWholeFile( ":text/copyright-message.txt" ) );


    _updateSoftwareButton->setEnabled( false );
    _updateSoftwareButton->setFont( font16pt );
    _updateSoftwareButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _updateSoftwareButton->setText( "Update software" );
    QObject::connect( _updateSoftwareButton, &QPushButton::clicked, this, &SystemTab::updateSoftwareButton_clicked );

    _updateFirmwareButton->setEnabled( false );
    _updateFirmwareButton->setFont( font16pt );
    _updateFirmwareButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _updateFirmwareButton->setText( "Update firmware" );
    QObject::connect( _updateFirmwareButton, &QPushButton::clicked, this, &SystemTab::updateFirmwareButton_clicked );

    auto updateButtonsLayout = WrapWidgetsInHBox( { nullptr, _updateSoftwareButton, nullptr, _updateFirmwareButton, nullptr } );
    updateButtonsLayout->setContentsMargins( { } );


    _restartButton->setFont( font16pt );
    _restartButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _restartButton->setText( "Restart" );
    QObject::connect( _restartButton, &QPushButton::clicked, this, &SystemTab::restartButton_clicked );

    _shutDownButton->setFont( font16pt );
    _shutDownButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _shutDownButton->setText( "Shut down" );
    QObject::connect( _shutDownButton, &QPushButton::clicked, this, &SystemTab::shutDownButton_clicked );

    auto mainButtonsLayout = WrapWidgetsInHBox( { nullptr, _restartButton, nullptr, _shutDownButton, nullptr } );
    mainButtonsLayout->setContentsMargins( { } );

    //
    // Top level
    //

    _layout->addLayout( versionInfoLayout );
    _layout->addWidget( _copyrightsLabel );
    _layout->addStretch( );
    _layout->addLayout( updateButtonsLayout );
    _layout->addStretch( );
    _layout->addLayout( mainButtonsLayout );
    _layout->setAlignment( Qt::AlignCenter );
    _layout->setContentsMargins( { } );
    setLayout( _layout );
}

SystemTab::~SystemTab( ) {
    /*empty*/
}

void SystemTab::_connectShepherd( ) {
    if ( _shepherd ) {
        QObject::connect( _shepherd, &Shepherd::printer_online,                this, &SystemTab::printer_online                 );
        QObject::connect( _shepherd, &Shepherd::printer_offline,               this, &SystemTab::printer_offline                );
        QObject::connect( _shepherd, &Shepherd::printer_firmwareVersionReport, this, &SystemTab::shepherd_firmwareVersionReport );
    }
}

void SystemTab::_initialShowEvent( QShowEvent* event ) {
    QSize newSize = maxSize( maxSize( maxSize( _updateSoftwareButton->size( ), _updateFirmwareButton->size( ) ), _restartButton->size( ) ), _shutDownButton->size( ) ) + QSize { 20, 4 };

    _updateSoftwareButton->setFixedSize( newSize );
    _updateFirmwareButton->setFixedSize( newSize );
    _restartButton       ->setFixedSize( newSize );
    _shutDownButton      ->setFixedSize( newSize );

    event->accept( );

    update( );
}

void SystemTab::_updateButtons( ) {
    _updateSoftwareButton->setEnabled( _isSoftwareUpgradeAvailable && _isPrinterAvailable                     );
    _updateFirmwareButton->setEnabled( _isFirmwareUpgradeAvailable && _isPrinterAvailable && _isPrinterOnline );
    _restartButton       ->setEnabled(                                _isPrinterAvailable                     );
    _shutDownButton      ->setEnabled(                                _isPrinterAvailable                     );

    update( );
}

bool SystemTab::_yesNoPrompt( QString const& title, QString const& text ) {
    QMessageBox messageBox { this };
    messageBox.setIcon( QMessageBox::Question );
    messageBox.setText( title );
    messageBox.setInformativeText( text );
    messageBox.setStandardButtons( QMessageBox::Yes | QMessageBox::No );
    messageBox.setDefaultButton( QMessageBox::No );
    messageBox.setFont( ModifyFont( messageBox.font( ), 16.0 ) );

    auto mainWindow = getMainWindow( );
    mainWindow->hide( );
    auto result = static_cast<QMessageBox::StandardButton>( messageBox.exec( ) );
    mainWindow->show( );

    return ( result == QMessageBox::Yes );
}

void SystemTab::tab_uiStateChanged( TabIndex const sender, UiState const state ) {
    debug( "+ SystemTab::tab_uiStateChanged: from %sTab: %s => %s\n", ToString( sender ), ToString( _uiState ), ToString( state ) );
    _uiState = state;

    switch ( _uiState ) {
        case UiState::SelectStarted:
        case UiState::SelectCompleted:
        case UiState::SliceStarted:
        case UiState::SliceCompleted:
        case UiState::PrintCompleted:
            setPrinterAvailable( true );
            break;

        case UiState::PrintStarted:
            setPrinterAvailable( false );
            break;
    }
}

void SystemTab::shepherd_firmwareVersionReport( QString const& version ) {
    _versionLabel->setText(
        VersionMessage
        .arg( QCoreApplication::applicationName( )    )
        .arg( QCoreApplication::applicationVersion( ) )
        .arg( version )
        .arg( QCoreApplication::organizationName( )   )
    );
}

void SystemTab::upgradeManager_upgradeCheckComplete( bool const upgradesFound ) {
    _isSoftwareUpgradeAvailable = upgradesFound;
    debug( "+ SystemTab::upgradeManager_upgradeCheckComplete: upgrades available? %s PO? %s PA? %s\n", YesNoString( _isSoftwareUpgradeAvailable ), YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ) );

    _updateButtons( );
}

void SystemTab::upgradeSelector_canceled( ) {
    debug( "+ SystemTab::upgradeSelector_canceled\n" );

    auto mainWindow = getMainWindow( );
    mainWindow->show( );

    _upgradeSelector->close( );
    _upgradeSelector->deleteLater( );
    _upgradeSelector = nullptr;

    setPrinterAvailable( true );
    emit printerAvailabilityChanged( true );
}

void SystemTab::upgradeSelector_kitSelected( UpgradeKitInfo const& kit ) {
    debug( "+ SystemTab::upgradeSelector_kitSelected: version %s (%s)\n", kit.versionString.toUtf8( ).data( ), ToString( kit.buildType ) );

    setPrinterAvailable( false );
    emit printerAvailabilityChanged( false );

    _upgradeSelector->showInProgressMessage( );

    _upgradeManager->installUpgradeKit( kit );
}

void SystemTab::upgradeManager_upgradeFailed( ) {
    debug( "+ SystemTab::upgradeManager_upgradeFailed\n" );

    _upgradeSelector->showFailedMessage( );
}

void SystemTab::setPrinterAvailable( bool const value ) {
    _isPrinterAvailable = value;
    debug( "+ SystemTab::setPrinterAvailable: PO? %s PA? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ) );

    _updateButtons( );
}

void SystemTab::printer_online( ) {
    _isPrinterOnline = true;
    debug( "+ SystemTab::printer_online: PO? %s PA? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ) );

    _updateButtons( );
}

void SystemTab::printer_offline( ) {
    _isPrinterOnline = false;
    debug( "+ SystemTab::printer_offline: PO? %s PA? %s\n", YesNoString( _isPrinterOnline ), YesNoString( _isPrinterAvailable ) );

    _updateButtons( );
}

void SystemTab::updateSoftwareButton_clicked( bool ) {
    debug( "+ SystemTab::updateSoftwareButton_clicked\n" );

    _upgradeSelector = new UpgradeSelector( _upgradeManager, this );
    QObject::connect( _upgradeSelector, &UpgradeSelector::canceled,    this, &SystemTab::upgradeSelector_canceled    );
    QObject::connect( _upgradeSelector, &UpgradeSelector::kitSelected, this, &SystemTab::upgradeSelector_kitSelected );
    _upgradeSelector->show( );

    auto mainWindow = getMainWindow( );
    mainWindow->hide( );
}

void SystemTab::updateFirmwareButton_clicked( bool ) {
    debug( "+ SystemTab::updateFirmwareButton_clicked\n" );
}

void SystemTab::restartButton_clicked( bool ) {
    if ( _yesNoPrompt( "Confirm", "Are you sure you want to restart?" ) ) {
        system( "sudo systemctl reboot" );
    }
}

void SystemTab::shutDownButton_clicked( bool ) {
    if ( _yesNoPrompt( "Confirm", "Are you sure you want to shut down?" ) ) {
        system( "sudo systemctl poweroff" );
    }
}

void SystemTab::setUpgradeManager( UpgradeManager* upgradeManager ) {
    if ( upgradeManager ) {
        _upgradeManager = upgradeManager;
        QObject::connect( _upgradeManager, &UpgradeManager::upgradeCheckComplete, this, &SystemTab::upgradeManager_upgradeCheckComplete );
        QObject::connect( _upgradeManager, &UpgradeManager::upgradeFailed,        this, &SystemTab::upgradeManager_upgradeFailed        );
    } else {
        QObject::disconnect( _upgradeManager, nullptr, this, nullptr );
        _upgradeManager = nullptr;
    }
}
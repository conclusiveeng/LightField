#include "pch.h"

#include "selecttab.h"

#include "canvas.h"
#include "loader.h"
#include "mesh.h"
#include "printjob.h"
#include "processrunner.h"
#include "shepherd.h"
#include "strings.h"
#include "utils.h"

namespace {

    QString DefaultModelFileName { ":gl/BoundingBox.stl" };

    QRegularExpression VolumeLineMatcher { QString { "^volume\\s*=\\s*(\\d+(?:\\.(?:\\d+))?)" }, QRegularExpression::CaseInsensitiveOption };

}

SelectTab::SelectTab( QWidget* parent ): QWidget( parent ) {
    debug( "+ SelectTab::`ctor: construct at %p\n", this );

    _currentFsModel = _libraryFsModel;

    _libraryFsModel->setFilter( QDir::Files );
    _libraryFsModel->setNameFilterDisables( false );
    _libraryFsModel->setNameFilters( { { "*.stl" } } );
    _libraryFsModel->setRootPath( StlModelLibraryPath );
    QObject::connect( _libraryFsModel, &QFileSystemModel::directoryLoaded, this, &SelectTab::libraryFsModel_directoryLoaded );

    _usbFsModel->setFilter( QDir::Drives | QDir::Files );
    _usbFsModel->setNameFilterDisables( false );
    _usbFsModel->setNameFilters( { { "*.stl" } } );
    QObject::connect( _usbFsModel, &QFileSystemModel::directoryLoaded, this, &SelectTab::usbFsModel_directoryLoaded );

    QObject::connect( _fsWatcher, &QFileSystemWatcher::directoryChanged, this, &SelectTab::_lookForUsbStick );
    _fsWatcher->addPath( UsbStickPath );
    _lookForUsbStick( QString( UsbStickPath ) );

    _availableFilesLabel->setText( "Models in library:" );

    _availableFilesListView->setFlow( QListView::TopToBottom );
    _availableFilesListView->setLayoutMode( QListView::SinglePass );
    _availableFilesListView->setMovement( QListView::Static );
    _availableFilesListView->setResizeMode( QListView::Fixed );
    _availableFilesListView->setViewMode( QListView::ListMode );
    _availableFilesListView->setWrapping( true );
    _availableFilesListView->setModel( _libraryFsModel );
    QObject::connect( _availableFilesListView, &QListView::clicked, this, &SelectTab::availableFilesListView_clicked );

    _toggleLocationButton->setText( "Show USB stick" );
    QObject::connect( _toggleLocationButton, &QPushButton::clicked, this, &SelectTab::toggleLocationButton_clicked );

    _availableFilesLayout->setContentsMargins( { } );
    _availableFilesLayout->addWidget( _availableFilesLabel,    0, 0 );
    _availableFilesLayout->addWidget( _availableFilesListView, 1, 0 );
    _availableFilesLayout->addWidget( _toggleLocationButton,   2, 0 );

    _availableFilesContainer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    _availableFilesContainer->setLayout( _availableFilesLayout );

    _selectButton->setFont( ModifyFont( _selectButton->font( ), 22.0f ) );
    _selectButton->setText( "Select" );
    _selectButton->setEnabled( false );
    _selectButton->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::MinimumExpanding );
    QObject::connect( _selectButton, &QPushButton::clicked, this, &SelectTab::selectButton_clicked );

    QSurfaceFormat format;
    format.setDepthBufferSize( 24 );
    format.setStencilBufferSize( 8 );
    format.setVersion( 2, 1 );
    format.setProfile( QSurfaceFormat::CoreProfile );
    QSurfaceFormat::setDefaultFormat( format );

    _canvas = new Canvas( format, this );
    _canvas->setMinimumWidth( MaximalRightHandPaneSize.width( ) );
    _canvas->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    {
        auto pal = _dimensionsErrorLabel->palette( );
        pal.setColor( QPalette::WindowText, Qt::red );
        _dimensionsErrorLabel->setPalette( pal );
    }
    _dimensionsErrorLabel->setText( "Model exceeds build volume!" );
    _dimensionsErrorLabel->hide( );

    _dimensionsLayout->setContentsMargins( { } );
    _dimensionsLayout->setAlignment( Qt::AlignLeft );
    _dimensionsLayout->addWidget( _dimensionsLabel );
    _dimensionsLayout->addStretch( );
    _dimensionsLayout->addWidget( _dimensionsErrorLabel );

    _canvasLayout->setContentsMargins( { } );
    _canvasLayout->addWidget( _canvas );
    _canvasLayout->addLayout( _dimensionsLayout );

    _layout->setContentsMargins( { } );
    _layout->addWidget( _availableFilesContainer, 0, 0, 1, 1 );
    _layout->addWidget( _selectButton,            1, 0, 1, 1 );
    _layout->addLayout( _canvasLayout,            0, 1, 2, 1 );
    _layout->setRowStretch( 0, 4 );
    _layout->setRowStretch( 1, 1 );

    setLayout( _layout );

    _loadModel( DefaultModelFileName );
}

SelectTab::~SelectTab( ) {
    debug( "+ SelectTab::`dtor: destruct at %p\n", this );
}

void SelectTab::libraryFsModel_directoryLoaded( QString const& name ) {
    debug( "+ SelectTab::libraryFsModel_directoryLoaded: name '%s'\n", name.toUtf8( ).data( ) );
    if ( _modelsLocation == ModelsLocation::Library ) {
        _libraryFsModel->sort( 0, Qt::AscendingOrder );
        _availableFilesListView->setRootIndex( _libraryFsModel->index( StlModelLibraryPath ) );
    }
}

void SelectTab::usbFsModel_directoryLoaded( QString const& name ) {
    debug( "+ SelectTab::usbFsModel_directoryLoaded: name '%s'\n", name.toUtf8( ).data( ) );
    if ( _modelsLocation == ModelsLocation::Usb ) {
        _usbFsModel->sort( 0, Qt::AscendingOrder );
        _availableFilesListView->setRootIndex( _usbFsModel->index( _usbPath ) );
    }
}

void SelectTab::_lookForUsbStick( QString const& path ) {
    debug( "+ SelectTab::_lookForUsbStick: name '%s'\n", path.toUtf8( ).data( ) );

    auto dir = new QDir( path );
    dir->setFilter( QDir::Dirs );
    auto names = dir->entryList( );
    QString dirname;
    for ( auto name : names ) {
        if ( ( name == "." ) || ( name == ".." ) ) {
            continue;
        }
        dirname = name;
        break;
    }
    if ( dirname.isEmpty( ) ) {
        debug( "  + no directories on USB stick\n" );
        _showLibrary( );
        _toggleLocationButton->setEnabled( false );
    } else {
        debug( "  + USB path is '%s/%s'\n", UsbStickPath.toUtf8( ).data( ), dirname.toUtf8( ).data( ) );
        _usbPath = UsbStickPath + QString( "/" ) + dirname;
        _usbFsModel->setRootPath( _usbPath );
        _toggleLocationButton->setEnabled( true );
    }
}

void SelectTab::availableFilesListView_clicked( QModelIndex const& index ) {
    auto fileName = ( ( _modelsLocation == ModelsLocation::Library ) ? StlModelLibraryPath : _usbPath ) + QString( "/" ) + index.data( ).toString( );
    int indexRow = index.row( );
    debug( "+ SelectTab::availableFilesListView_clicked: row %d, file name '%s'\n", indexRow, fileName.toUtf8( ).data( ) );
    if ( _selectedRow != indexRow ) {
        _fileName = fileName;
        _selectedRow = indexRow;
        _selectButton->setEnabled( false );
        _availableFilesListView->setEnabled( false );
        if ( !_loadModel( _fileName ) ) {
            debug( "  + _loadModel failed!\n" );
            _availableFilesListView->setEnabled( true );
        }
    }
}

void SelectTab::toggleLocationButton_clicked( bool ) {
    if ( _modelsLocation == ModelsLocation::Library ) {
        _showUsbStick( );
    } else {
        _showLibrary( );
    }
}

void SelectTab::selectButton_clicked( bool ) {
    debug( "+ SelectTab::selectButton_clicked\n" );
    emit modelSelected( true, _fileName );
}

void SelectTab::loader_gotMesh( Mesh* m ) {
    float minX, minY, minZ, maxX, maxY, maxZ;
    size_t count;

    m->bounds( count, minX, minY, minZ, maxX, maxY, maxZ );
    float sizeX  = maxX - minX;
    float sizeY  = maxY - minY;
    float sizeZ  = maxZ - minZ;
    debug(
        "+ SelectTab::loader_gotMesh:\n"
        "  + count of vertices: %5zu\n"
        "  + X range:           %12.6f .. %12.6f, %12.6f\n"
        "  + Y range:           %12.6f .. %12.6f, %12.6f\n"
        "  + Z range:           %12.6f .. %12.6f, %12.6f\n"
        "",
        count,
        minX, maxX, sizeX,
        minY, maxY, sizeY,
        minZ, maxZ, sizeZ
    );

    {
        auto sizeXstring = GroupDigits( QString( "%1" ).arg( sizeX, 0, 'f', 2 ), ' ' );
        auto sizeYstring = GroupDigits( QString( "%1" ).arg( sizeY, 0, 'f', 2 ), ' ' );
        auto sizeZstring = GroupDigits( QString( "%1" ).arg( sizeZ, 0, 'f', 2 ), ' ' );
        _dimensionsLabel->setText( QString( "%1 mm × %2 mm × %3 mm" ).arg( sizeXstring ).arg( sizeYstring ).arg( sizeZstring ) );
    }

    if ( ( sizeX > PrinterMaximumX ) || ( sizeY > PrinterMaximumY ) || ( sizeZ > PrinterMaximumZ ) ) {
        _dimensionsErrorLabel->show( );
        _selectButton->setEnabled( false );
    } else {
        _dimensionsErrorLabel->hide( );
        _selectButton->setEnabled( true );
    }

    _canvas->load_mesh( m );

    if ( _processRunner ) {
        QObject::disconnect( _processRunner, nullptr, this, nullptr );
        _processRunner->kill( );
        _processRunner->deleteLater( );
    }

    _processRunner = new ProcessRunner( this );
    QObject::connect( _processRunner, &ProcessRunner::succeeded,               this, &SelectTab::processRunner_succeeded               );
    QObject::connect( _processRunner, &ProcessRunner::failed,                  this, &SelectTab::processRunner_failed                  );
    QObject::connect( _processRunner, &ProcessRunner::readyReadStandardOutput, this, &SelectTab::processRunner_readyReadStandardOutput );
    QObject::connect( _processRunner, &ProcessRunner::readyReadStandardError,  this, &SelectTab::processRunner_readyReadStandardError  );

    if ( !_fileName.isEmpty( ) && ( _fileName[0].unicode( ) != L':' ) ) {
        debug( "+ SelectTab::loader_gotMesh: file name '%s'\n", _fileName.toUtf8( ).data( ) );
        _processRunner->start(
            { "slic3r" },
        {
            { "--info"  },
            { _fileName }
        }
        );
    }

    emit modelDimensioned( count, { minX, maxX }, { minY, maxY }, { minZ, maxZ } );
}

void SelectTab::loader_ErrorBadStl( ) {
    debug( "+ SelectTab::loader_ErrorBadStl\n" );
    QMessageBox::critical( this, "Error",
        "<b>Error:</b><br>"
        "This <code>.stl</code> file is invalid or corrupted.<br>"
        "Please export it from the original source, verify, and retry."
    );
    emit modelSelected( false, _fileName );
}

void SelectTab::loader_ErrorEmptyMesh( ) {
    debug( "+ SelectTab::loader_ErrorEmptyMesh\n" );
    QMessageBox::critical( this, "Error",
        "<b>Error:</b><br>"
        "This file is syntactically correct<br>but contains no triangles."
    );
    emit modelSelected( false, _fileName );
}

void SelectTab::loader_ErrorMissingFile( ) {
    debug( "+ SelectTab::loader_ErrorMissingFile\n" );
    QMessageBox::critical( this, "Error",
        "<b>Error:</b><br>"
        "The target file is missing.<br>"
    );
    emit modelSelected( false, _fileName );
}

void SelectTab::loader_Finished( ) {
    debug( "+ SelectTab::loader_Finished\n" );
    _availableFilesListView->setEnabled( true );
    _canvas->clear_status( );
    _loader->deleteLater( );
    _loader = nullptr;
}

void SelectTab::loader_LoadedFile( const QString& fileName ) {
    debug( "+ SelectTab::loader_LoadedFile: fileName: '%s'\n", fileName.toUtf8( ).data( ) );
}

void SelectTab::processRunner_succeeded( ) {
    debug( "+ SelectTab::processRunner_succeeded\n" );

    for ( auto line : _slicerBuffer.split( QRegularExpression { QString { "\\r?\\n" } } ) ) {
        auto match = VolumeLineMatcher.match( line );
        if ( match.hasMatch( ) ) {
            auto value = match.captured( 1 ).toDouble( );
            _dimensionsLabel->setText( _dimensionsLabel->text( ) + QString( "  •  %4 mL" ).arg( value, 0, 'f', 2 ) );
        }
    }

    _slicerBuffer.clear( );
}

void SelectTab::processRunner_failed( QProcess::ProcessError const error ) {
    debug( "SelectTab::processRunner_failed: error %s [%d]\n", ToString( error ), error );
    _slicerBuffer.clear( );
}

void SelectTab::processRunner_readyReadStandardOutput( QString const& data ) {
    debug( "+ SelectTab::processRunner_readyReadStandardOutput: %d bytes from slic3r\n", data.length( ) );
    _slicerBuffer += data;
}

void SelectTab::processRunner_readyReadStandardError( QString const& data ) {
    debug(
        "+ SelectTab::processRunner_readyReadStandardError: %d bytes from slic3r:\n"
        "%s",
        data.length( ),
        data.toUtf8( ).data( )
    );
}

bool SelectTab::_loadModel( QString const& fileName ) {
    debug( "+ SelectTab::_loadModel: fileName: '%s'\n", fileName.toUtf8( ).data( ) );
    if ( _loader ) {
        debug( "+ SelectTab::_loadModel: loader object exists, not loading\n" );
        return false;
    }

    _canvas->set_status( QString( "Loading " ) + getFileBaseName( fileName ) );

    _loader = new Loader( fileName, this );
    connect( _loader, &Loader::got_mesh,           this,    &SelectTab::loader_gotMesh          );
    connect( _loader, &Loader::error_bad_stl,      this,    &SelectTab::loader_ErrorBadStl      );
    connect( _loader, &Loader::error_empty_mesh,   this,    &SelectTab::loader_ErrorEmptyMesh   );
    connect( _loader, &Loader::error_missing_file, this,    &SelectTab::loader_ErrorMissingFile );
    connect( _loader, &Loader::finished,           this,    &SelectTab::loader_Finished         );

    if ( fileName[0] != ':' ) {
        connect( _loader, &Loader::loaded_file,    this,    &SelectTab::loader_LoadedFile       );
    }

    _selectButton->setEnabled( false );
    _loader->start( );
    return true;
}

void SelectTab::_showLibrary( ) {
    _modelsLocation = ModelsLocation::Library;
    _currentFsModel = _libraryFsModel;

    _libraryFsModel->sort( 0, Qt::AscendingOrder );
    _availableFilesLabel->setText( "Models in library:" );
    _availableFilesListView->setModel( _libraryFsModel );
    _availableFilesListView->setRootIndex( _libraryFsModel->index( StlModelLibraryPath ) );
    _toggleLocationButton->setText( "Show USB stick" );
}

void SelectTab::_showUsbStick( ) {
    _modelsLocation = ModelsLocation::Usb;
    _currentFsModel = _usbFsModel;

    _usbFsModel->sort( 0, Qt::AscendingOrder );
    _availableFilesLabel->setText( "Models on USB stick:" );
    _availableFilesListView->setModel( _usbFsModel );
    _availableFilesListView->setRootIndex( _usbFsModel->index( _usbPath ) );
    _toggleLocationButton->setText( "Show library" );
}

void SelectTab::setPrintJob( PrintJob* printJob ) {
    debug( "+ StatusTab::setPrintJob: printJob %p\n", printJob );
    _printJob = printJob;
}

void SelectTab::setShepherd( Shepherd* newShepherd ) {
    _shepherd = newShepherd;
}
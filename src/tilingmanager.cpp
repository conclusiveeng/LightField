
#include "tilingmanager.h"


TilingManager::TilingManager( OrderManifestManager* manifestMgr, PrintJob* printJob )
{
    _manifestMgr = manifestMgr;
    _printJob = printJob;
}

void TilingManager::processImages( int width, int height, int expoTime, int step, double space )
{
    debug( "+ TilingManager::processImages\n");

    _width = width;
    _height = height;
    _expoTime = expoTime;
    _step = step;
    _space = space;

    QString jobWorkingDir = _printJob->jobWorkingDirectory;

    QString dirName = QString("tiled-%1-%2-%3-").arg(_expoTime).arg(_step).arg(_space, 0, 'g', 3) % GetFileBaseName( jobWorkingDir );
    _path = jobWorkingDir.mid( 0, jobWorkingDir.lastIndexOf( Slash ) ) % Slash % dirName;

    QDir dir ( JobWorkingDirectoryPath );
    dir.mkdir( dirName );

    tileImages( );

    QFile::link( _path , StlModelLibraryPath % Slash % dirName );

    _manifestMgr->setFileList( _fileNameList );
    _manifestMgr->setPath( JobWorkingDirectoryPath % Slash % dirName );
    _manifestMgr->save();
}

void TilingManager::tileImages ( )
{
    debug( "+ TilingManager::tileImages\n");

    QPixmap pixmap;
    pixmap.load(_printJob->jobWorkingDirectory % Slash % _manifestMgr->getFirstElement());

    /* calculating tiled images count in rows and columns */
    _wCount = floor( _width / (pixmap.width() + pixmap.width() * _space ) );
    _hCount =  floor( _height / (pixmap.height() + pixmap.height() * _space ) );

    debug( "+ TilingManager::renderTiles _width %d, _height %d, pixmap.width %d, pixmap.height %d, _space %f \n",
                                         _width,    _height,    pixmap.width(),  pixmap.height(),  _space);

    _counter = 0;

    OrderManifestManager::Iterator iter = _manifestMgr->iterator();


    /* iterating over slices in manifest */
    while ( iter.hasNext() ) {
        QFileInfo entry ( _printJob->jobWorkingDirectory % Slash % *iter);
        ++iter;

        /* render tiles based on slice */
        renderTiles ( entry );
    }
}

void TilingManager::renderTiles ( QFileInfo info ) {
    int overalCount = _wCount * _hCount; // overal count of tiles

    /* interating over each exposure time */
    for ( int e = 1; e <= overalCount; ++e)
    {
        /* pixmap of single tile */
        QPixmap pixmap ( _width, _height );
        QPainter painter ( &pixmap );
        painter.fillRect(0,0, _width, _height, QBrush("#000000"));

        int innerCount=0;
        /* iterating over rows and columns */
        for( int r=0; (r<_wCount) && (innerCount<e); ++r )
        {
            for( int c=0; (c<_hCount) && (innerCount<e); ++c )
            {
                debug( "+ TilingManager::renderTiles overalCount %d, e %d, innerCount %d, r %d, c %d \n",
                                                    overalCount,    e,    innerCount,    r,    c);
                QPixmap sprite;

                debug( "+ TilingManager::renderTiles path %s\n", info.filePath().toUtf8().data() );
                sprite.load( info.filePath() );
                putImageAt ( sprite, &painter, r, c );

                ++innerCount;
            }
        }

        QString filename = QString( "%1/%2.png" ).arg( _path ).arg( _counter, 6, 10, DigitZero );
        debug( "+ TilingManager::tileImages save %s\n", filename.toUtf8().data());

        QFile file ( filename );
        file.open(QIODevice::WriteOnly);
        pixmap.save( &file, "PNG" );

        _fileNameList.push_back( GetFileBaseName( filename ) );

        _counter++;
    }
}

void TilingManager::putImageAt ( QPixmap pixmap, QPainter* painter, int i, int j ) {

    int x = ( pixmap.width( ) * _space) + ( pixmap.width( ) * i ) + ( pixmap.width( ) * _space * i );
    int y = ( pixmap.height( ) * _space) + ( pixmap.height( ) * j )  + ( pixmap.height( ) * _space * j );

    debug( "+ TilingManager::renderTiles x %d, y %d \n", x, y);

    painter->drawPixmap( x, y, pixmap );
}

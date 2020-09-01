#ifndef __PNGDISPLAYER_H__
#define __PNGDISPLAYER_H__

#include <QtCore>
#include <QtWidgets>

class PngDisplayer: public QMainWindow
{
    Q_OBJECT

public:

    PngDisplayer( QWidget* parent = nullptr );
    virtual ~PngDisplayer( ) override;

protected:

    virtual void closeEvent( QCloseEvent* event ) override;

private:

    QLabel* _label { new QLabel };
    QImage  image;
signals:

    void terminationRequested( );

public slots:

    void clear( );
    bool loadImageFile( QString const& fileName );
    void setPixmap( QPixmap const& pixmap );
    void printJobChanged();
    void moveToOffset(QPoint offset);

protected slots:

private slots:

};

#endif // __PNGDISPLAYER_H__

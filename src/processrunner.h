#ifndef __PROCESSRUNNER_H__
#define __PROCESSRUNNER_H__

#include <QProcess>
#include <QString>
#include <QStringList>

class ProcessRunner: public QObject {

    Q_OBJECT

public:

    ProcessRunner( );
    virtual ~ProcessRunner( ) override;

    void setProgram( QString const& program );
    void setArguments( QStringList const& arguments );
    void start( QProcess::OpenMode const mode = QProcess::ReadWrite );

    QString program( ) const {
        return _process.program( );
    }

    QStringList arguments( ) const {
        return _process.arguments( );
    }

protected:

private:

    QProcess _process;

signals:

    void succeeded( );
    void failed( QProcess::ProcessError const error );

public slots:

protected slots:

private slots:

    void processErrorOccurred( QProcess::ProcessError error );
    void processStarted( );
    void processStateChanged( QProcess::ProcessState newState );
    void processFinished( int exitCode, QProcess::ExitStatus exitStatus );

};

#endif // __PROCESSRUNNER_H__
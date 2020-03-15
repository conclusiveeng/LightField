#include "pch.h"

#include "shepherd.h"

#include "processrunner.h"
#include "window.h"

namespace {

    QRegularExpression FirmwareVersionMatcher    { "^echo:.*?Author:\\s*(.+?)(?:\\s|;|$)",                                                                                    QRegularExpression::CaseInsensitiveOption };
    QRegularExpression PositionReportMatcher     { "^X:(-?\\d+\\.\\d\\d) Y:(-?\\d+\\.\\d\\d) Z:(-?\\d+\\.\\d\\d) E:(-?\\d+\\.\\d\\d) Count X:(-?\\d+) Y:(-?\\d+) Z:(-?\\d+)", QRegularExpression::CaseInsensitiveOption };
    QRegularExpression TemperatureReport1Matcher { "^T:(-?\\d+\\.\\d\\d)\\s*/(-?\\d+\\.\\d\\d) B:(-?\\d+\\.\\d\\d)\\s*/(-?\\d+\\.\\d\\d) @:(-?\\d+) B@:(-?\\d+)",             QRegularExpression::CaseInsensitiveOption };
    QRegularExpression TemperatureReport2Matcher { "^T:(-?\\d+\\.\\d\\d)\\s*/(-?\\d+\\.\\d\\d) @:(-?\\d+)",                                                                   QRegularExpression::CaseInsensitiveOption };

}

Shepherd::Shepherd( QObject* parent ): QObject( parent ) {
    /*empty*/
}

Shepherd::~Shepherd( ) {
    /*empty*/
}

void Shepherd::process_errorOccurred( QProcess::ProcessError error ) {
    debug( "+ Shepherd::process_errorOccurred: error %s [%d]\n", ToString( error ), error );
    if ( error == QProcess::FailedToStart ) {
        debug( "+ Shepherd::process_errorOccurred: process failed to start\n" );
        emit shepherd_startFailed( );
    }
}

void Shepherd::process_started( ) {
    debug( "+ Shepherd::process_started\n" );
    emit shepherd_started( );

#if defined _DEBUG
    if ( g_settings.pretendPrinterIsOnline ) {
        emit printer_online( );
    }
#endif // defined _DEBUG
}

void Shepherd::process_readyReadStandardError( ) {
    _process->setReadChannel( QProcess::StandardError );

    QString input = _process->readAllStandardError( );
    if ( input.length( ) ) {
        debug(
            "+ Shepherd::process_readyReadStandardError: from stderr:\n"
            "%s\n",
            input.toUtf8( ).data( )
        );
    }
}

void Shepherd::process_readyReadStandardOutput( ) {
    _process->setReadChannel( QProcess::StandardOutput );
    if ( auto input = _process->readAllStandardOutput( ); !input.isEmpty( ) ) {
        handleInput( input );
    }
}

void Shepherd::process_finished( int exitCode, QProcess::ExitStatus exitStatus ) {
    debug( "+ Shepherd::process_finished: exitCode: %d, exitStatus: %s [%d]\n", exitCode, ToString( exitStatus ), exitStatus );
    if ( ( exitStatus == QProcess::CrashExit ) || ( exitCode != 0 ) ) {
        debug( "+ Shepherd::process_finished: process failed: exit status: %s [%d]; exit code: %d\n", ToString( exitStatus ), exitStatus, exitCode );
        emit shepherd_terminated( _isTerminationExpected, false );
    } else {
        emit shepherd_terminated( _isTerminationExpected, true );
    }
}

void Shepherd::processRunner_succeeded( ) {
    debug( "+ Shepherd::processRunner_succeeded: serial port reset completed successfully\n" );

    if ( _processRunner ) {
        _processRunner->deleteLater( );
        _processRunner = nullptr;
    }

    _stdoutBuffer.clear( );
    _stderrBuffer.clear( );

    launchShepherd( );
}

void Shepherd::processRunner_failed( int const exitCode, QProcess::ProcessError const processError ) {
    debug(
        "+ Shepherd::processRunner_failed: serial port reset failed: exit code %d, process error %s [%d]\n"
        "  + stdout:\n%s"
        "  + stderr:\n%s"
        "",
        exitCode, ToString( processError ), processError,
        _stdoutBuffer.toUtf8( ).data( ),
        _stderrBuffer.toUtf8( ).data( )
    );

    if ( _processRunner ) {
        _processRunner->deleteLater( );
        _processRunner = nullptr;
    }

    _stdoutBuffer.clear( );
    _stderrBuffer.clear( );

    launchShepherd( );
}

void Shepherd::processRunner_stdout( QString const& data ) {
    _stdoutBuffer += data;
}

void Shepherd::processRunner_stderr( QString const& data ) {
    _stderrBuffer += data;
}

bool Shepherd::getReady( char const* functionName, PendingCommand const pendingCommand, int const expectedOkCount ) {
    if ( _pendingCommand != PendingCommand::none ) {
        debug( "+ Shepherd::%s: command %s already in progress\n", functionName, ToString( pendingCommand ) );
        return false;
    }

    _pendingCommand  = pendingCommand;
    _expectedOkCount = expectedOkCount;
    _okCount         = 0;
    return true;
}

QStringList Shepherd::splitLine( QString const& line ) {
    QStringList pieces;
    QString piece;
    bool inQuote = false;

    int length = line.length( );
    int index = 0;
    while ( index < length ) {
        QChar ch = line[index];
        switch ( ch.unicode( ) ) {
            case L' ':
                if ( inQuote ) {
                    piece += ch;
                } else {
                    pieces += piece;
                    piece.clear( );
                }
                break;

            case L'"':
                inQuote = !inQuote;
                break;

            case L'\\':
                if ( inQuote ) {
                    if ( ( index + 1 ) < length ) {
                        piece += line[++index];
                    }
                } else {
                    piece += ch;
                }
                break;

            default:
                piece += ch;
                break;
        }
        ++index;
    }
    if ( !piece.isEmpty( ) ) {
        pieces += piece;
    }

    return pieces;
}

void Shepherd::handleFromPrinter( QString const& input ) {
    if ( input == "ok" ) {
        ++_okCount;
        if ( _okCount == _expectedOkCount ) {
            debug( "+ Shepherd::handleFromPrinter/ok: pendingCommand: %s; expectedOkCount: %d; okCount: %d; got final expected 'ok', dispatching completion notification\n", ToString( _pendingCommand ), _expectedOkCount, _okCount );

            auto pending = _pendingCommand;
            _pendingCommand = PendingCommand::none;
            _actionCompleteMap[pending]( true );
        } else {
            debug( "+ Shepherd::handleFromPrinter/ok: pendingCommand: %s; expectedOkCount: %d; okCount: %d\n", ToString( _pendingCommand ), _expectedOkCount, _okCount );
        }
    } else if ( 0 == input.left( 6 ).compare( "error:", Qt::CaseInsensitive ) ) {
        debug( "+ Shepherd::handleFromPrinter/error: printer says: '%s'\n", input.toUtf8( ).data( ) );

        auto pending = _pendingCommand;
        _pendingCommand = PendingCommand::none;
        _actionCompleteMap[pending]( false );
    } else if ( auto match = PositionReportMatcher.match( input ); match.hasMatch( ) ) {
        auto px = match.captured( 1 ).toDouble( );
        auto cx = match.captured( 5 ).toInt( );
        emit printer_positionReport( px, cx );
    } else if ( auto match = TemperatureReport1Matcher.match( input ); match.hasMatch( ) ) {
        auto bedCurrentTemperature = match.captured( 3 ).toDouble( );
        auto bedTargetTemperature  = match.captured( 4 ).toDouble( );
        auto bedPwm                = match.captured( 6 ).toInt( );
        emit printer_temperatureReport( bedCurrentTemperature, bedTargetTemperature, bedPwm );
    } else if ( auto match = TemperatureReport2Matcher.match( input ); match.hasMatch( ) ) {
        auto bedCurrentTemperature = match.captured( 1 ).toDouble( );
        auto bedTargetTemperature  = match.captured( 2 ).toDouble( );
        auto bedPwm                = match.captured( 3 ).toInt( );
        emit printer_temperatureReport( bedCurrentTemperature, bedTargetTemperature, bedPwm );
    } else if ( auto match = FirmwareVersionMatcher.match( input ); match.hasMatch( ) ) {
        auto firmwareVersion = match.captured( 1 );
        emit printer_firmwareVersionReport( firmwareVersion );
    }
}

void Shepherd::handleCommandFail( QStringList const& input ) {
    debug( "+ Shepherd::handleCommandFail: input='%s' pendingCommand=%s [%d]\n", input.join( Space ).toUtf8( ).data( ), ToString( _pendingCommand ), _pendingCommand );

    auto pending = _pendingCommand;
    _pendingCommand = PendingCommand::none;
    switch ( pending ) {
        case PendingCommand::moveRelative:
            emit action_moveRelativeComplete( false );
            break;

        case PendingCommand::moveAbsolute:
            emit action_moveAbsoluteComplete( false );
            break;

        case PendingCommand::home:
            emit action_homeComplete( false );
            break;

        case PendingCommand::send:
            emit action_sendComplete( false );
            break;

        case PendingCommand::none:
            debug( "+ Shepherd::handleCommandFail: no pending command\n" );
            break;

        default:
            debug( "+ Shepherd::handleCommandFail: unknown pending command\n" );
            break;
    }
}

#if defined _DEBUG
void Shepherd::handleCommandFailAlternate( QStringList const& input ) {
    debug( "+ Shepherd::handleCommandFailAlternate: input='%s' pendingCommand=%s [%d]\n", input.join( Space ).toUtf8( ).data( ), ToString( _pendingCommand ), _pendingCommand );

    auto pending = _pendingCommand;
    _pendingCommand = PendingCommand::none;
    switch ( pending ) {
        case PendingCommand::moveRelative:
            emit action_moveRelativeComplete( true );
            break;

        case PendingCommand::moveAbsolute:
            emit action_moveAbsoluteComplete( true );
            break;

        case PendingCommand::home:
            emit action_homeComplete( true );
            break;

        case PendingCommand::send:
            emit action_sendComplete( true );
            break;

        case PendingCommand::none:
            debug( "+ Shepherd::handleCommandFailAlternate: no pending command\n" );
            break;

        default:
            debug( "+ Shepherd::handleCommandFailAlternate: unknown pending command\n" );
            break;
    }
}
#endif // defined _DEBUG

void Shepherd::handleInput( QString const& input ) {
    _buffer += input;

    auto lines = _buffer.split( NewLineRegex );

    // if the buffer doesn't end with a newline character, then the last line is
    // not yet complete. put it back in the buffer and forget about it for now.
    if ( !_buffer.endsWith( LineFeed ) ) {
        _buffer = lines.last( );
        lines.removeLast( );
    } else {
        _buffer.clear( );
    }

    for ( auto line : lines ) {
        if ( line.endsWith( CarriageReturn ) ) {
            line.chop( 1 );
        }
        if ( line.isEmpty( ) ) {
            continue;
        }

        auto pieces = splitLine( line );
        if ( pieces[0] == "ok" ) {
            /*empty*/
        } else if ( pieces[0] == "fail" ) {
            debug( "+ Shepherd::handleInput: fail %s\n", pieces[1].toUtf8( ).data( ) );
#if defined _DEBUG
            if ( g_settings.ignoreShepherdFailures ) {
                handleCommandFailAlternate( pieces );
            } else {
                handleCommandFail( pieces );
            }
#else
            handleCommandFail( pieces );
#endif // defined _DEBUG
        } else if ( pieces[0] == "warning" ) {
            debug( "+ Shepherd::handleInput: warning from shepherd: %s\n", pieces[1].toUtf8( ).data( ) );
        } else if ( pieces[0] == "info" ) {
            debug( "+ Shepherd::handleInput: info from shepherd about '%s': %s\n", pieces[1].toUtf8( ).data( ), pieces[2].toUtf8( ).data( ) );
        } else if ( pieces[0] == "from_printer" ) {
            debug( "<<< '%s'\n", pieces[1].toUtf8( ).data( ) );
            handleFromPrinter( pieces[1] );
        } else if ( pieces[0] == "to_printer" ) {
            debug( ">>> '%s'\n", pieces[1].toUtf8( ).data( ) );
        } else if ( pieces[0] == "printer_online" ) {
            emit printer_online( );
        } else if ( pieces[0] == "printer_offline" ) {
            emit printer_offline( );
        } else {
            debug( "+ Shepherd::handleInput: unknown verb from shepherd: '%s'\n", pieces[0].toUtf8( ).data( ) );
        }
    }
}

void Shepherd::start( ) {
    debug( "+ Shepherd::start\n" );

    _stdoutBuffer.clear( );
    _stderrBuffer.clear( );

    _processRunner = new ProcessRunner( this );
    QObject::connect( _processRunner, &ProcessRunner::succeeded,               this, &Shepherd::processRunner_succeeded );
    QObject::connect( _processRunner, &ProcessRunner::failed,                  this, &Shepherd::processRunner_failed    );
    QObject::connect( _processRunner, &ProcessRunner::readyReadStandardOutput, this, &Shepherd::processRunner_stdout    );
    QObject::connect( _processRunner, &ProcessRunner::readyReadStandardError,  this, &Shepherd::processRunner_stderr    );
    _processRunner->start( ResetLumenArduinoPortCommand, { } );
}

void Shepherd::doMoveRelative( float const relativeDistance, float const speed ) {

#if defined _DEBUG
    if ( g_settings.pretendPrinterIsOnline ) {
        debug( "+ Shepherd::doMoveRelative: Mocking relative move : %.2f with speed %.2f\nThis could happen only in debug!\n", relativeDistance,  speed);
        emit action_moveRelativeComplete( true );
        return;
    }
#endif // defined _DEBUG
    if ( getReady( "doMoveRelative", PendingCommand::moveRelative, 4 ) ) {
        _process->write( QString::asprintf( "moveRel %.2f %.2f\n", relativeDistance, speed ).toUtf8( ) );
    }
}

void Shepherd::doMoveAbsolute( float const absolutePosition, float const speed ) {
#if defined _DEBUG
    if ( g_settings.pretendPrinterIsOnline ) {
        debug( "+ Shepherd::doMoveAbsolute: Mocking absolute move to: %.2f with speed %.2f\nThis could happen only in debug!\n", absolutePosition,  speed);
        emit action_moveAbsoluteComplete( true );
        return;
    }
#endif // defined _DEBUG
    if ( getReady( "doMoveAbsolute", PendingCommand::moveAbsolute, 4 ) ) {
        _process->write( QString::asprintf( "moveAbs %.2f %.2f\n", absolutePosition, speed ).toUtf8( ) );
    }
}

void Shepherd::doHome( ) {
    if ( getReady( "doHome", PendingCommand::home, 3 ) ) {
        _process->write( "home\n" );
    }
}

void Shepherd::doSend( QString cmd ) {
    if ( getReady( "doSend", PendingCommand::send, 1 ) ) {
        doSendOne( cmd );
    }
}

void Shepherd::doSend( QStringList cmds ) {
    if ( getReady( "doSend", PendingCommand::send, cmds.count( ) ) ) {
        for ( auto& cmd : cmds ) {
            doSendOne( cmd );
        }
    }
}

void Shepherd::doSendOne( QString& cmd ) {
    _process->write( QString( "send \"%1\"\n" ).arg( cmd.replace( "\\", "\\\\" ).replace( "\"", "\\\"" ) ).toUtf8( ) );
}

void Shepherd::launchShepherd( ) {
    if ( _process ) {
        _process->kill( );
        _process->deleteLater( );
        _process = nullptr;
    }

    _process = new QProcess( parent( ) );
    QObject::connect( _process, &QProcess::errorOccurred,                                        this, &Shepherd::process_errorOccurred           );
    QObject::connect( _process, &QProcess::started,                                              this, &Shepherd::process_started                 );
    QObject::connect( _process, &QProcess::readyReadStandardError,                               this, &Shepherd::process_readyReadStandardError  );
    QObject::connect( _process, &QProcess::readyReadStandardOutput,                              this, &Shepherd::process_readyReadStandardOutput );
    QObject::connect( _process, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), this, &Shepherd::process_finished                );
    _process->setWorkingDirectory( ShepherdPath );
    _process->start( "./stdio-shepherd.py" );
}

void Shepherd::doTerminate( ) {
    _isTerminationExpected = true;
    _process->write( "terminate\n" );
    _process->waitForFinished( );
}

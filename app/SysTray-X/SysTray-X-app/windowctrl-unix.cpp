#include "windowctrl-unix.h"

#ifdef Q_OS_UNIX

/*
 *  Local includes
 */
#include "debug.h"

/*
 *  Local includes
 */
#include "preferences.h"

/*
 *  System includes
 */
#include <unistd.h>

/*
 *  Qt includes
 */
#include <QFileInfo>

/*
 *  Constructor
 */
WindowCtrlUnix::WindowCtrlUnix( QObject *parent ) : QObject( parent )
{
    /*
     *  Initialize
     */
    m_tb_windows = QList< quint64 >();
    m_tb_window_positions = QList< QPoint >();
    m_tb_window_states = QList< Preferences::WindowState >();
    m_tb_window_hints = QMap< quint64, SizeHints >();

    /*
     *  Get the base display and window
     */
    m_display = OpenDisplay();
}


/*
 *  Set the minimize type
 */
void    WindowCtrlUnix::setMinimizeType( Preferences::MinimizeType type )
{
    m_minimize_type = type;
}


/*
 *  Get the minimize type
 */
Preferences::MinimizeType    WindowCtrlUnix::getMinimizeType() const
{
    return m_minimize_type;
}


/*
 *  Get the parent pid of SysTray-X, TB hopefully
 */
qint64  WindowCtrlUnix::getPpid() const
{
    return getppid();
}


/*
 *  Is the pid from thunderbird
 */
bool    WindowCtrlUnix::isThunderbird( qint64 pid ) const
{
    return getProcessName( pid ).contains( "thunderbird", Qt::CaseInsensitive );
}


/*
 *  Get the process name
 */
QString WindowCtrlUnix::getProcessName( qint64 pid ) const
{
    QString process_name = QString( "/proc/%1/exe" ).arg( pid );
    QFileInfo process( process_name );

    return process.canonicalFilePath();
}


/*
 *  Find window(s) by title
 */
bool    WindowCtrlUnix::findWindow( const QString& title )
{
    QList< WindowItem > windows = listXWindows( m_display, GetDefaultRootWindow( m_display ) );

    m_tb_windows = QList< quint64 >();
    m_tb_window_positions = QList< QPoint >();
    for( int i = 0 ; i < windows.length() ; ++i )
    {
        WindowItem win = windows.at( i );

        char *name = nullptr;
        if( FetchName( m_display, win.window, &name ) > 0 ) {
            QString win_name( name );

            Free( name );

            if( win_name.contains( title, Qt::CaseInsensitive ) ) {
                /*
                 *  Store the XID
                 */
                m_tb_windows.append( win.window );
                m_tb_window_positions.append( QPoint() );
            }
        }
    }

    if( m_tb_windows.length() > 0 )
    {
        return true;
    }

    return false;
}


/*
 *  Find a window by PID
 */
void    WindowCtrlUnix::findWindows( qint64 pid )
{
#ifdef DEBUG_DISPLAY_ACTIONS
    emit signalConsole( "Find windows and states" );
#endif

    QList< WindowItem > windows = listXWindows( m_display, GetDefaultRootWindow( m_display ) );

    QList< QPoint > old_positions = m_tb_window_positions;

    m_tb_windows = QList< quint64 >();
    m_tb_window_positions = QList< QPoint >();
    for( int i = 0 ; i < windows.length() ; ++i )
    {
        WindowItem win = windows.at( i );

        qint32 n_propPID;
        void* propPID = GetWindowProperty( m_display, win.window, "_NET_WM_PID", &n_propPID );

        if( propPID != nullptr )
        {
            if( pid == *((reinterpret_cast<qint64 *>( propPID ) ) ) )
            {

                qint32 n_wm_state;
                void* wm_stat_ptr = GetWindowProperty( m_display, win.window, "WM_STATE", &n_wm_state );

                qint32 n_net_wm_state;
                void* net_wm_state_ptr = GetWindowProperty( m_display, win.window, "_NET_WM_STATE", &n_net_wm_state );

                if( wm_stat_ptr != nullptr || net_wm_state_ptr != nullptr  )
                {
                    m_tb_windows.append( win.window );

                    QPoint point;
                    if( m_tb_windows.length() <= old_positions.length() )
                    {
                        point = old_positions.at( m_tb_window_positions.length() - 1 );
                    }

                    m_tb_window_positions.append( point );

                    if( wm_stat_ptr != nullptr  )
                    {
                        Free( wm_stat_ptr );
                    }

                    if( net_wm_state_ptr != nullptr  )
                    {
                        Free( net_wm_state_ptr );
                    }
                }
            }

            Free( propPID );
        }
    }

#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
    emit signalConsole( QString( "Number of windows found: %1" ).arg( m_tb_windows.length() ) );
#endif

    /*
     *  Get the new window states, store the old ones
     */
    m_tb_window_states = QList< Preferences::WindowState >();
    for( int i = 0 ; i< m_tb_windows.length() ; ++i )
    {
        int state = -1;

        qint32 n_state;
        void* state_ptr = GetWindowProperty( m_display, m_tb_windows.at( i ), "WM_STATE", &n_state );

        if( state_ptr != nullptr )
        {
            state = *reinterpret_cast<long *>( state_ptr );

            Free( state_ptr );
        }

/*
        if( atom_list.contains( "_NET_WM_STATE_HIDDEN" ) && atom_list.contains( "_NET_WM_STATE_SKIP_TASKBAR" ) )
        {
            m_tb_window_states.append( Preferences::STATE_DOCKED );
        }
        else
        if( atom_list.contains( "_NET_WM_STATE_HIDDEN" ) )
*/

        if( state == -1 || state == 3 )
        {
#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
            emit signalConsole( QString( "WinID %1, state: %2, Minimized").arg( m_tb_windows.at( i ) ).arg( state ) );
#endif

            m_tb_window_states.append( Preferences::STATE_MINIMIZED );
        }
        else
        {
#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
            emit signalConsole( QString( "WinID %1, state: %2, Normal").arg( m_tb_windows.at( i ) ).arg( state ) );
#endif

            m_tb_window_states.append( Preferences::STATE_NORMAL );
        }
    }

#ifdef DEBUG_DISPLAY_ACTIONS_END
    emit signalConsole( "Find windows and states done" );
#endif
}


/*
 *  Find a window by PID
 */
void    WindowCtrlUnix::findWindows2( qint64 pid )
{
#ifdef DEBUG_DISPLAY_ACTIONS
    emit signalConsole( "Find windows and states 2" );
#endif

    QList< WindowItem > windows = listXWindows( m_display, GetDefaultRootWindow( m_display ) );

    QList< QPoint > old_positions = m_tb_window_positions;

    QList< quint64 > tb_windows;
    QList< QPoint > tb_window_positions;
    for( int i = 0 ; i < windows.length() ; ++i )
    {
        WindowItem win = windows.at( i );

        qint32 n_propPID;
        void* propPID = GetWindowProperty( m_display, win.window, "_NET_WM_PID", &n_propPID );

        if( propPID != nullptr )
        {
            if( pid == *((reinterpret_cast<qint64 *>( propPID ) ) ) )
            {
                emit signalConsole( QString( "Pid found %1" ).arg( pid ) );

                qint32 n_wm_state;
                void* wm_stat_ptr = GetWindowProperty( m_display, win.window, "WM_STATE", &n_wm_state );

                if( wm_stat_ptr != nullptr )
                {
                    int wm_state = *reinterpret_cast<long *>( wm_stat_ptr );

                    Free( wm_stat_ptr );

                    emit signalConsole( QString( "wm_state %1, nr: %2, %3" ).arg( win.window ).arg( n_wm_state ).arg( wm_state ) );
                }

                qint32 n_net_wm_state;
                void* net_wm_state_ptr = GetWindowProperty( m_display, win.window, "_NET_WM_STATE", &n_net_wm_state );

                if( net_wm_state_ptr != nullptr )
                {
                    tb_windows.append( win.window );

                    emit signalConsole( QString( "net_wm_state %1, nr: %2" ).arg( win.window ).arg( n_net_wm_state ) );

                    QPoint point;
                    if( tb_windows.length() <= old_positions.length() )
                    {
                        point = old_positions.at( tb_window_positions.length() - 1 );
                    }

                    tb_window_positions.append( point );

                    Free( net_wm_state_ptr );
                }
            }

            Free( propPID );
        }
    }

#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
    emit signalConsole( QString( "Number of windows found: %1" ).arg( tb_windows.length() ) );
#endif

    /*
     *  Get the new window states, store the old ones
     */
    m_tb_window_states = QList< Preferences::WindowState >();
    for( int i = 0 ; i< tb_windows.length() ; ++i )
    {
        int state = -1;

        qint32 n_state;
        void* state_ptr = GetWindowProperty( m_display, tb_windows.at( i ), "WM_STATE", &n_state );

        if( state_ptr != nullptr )
        {
            state = *reinterpret_cast<long *>( state_ptr );

            Free( state_ptr );
        }

        if( state == -1 || state == 3 )
        {
#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
            emit signalConsole( QString( "WinID %1, state: %2, Minimized").arg( tb_windows.at( i ) ).arg( state ) );
#endif

            m_tb_window_states.append( Preferences::STATE_MINIMIZED );
        }
        else
        {
#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
            emit signalConsole( QString( "WinID %1, state: %2, Normal").arg( tb_windows.at( i ) ).arg( state ) );
#endif

            m_tb_window_states.append( Preferences::STATE_NORMAL );
        }
    }

#ifdef DEBUG_DISPLAY_ACTIONS_END
    emit signalConsole( "Find windows and states 2 done" );
#endif
}


/*
 *  Get the Thunderbird window IDs
 */
QList< quint64 >   WindowCtrlUnix::getWinIds()
{
    return m_tb_windows;
}


/*
 *  Get the states of the TB windows.
 */
const QList< Preferences::WindowState >&    WindowCtrlUnix::getWindowStates() const
{
    return m_tb_window_states;
}


/*
 *  Display window atoms
 */
void    WindowCtrlUnix::displayWindowElements( const QString& title )
{
    QList< WindowItem > windows = listXWindows( m_display, GetDefaultRootWindow( m_display ) );

    for( int i = 0 ; i < windows.length() ; ++i )
    {
        WindowItem win = windows.at( i );

        char *name = nullptr;
        if( FetchName( m_display, win.window, &name ) > 0 ) {
            QString win_name( name );

            Free( name );

            if( win_name.contains( title, Qt::CaseInsensitive ) ) {

                emit signalConsole( QString( "Found: Level %1, XID %2, Name %3" ).arg( win.level ).arg( win.window ).arg( win_name ) );

                displayWindowElements( win.window );
            }
        }
    }
}


/*
 *  Display window atoms
 */
void    WindowCtrlUnix::displayWindowElements( quint64 window )
{
    QString name;

    qint32 n_name;
    void* name_ptr = GetWindowProperty( m_display, window, "_NET_WM_NAME", &n_name );

    if( name_ptr != nullptr )
    {
        name = QString( reinterpret_cast<char*>( name_ptr ) );

        Free( name_ptr );
    }

    emit signalConsole( QString( "Atom name: %1" ).arg( name ) );

    QStringList types;

    qint32 n_types;
    void* types_ptr = GetWindowProperty( m_display, window, "_NET_WM_WINDOW_TYPE", &n_types );

    if( types_ptr != nullptr )
    {
        for( qint32 i = 0; i < n_types; ++i )
        {
            char* type_name = GetAtomName( m_display, reinterpret_cast<long *>( types_ptr )[ i ] );

            types.append( type_name );

            if( type_name )
            {
                Free( type_name );
            }
        }

        Free( types_ptr );
    }

    for( int i = 0 ; i < types.length() ; ++i )
    {
        emit signalConsole( QString( "Atom type: %1" ).arg( types.at( i ) ) );
    }

    QStringList states;

    qint32 n_states;
    void* states_ptr = GetWindowProperty( m_display,window, "_NET_WM_STATE", &n_states );

    if( states_ptr != nullptr )
    {
        for( qint32 i = 0; i < n_states ; ++i )
        {
            char* atom_name = GetAtomName( m_display, reinterpret_cast<long *>( states_ptr )[ i ] );

            states.append( atom_name );

            if( atom_name )
            {
                Free( atom_name );
            }
        }

        Free( states_ptr );
    }

    bool max_vert = false;
    bool max_horz = false;
    bool hidden = false;

    for( int i = 0 ; i < states.length() ; ++i )
    {
        QString state = states.at( i );

        emit signalConsole( QString( "Atom state: %1" ).arg( state ) );

        int state_code = WindowStates.indexOf( state ) ;

        switch( state_code )
        {
            case STATE_MAXIMIZED_VERT:
            {
                max_vert = true;
                break;
            }

            case STATE_MAXIMIZED_HORZ:
            {
                max_horz = true;
                break;
            }

            case STATE_HIDDEN:
            {
                hidden = true;
                break;
            }
        }
    }

    if( states.length() > 0 )
    {
        if( hidden )
        {
            emit signalConsole( "Window State: Hidden" );
        }
        else
        if( max_vert && max_horz )
        {
            emit signalConsole( "Window State: Maximize" );
        }
        else
        {
            emit signalConsole( "Window State: Normal" );
        }
    }
    else
    {
        emit signalConsole( "Window State: Normal" );
    }
}


/*
 *  Get window positions
 */
void    WindowCtrlUnix::updatePositions()
{
#ifdef DEBUG_DISPLAY_ACTIONS
    emit signalConsole( "Update positions" );
#endif

    bool changed = false;
    for( int i = 0 ; i < m_tb_windows.length() ; ++i )
    {
        if( m_tb_window_states.at( i ) != Preferences::STATE_MINIMIZED )
        {
            quint64 window = m_tb_windows.at( i );

            /*
             *  Get border / title bar sizes
             */
            long left;
            long top;
            long right;
            long bottom;
            GetWindowFrameExtensions( m_display, window, &left, &top, &right, &bottom );

#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
            emit signalConsole( QString( "Margins: %1, %2, %3, %4" ).arg( left ).arg( top ).arg( right ).arg( bottom ) );
#endif

            /*
             *  Get the position
             */
            long x;
            long y;
            GetWindowPosition( m_display, window, &x, &y );

            /*
             *  Update the list?
             */
            QPoint point = QPoint( x - left, y - top );

            if( m_tb_window_positions[ i ] != point )
            {
                m_tb_window_positions[ i ] = point;

                /*
                 *  Mar the list changed
                 */
                changed = true;
            }

#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
            emit signalConsole( QString( "Update pos: %1, %2" ).arg( x - left ).arg( y - top ) );
#endif
        }
    }

    if( changed )
    {
        emit signalPositions( m_tb_window_positions );
    }

#ifdef DEBUG_DISPLAY_ACTIONS_END
    emit signalConsole( "Update positions done" );
#endif
}


/*
 *  Minimize a window
 */
void    WindowCtrlUnix::minimizeWindow( quint64 window )
{
#ifdef DEBUG_DISPLAY_ACTIONS
    emit signalConsole( "Minimize" );
#endif

    /*
     *  Save the hints
     */
    GetWMNormalHints( m_display, window, &m_tb_window_hints[ window ] );

    /*
     *  Minimize the window
     */
    IconifyWindow( m_display, window );

    /*
     *  Sync the events
     */
    Sync( m_display );

    if( getMinimizeType() != Preferences::PREF_DEFAULT_MINIMIZE )
    {
#ifdef DEBUG_DISPLAY_ACTIONS
        emit signalConsole( "Withdraw window" );
#endif

        /*
         *  Set the flags (GNOME, Wayland?)
         */
        SendEvent( m_display, window, "_NET_WM_STATE", _NET_WM_STATE_ADD, _ATOM_SKIP_TASKBAR );
        SendEvent( m_display, window, "_NET_WM_STATE", _NET_WM_STATE_ADD, _ATOM_SKIP_PAGER );

        Flush( m_display );

        /*
         *  Remove from taskbar and task switchers
         */
        WithdrawWindow( m_display, window );
    }

    /*
     *  Flush the pipes
     */
    Flush( m_display );

#ifdef DEBUG_DISPLAY_ACTIONS_END
    emit signalConsole( "Minimize done" );
#endif
}


/*
 *  Hide window to system tray
 */
void    WindowCtrlUnix::hideWindow( quint64 window, bool hide )
{
    if( hide )
    {
#ifdef DEBUG_DISPLAY_ACTIONS
        emit signalConsole( "Remove from taskbar and pager" );
#endif

        SendEvent( m_display, window, "_NET_WM_STATE", _NET_WM_STATE_ADD, _ATOM_SKIP_TASKBAR );
        SendEvent( m_display, window, "_NET_WM_STATE", _NET_WM_STATE_ADD, _ATOM_SKIP_PAGER );

//        Sync( m_display );
//        ChangeWindowTypeProperty( m_display, window, "_NET_WM_WINDOW_TYPE_DOCK" );
    }
    else
    {
#ifdef DEBUG_DISPLAY_ACTIONS
        emit signalConsole( "Display in taskbar and pager" );
#endif

//        ChangeWindowTypeProperty( m_display, window, "_NET_WM_WINDOW_TYPE_NORMAL" );
//        Sync( m_display );

        SendEvent( m_display, window, "_NET_WM_STATE", _NET_WM_STATE_REMOVE, _ATOM_SKIP_TASKBAR );
        SendEvent( m_display, window, "_NET_WM_STATE", _NET_WM_STATE_REMOVE, _ATOM_SKIP_PAGER );
    }

    Flush( m_display );

#ifdef DEBUG_DISPLAY_ACTIONS_END
    emit signalConsole( QString( "Hide: %1").arg(hide) );
#endif
}


/*
 *  Normalize a window
 */
void    WindowCtrlUnix::normalizeWindow( quint64 window )
{
#ifdef DEBUG_DISPLAY_ACTIONS
    emit signalConsole( "Normalize" );
#endif

    /*
     *  Show window on taskbar an in switcher
     */
    if( getMinimizeType() != Preferences::PREF_DEFAULT_MINIMIZE )
    {
        MapWindow( m_display, window );

        SetWMNormalHints( m_display, window, m_tb_window_hints[ window ] );

        /*
         *  Reset the hide flags
         */
        SendEvent( m_display, window, "_NET_WM_STATE", _NET_WM_STATE_REMOVE, _ATOM_SKIP_TASKBAR );
        SendEvent( m_display, window, "_NET_WM_STATE", _NET_WM_STATE_REMOVE, _ATOM_SKIP_PAGER );

        Flush( m_display );
    }

    /*
     *  Raise the window to the top
     */
    MapRaised( m_display, window );

    /*
     *  Flush the pipes
     */
    Flush( m_display );

    /*
     *  Get the current desktop
     */
    int current_desktop = 0;

    qint32 n_current_desktop;
    long* current_desktop_ptr = (long*)GetWindowProperty( m_display, 0, "_NET_CURRENT_DESKTOP", &n_current_desktop );

    if( current_desktop_ptr != nullptr )
    {
        /*
         *  Store it
         */
        current_desktop = *current_desktop_ptr;

        /*
         *  Cleanup
         */
        Free( current_desktop_ptr );

        /*
         *  Set the desktop for the window
         */
        SendEvent( m_display, window, "_NET_WM_DESKTOP", current_desktop, 1 );
    }

    /*
     *  Normalize
     */
    SendEvent( m_display, window, "_NET_ACTIVE_WINDOW" );

    /*
     *  Flush the pipes
     */
    Flush( m_display );

#ifdef DEBUG_DISPLAY_ACTIONS_END
    emit signalConsole( "Normalize done" );
#endif
}


/*
 *  Delete the window
 */
void    WindowCtrlUnix::deleteWindow( quint64 window )
{
    SendEvent( m_display, window, "WM_PROTOCOLS", _ATOM_DELETE_WINDOW );

    Flush( m_display );
}


/*
 *  Set the window positions
 */
void    WindowCtrlUnix::setPositions( QList< QPoint > window_positions )
{
    for( int i = 0 ; i < m_tb_windows.length() ; ++i )
    {
        quint64 window = m_tb_windows.at( i );

#ifdef DEBUG_DISPLAY_ACTIONS_DETAILS
        emit signalConsole( QString( "Set pos: %1, %2").arg( window_positions.at( i ).x() ).arg( window_positions.at( i ).y() ) );
#endif

        if( i < window_positions.length() ) {
            MoveWindow( m_display, window, window_positions.at( i ).x(), window_positions.at( i ).y() );
        }
    }

    Flush( m_display );
}


/*
 *  Get the X11 window list
 */
QList< WindowCtrlUnix::WindowItem >   WindowCtrlUnix::listXWindows( void* display, quint64 window, int level )
{
    quint64 root;
    quint64 parent;
    void* children;
    unsigned int childrenCount;

    QList< WindowItem > windows;
    if( QueryTree( display, window, &root, &parent, &children, &childrenCount) )
    {
        for( unsigned int i = 0; i < childrenCount; ++i )
        {
            windows.append( WindowItem( ((quint64*)children)[ i ], level ) );
            windows.append( listXWindows( display, ((quint64*)children)[ i ], level + 1) );
        }

        Free( children );
    }

    return windows;
}

#endif // Q_OS_UNIX

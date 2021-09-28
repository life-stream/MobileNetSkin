#include "LTEModemIf.h"
#include <fcntl.h>
#include <thread>
#include <termios.h>
#include <iostream>
#include <mutex>
#include <filesystem>
#include <unistd.h>
#include "Log.h"

using namespace std;

#define MODEM_READBUF_SIZE 50
#define LINE_LENGTH 12

mutex g_mux;
string CLTEModemIf::ms_reply{""};

CLTEModemIf& CLTEModemIf::GetInstance()
{
    static CLTEModemIf instance;
    return instance;
}

CLTEModemIf::CLTEModemIf(): m_fd{-1}
{
}

CLTEModemIf::~CLTEModemIf()
{
    close( m_fd );
}

bool CLTEModemIf::InitModem( string dev_name )
{
    LogOutLine( "InitModem called.", 3 );
    if( !filesystem::exists( dev_name ) ) //出问题多半是设备脱线了
    {
        LogOutLine( "Usb communication interface offline.");
        return false;
    }

    if( m_fd != -1 )
        close( m_fd );

    m_fd = open( dev_name.c_str(), O_RDWR|O_NOCTTY|O_NDELAY );

    struct termios opt;
    memset( &opt, 0, sizeof( opt ) );

    cfsetispeed( &opt, B115200 );
    cfsetospeed (&opt, B115200 );

    opt.c_cflag &= ~CSIZE;                            //字符长度（据说设置数据位之前一定要先关闭一下）
    opt.c_cflag |= CS8;                               //数据位
    opt.c_cflag &= ~CSTOPB;                           //停止位为1
    opt.c_cflag &= ~PARENB;                           //无奇偶校验
    opt.c_cflag &= ~CRTSCTS;                          //不使用硬件流控（因为我的网卡不支持）
    opt.c_cflag |= IXON | IXOFF | IXANY;              //用软件流控
    opt.c_cflag |= ( CLOCAL | CREAD );                //忽略所有状态行，启用字符接收器（…？）

    opt.c_lflag &= ~( ICANON | ECHO | ECHOE | ISIG ); //设置为本地模式（啥玩意？）
    opt.c_oflag &= ~OPOST;                            //不使用自定义输出处理（也没看懂）

    opt.c_cc[VTIME] = 3;                              //读取的超时时间，单位100ms
    opt.c_cc[VMIN]  = 1;                              //读取的最小字符数

    tcflush( m_fd, TCIOFLUSH );                       //为免出现奇怪的问题把读写缓存都清空一下

    //激活配置，出问题可能是usb线的接头芯片坏了或者网卡内部老化损坏了。
    bool res = ( tcsetattr( m_fd, TCSANOW, &opt ) == 0 );
    LogOutLine( "Communication attr set: " + to_string( res ), 2 );
    if( !res )
    {
        LogOutLine( to_string( errno ) + " " + strerror( errno ), 2 );
        LogOutLine( "m_fd " + to_string( m_fd ), 2 );
    }

    return res;
}

bool CLTEModemIf::CheckWwanDevStatus( string dev_name )
{
    LogOutLine( "CheckWwanDevStatus called.", 3 );
    return filesystem::exists( dev_name );
}

bool CLTEModemIf::CheckSimCardStatus()
{
    LogOutLine( "CheckSimCardStatus called.", 3 );
    string at_cmd{"AT+CPIN?\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );

    lock_guard<mutex> lock( g_mux );
    ms_reply.clear();
    thread at_reply_th( GetAtCmdReply, m_fd, "READY", 5000 );

    this_thread::sleep_for( std::chrono::milliseconds( 6000 ) );
    if( at_reply_th.joinable() )
    {
        pthread_cancel( at_reply_th.native_handle() );
    }
    at_reply_th.join();

    if( ms_reply.empty() )
    {
        LogOutLine( "AT+CPIN? no reply." );
        return false;
    }

    LogOutLine( "AT+CPIN? reply.", 1 );
    LogOutLine( ms_reply, 1 );
    return true;
}

int CLTEModemIf::GetSignalStrengthLevel()
{
    LogOutLine( "GetSignalStrengthLevel called.", 3 );
    string at_cmd{"AT+CSQ\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );

    int res_value{0};

    //另起线程读AT指令返回值
    //返回值没有靠谱的结束特征…除了指定时间内指令一定会响应返回之外这个网卡没有任何保证。
    {
        lock_guard<mutex> lock( g_mux );
        ms_reply.clear();
        thread at_reply_th( GetAtCmdReply, m_fd, "+CSQ: ", 300 );   //300ms内网卡固件就会响应

        //正常1s内肯定返回了，但曾经偶然憋住过一次，保险起见
        this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
        if( at_reply_th.joinable() )
        {
            pthread_cancel( at_reply_th.native_handle() );
        }
        at_reply_th.join();

        if( ms_reply.empty() )
        {
            LogOutLine( "AT+CSQ no reply." );
            return -1;   //ERROR
        }

        LogOutLine( "AT+CSQ reply.", 1 );
        LogOutLine( ms_reply, 1 );
        res_value = stoi( ms_reply.substr( 6, 2 ) ); //at_reply的值类似"+CSQ: 28,99"这样
    }

    int level{0};
    if( res_value == 99 )      level = 0;
    else if( res_value >= 30 ) level = 4;
    else if( res_value >= 23 ) level = 3;
    else if( res_value >= 16 ) level = 2;
    else if( res_value >= 9 )  level = 1;
    else                       level = 0;

    LogOutLine( "Signal level: " + to_string( level ), 3 );
    return level;
}

int CLTEModemIf::DeregisterFromLTE()
{
    LogOutLine( "DeregisterFromLTE called.", 3 );
    string at_cmd{"AT+COPS=2,2\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );

    {
        lock_guard<mutex> lock( g_mux );
        ms_reply.clear();
        thread at_reply_th( GetAtCmdReply, m_fd, "OK", 180000 );    //最长180s内响应

        this_thread::sleep_for( std::chrono::milliseconds( 185000 ) );
        if( at_reply_th.joinable() )
        {
            pthread_cancel( at_reply_th.native_handle() );
        }
        at_reply_th.join();

        if( ms_reply.empty() )
        {
            LogOutLine( "AT+COPS=2,2 no reply." );
            return -1;  //ERROR
        }
    }

    LogOutLine( "AT+COPS=2,2 reply.", 1 );
    LogOutLine( ms_reply, 1 );
    return 0;
}
int CLTEModemIf::AutomaticRegisterLTE()
{
    LogOutLine( "AutomaticRegisterLTE called.", 3 );
    string at_cmd{"AT+COPS=0\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );

    {
        lock_guard<mutex> lock( g_mux );
        ms_reply.clear();
        thread at_reply_th( GetAtCmdReply, m_fd, "OK", 180000 );    //最长180s内响应

        this_thread::sleep_for( std::chrono::milliseconds( 185000 ) );
        if( at_reply_th.joinable() )
        {
            pthread_cancel( at_reply_th.native_handle() );
        }
        at_reply_th.join();

        if( ms_reply.empty() )
        {
            LogOutLine( "AT+COPS=0 no reply." );
            return -1;  //ERROR
        }
    }

    LogOutLine( "AT+COPS=0 reply.", 1 );
    LogOutLine( ms_reply, 1 );
    return 0;
}

void CLTEModemIf::GetAtCmdReply( int modem_fd, string target, int waiting_ms )
{
    this_thread::sleep_for(std::chrono::milliseconds( 100 ));
    char buf[MODEM_READBUF_SIZE];
    string s_buf;
    for( int i {0}; i < waiting_ms; i+= 100 ) 
    {
        memset( buf, 0, MODEM_READBUF_SIZE );
        int l = read( modem_fd, buf, MODEM_READBUF_SIZE );
        if( l == 0 ) continue;
        if( l== MODEM_READBUF_SIZE ) l--;
        buf[l] = '\0';
        s_buf = buf;

        LogOutLine( "read " + to_string( i ), 2 );
        LogOutLine( "\n" + s_buf, 2 );
        LogOutCharAsc( buf, MODEM_READBUF_SIZE, 3 );

        int line = s_buf.find( target );
        if( line != string::npos )
        {
            ms_reply = s_buf.substr( line, LINE_LENGTH );
            break;
        }
        this_thread::sleep_for(std::chrono::milliseconds( 100 ));
    }
    return;
}


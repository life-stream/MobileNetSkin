#include "LTEModemIf.h"
#include <fcntl.h>
#include <thread>
#include <termios.h>
#include <iostream>
#include <mutex>
#include <filesystem>
#include <unistd.h>
#include "Log.h"
#include <future>
#include <errno.h>
#include <cstring>

using namespace std;

#define MODEM_READBUF_SIZE 50
#define LINE_LENGTH 12

mutex g_mux;
bool CLTEModemIf::ms_get_reply{false};

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

bool CLTEModemIf::InitModem( const string& dev_name )
{
    LogOutLine( "InitModem called.", 3 );
    if( !filesystem::exists( dev_name ) ) //出问题多半是设备脱线了
    {
        LogOutLine( "Device communication interface offline.");
        return false;
    }

    if( m_fd != -1 )
        close( m_fd );

    m_fd = open( dev_name.c_str(), O_RDWR|O_NOCTTY|O_NDELAY );

    struct termios opt;
    memset( &opt, 0, sizeof( opt ) );

    cfsetispeed( &opt, B115200 );
    cfsetospeed (&opt, B115200 );

    //-------以下来自匿名文档-------

    opt.c_cflag &= ~CSIZE;                            //字符长度（据说设置数据位之前一定要先关闭一下）
    opt.c_cflag |= CS8;                               //数据位
    opt.c_cflag &= ~CSTOPB;                           //停止位为1
    opt.c_cflag &= ~PARENB;                           //无奇偶校验
    opt.c_cflag &= ~CRTSCTS;                          //不使用硬件流控
    opt.c_cflag |= IXON | IXOFF | IXANY;              //用软件流控
    opt.c_cflag |= ( CLOCAL | CREAD );                //忽略所有状态行，启用字符接收器（…？）

    opt.c_lflag &= ~( ICANON | ECHO | ECHOE | ISIG ); //设置为本地模式（啥玩意？）
    opt.c_oflag &= ~OPOST;                            //不使用自定义输出处理（也没看懂）

    //------------------------------

    opt.c_cc[VTIME] = 2;                              //读取的超时时间，单位100ms
    opt.c_cc[VMIN]  = 1;                              //读取的最小字符数

    tcflush( m_fd, TCIOFLUSH );                       //为免出现奇怪的问题把读写缓存都清空一下

    //激活配置，出问题可能是连接线的接头芯片坏了或者网卡内部老化损坏了。
    bool res = ( tcsetattr( m_fd, TCSANOW, &opt ) == 0 );
    LogOutLine( "Communication attr set: " + to_string( res ), 2 );
    if( !res )
    {
        LogOutLine( to_string( errno ) + " " + strerror( errno ), 2 );
        LogOutLine( "m_fd " + to_string( m_fd ), 2 );
    }

    return res;
}

bool CLTEModemIf::CheckWwanDevStatus( const string& dev_name )
{
    LogOutLine( "CheckWwanDevStatus called.", 3 );
    return filesystem::exists( dev_name );
}

bool CLTEModemIf::CheckSimCardStatus()
{
    LogOutLine( "CheckSimCardStatus called.", 3 );
    string at_cmd{"AT+CPIN?\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );

    future<string> at_reply;
    future_status reply_status;

    {
        lock_guard<mutex> lock( g_mux );
        ms_get_reply = true;
        at_reply = async( launch::async, &CLTEModemIf::GetAtCmdReply, this, "READY" );

        reply_status = at_reply.wait_for( chrono::seconds( 5 ) );
        ms_get_reply = false;
    }
    if( reply_status != future_status::ready )
    {
        LogOutLine( "AT+CPIN? no reply." );
        return -1;  //ERROR
    }

    LogOutLine( "AT+CPIN? reply.", 1 );
    return true;
}

int CLTEModemIf::GetSignalStrengthLevel()
{
    LogOutLine( "GetSignalStrengthLevel called.", 3 );
    string at_cmd{"AT+CSQ\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );

    int res_value{0};
    future<string> at_reply;
    future_status reply_status;

    {
        lock_guard<mutex> lock( g_mux );
        ms_get_reply = true;
        at_reply = async( launch::async, &CLTEModemIf::GetAtCmdReply, this, "+CSQ: " );

        reply_status = at_reply.wait_for( chrono::milliseconds( 300 ) );
        ms_get_reply = false;
    }
    if( reply_status == future_status::ready )
    {
        string reply = at_reply.get();
        res_value = stoi( reply.substr( 6, 2 ) ); //at_reply的值类似"+CSQ: 28,99"这样
        LogOutLine( "AT+CSQ reply.\n" + reply, 1 );
    }
    else
    {
        LogOutLine( "AT+CSQ no reply." );
        return -1;   //ERROR
    }

    int level {0};
    if( res_value == 99 )      level = 0;
    else if( res_value >= 30 ) level = 4;
    else if( res_value >= 23 ) level = 3;
    else if( res_value >= 16 ) level = 2;
    else if( res_value >= 9 )  level = 1;
    else                       level = 0;

    LogOutLine( "Signal level: " + to_string( level ), 3 );
    return level;
}

int CLTEModemIf::DeregisterFromMNet()
{
    LogOutLine( "DeregisterFromMNet called.", 3 );
    string at_cmd{"AT+COPS=2,2\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );
    future_status reply_status;

    {
        lock_guard<mutex> lock( g_mux );
        ms_get_reply = true;
        future<string> at_reply = async( launch::async, &CLTEModemIf::GetAtCmdReply, this, "OK" );

        reply_status = at_reply.wait_for( chrono::seconds( 180 ) );
        ms_get_reply = false;
    }
    if( reply_status != future_status::ready )
    {
        LogOutLine( "AT+COPS=2,2 no reply." );
        return -1;  //ERROR
    }

    LogOutLine( "AT+COPS=2,2 reply.", 1 );
    return 0;
}
int CLTEModemIf::AutoRegisterMNet()
{
    LogOutLine( "AutoRegisterMNet called.", 3 );
    string at_cmd{"AT+COPS=0\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );
    future_status reply_status;

    {
        lock_guard<mutex> lock( g_mux );
        ms_get_reply = true;
        future<string> at_reply = async( launch::async, &CLTEModemIf::GetAtCmdReply, this, "OK" );

        reply_status = at_reply.wait_for( chrono::seconds( 180 ) );
        ms_get_reply = false;
    }
    if( reply_status != future_status::ready )
    {
        LogOutLine( "AT+COPS=0 no reply." );
        return -1;  //ERROR
    }

    LogOutLine( "AT+COPS=0 reply.", 1 );
    return 0;
}

int CLTEModemIf::ResetUserEquipment()
{
    LogOutLine( "ResetUserEquipment called.", 3 );
    string at_cmd{"AT+CFUN=0\r"};
    write( m_fd, at_cmd.c_str(), at_cmd.length() );
    future_status reply_status;

    {
        lock_guard<mutex> lock( g_mux );
        ms_get_reply = true;
        future<string> at_reply = async( launch::async, &CLTEModemIf::GetAtCmdReply, this, "OK" );

        reply_status = at_reply.wait_for( chrono::seconds( 5 ) );
        ms_get_reply = false;

        if( reply_status != future_status::ready )
        {
            LogOutLine( "AT+CFUN=0 no reply." );
            return -1;  //ERROR
        }

        string at_cmd{"AT+CFUN=1\r"};
        write( m_fd, at_cmd.c_str(), at_cmd.length() );
        ms_get_reply = true;
        at_reply = async( launch::async, &CLTEModemIf::GetAtCmdReply, this, "+CPIN: READY" );

        reply_status = at_reply.wait_for( chrono::seconds( 15 ) );
        ms_get_reply = false;

        if( reply_status != future_status::ready )
        {
            LogOutLine( "AT+CFUN=1 no reply." );
            return -1;  //ERROR
        }
    }

    LogOutLine( "ResetUserEquipment success.", 1 );
    return 0;
}

//单独起线程读串口返回消息，是因为之前使用中曾经在读取时憋住过：
//没有设置VMIN，没有任何预兆也不知原因地完全阻塞在了不知什么地方。
//设置了VMIN为0之后，又发生过死活读不到返回消息的问题。
//虽然都是没有再现过的偶发事件，但还是应该从一开始就考虑要避免。
string CLTEModemIf::GetAtCmdReply( const string& target )
{
    this_thread::sleep_for(std::chrono::milliseconds( 100 ));
    char buf[MODEM_READBUF_SIZE];
    string s_buf;
    while( ms_get_reply ) 
    {
        memset( buf, 0, MODEM_READBUF_SIZE );
        int l = read( m_fd, buf, MODEM_READBUF_SIZE );
        if( l == 0 ) continue;
        if( l== MODEM_READBUF_SIZE ) l--;
        buf[l] = '\0';
        s_buf = buf;

        LogOutLine( "\n" + s_buf, 2 );
        LogOutCharAsc( buf, MODEM_READBUF_SIZE, 3 );

        int line = s_buf.find( target );
        if( line != string::npos )
        {
            return s_buf.substr( line, LINE_LENGTH );
        }

        this_thread::sleep_for(std::chrono::milliseconds( 100 ));
    }
    return "NULL";
}

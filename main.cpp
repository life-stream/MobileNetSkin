#include "LTEModemIf.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <uci.h>
#include <sstream>
#include <string>
#include "Log.h"
#include <cstdlib>

using namespace std;

typedef struct _ConfItems
{
    bool   enabled;
    string ping_target;
    string connect_exe;
    string wwan_dev;
    string usb_dev;
} ConfItems;

typedef struct _ICMPPack
{
    unsigned char  type;
    unsigned char  code;
    unsigned short checksum;
    unsigned short identifier;
    unsigned short seqnum;
    unsigned long  time;
} ICMPPack;

unsigned short Checksum( void* buffer, int len )
{
    LogOutLine( "Checksum called.", 3 );

    unsigned short* p = (unsigned short*)buffer;
    int sum{0};

    while( len > 1 )
    {
        sum += *p++;
        len -= 2;
    }
    if( len == 1 )
    {
        unsigned short t{0};
        *( unsigned char* )( &t ) = *( unsigned char* )p;
        sum += t;
    }

    sum = ( sum >> 16 ) + ( sum & 0xffff );
    sum += (sum >> 16);
    return( ~sum );
}

int Ping( const char* target_str, unsigned short num = 1 )
{
    LogOutLine( "Ping start. num: " + to_string( num ), 3 );
    LogOutChars( target_str, 3 );

    //判断输入的是域名还是IP地址
    unsigned int addr = inet_addr( target_str );
    if( addr == INADDR_NONE )   //是域名
    {
        hostent* p_hostent = gethostbyname( target_str );
        if( !p_hostent || !p_hostent->h_addr )
        {
            LogOutLine( "Get host failed.", 1 );
            return -1;  //大概DNS没连上
        }

        memcpy( &addr, p_hostent->h_addr_list[0], p_hostent->h_length );
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_addr.s_addr = addr;

    //socket
    int sock_fd = socket( AF_INET, SOCK_RAW, IPPROTO_ICMP ); //注意需root权限。
    if( sock_fd < 0 )
    {
        LogOutLine( "Get socket failed.", 1 );
        return -2;  //八成系统本身故障。
    }

    int res = 0;

    for( int i = 0; i < num; i++ )
    { 
        //ICMP请求包初始化
        ICMPPack request;
        request.type       = 8;           //可达性判断
        request.code       = 0;           //要求回报
        request.checksum   = 0;
        request.identifier = getpid();    //截断可能，无碍
        request.seqnum     = i;           //序号

        struct timeval time;
        gettimeofday( &time, NULL );
        request.time = time.tv_usec;      //数据为发送时刻，微秒

        request.checksum = Checksum( &request, sizeof( request ) );

        if( -1 == sendto( sock_fd, &request, sizeof( request ), 0, ( struct sockaddr* )&dest_addr, sizeof( dest_addr ) ) )
        {
            res += 5; //发送时设备没网?
            LogOutLine( "Ping: sendto failed. res + 5", 3 );
            continue;
        }

        char recvbuf[1500];
        struct sockaddr_in src_addr;
        socklen_t size = sizeof( sockaddr_in ); //不初始化取不到发送地址值，大概是据此分配sockaddr的空间。

        if( -1 == recvfrom( sock_fd, recvbuf, sizeof( recvbuf ), 0, ( struct sockaddr* )&src_addr, &size ) )
        {
            res += 4; //接收时没网?
            LogOutLine( "Ping: recvfrom failed. res + 4", 3 );
            continue;
        }

        gettimeofday( &time, NULL );    //回报到达时间

        unsigned char h = recvbuf[0];
        h = ( h & 0x0f ) * 4;           //IP包头长度计算
        ICMPPack reply;
        memcpy( &reply, recvbuf + h, sizeof( reply ) );

        //不能直接getpid
        if( ( reply.identifier != request.identifier ) ||
            ( reply.type != 0 ) ||
            ( src_addr.sin_addr.s_addr != dest_addr.sin_addr.s_addr ) ||
            ( reply.seqnum != i ) )
        {
            res += 2; //回包有误
            LogOutLine( "Ping: reply error. res + 2", 3 );
            continue;
        }

        //海外网络有时候会延迟1秒，但此时网页还是可以凑合看。再多就是丢包丢得没法用了。
        unsigned long cost = ( time.tv_usec - reply.time ) / 1000;  //ms
        if( cost >  1500 )
        {
            LogOutLine( "Ping: reply slowly. res + 1", 3 );
            res += 1;   //通但速度过慢
        }
    }

    return res;
}

bool LoadConfigFile( string file_name, ConfItems& conf )
{
    LogOutLine( "LoadConfigFile "+ file_name +" start.", 3 );

    struct uci_context* p_context = uci_alloc_context();
    struct uci_package* p_pkg = NULL;

    if( uci_load( p_context, file_name.c_str(), &p_pkg ) != UCI_OK )
    {
        LogOutLine( "uci_load failed." );
        uci_free_context( p_context );
        return false;
    }

    struct uci_section* p_section = uci_lookup_section( p_context, p_pkg, "conf" );
    if( !p_section )
    {
        LogOutLine( "uci_lookup_section error." );
        uci_unload( p_context, p_pkg );
        uci_free_context( p_context );
        return false;
    }
    LogOutLine( "ServiceArg: conf", 2 );

    stringstream ss;
    const char* p_enabled = uci_lookup_option_string( p_context, p_section, "enabled" );
    if( !p_enabled )
    {
        LogOutLine( "Lookup option enabled error.", 1 );
        uci_unload( p_context, p_pkg );
        uci_free_context( p_context );
        return false;
    }
    conf.enabled = ( *p_enabled == '1' );
    LogOutLine( "enabled: " + to_string( conf.enabled ), 2 );

    const char* p_ping_target = uci_lookup_option_string( p_context, p_section, "ping_target" );
    if( !p_ping_target )
    {
        conf.ping_target = "www.bing.com";
    }
    else
    {
        ss.str( "" );ss.clear();
        ss << p_ping_target;
        getline( ss, conf.ping_target );
    }
    LogOutLine( "ping_target: " + conf.ping_target, 2 );

    const char* p_connect_exe = uci_lookup_option_string( p_context, p_section, "connect_exe" );
    if( !p_connect_exe )
    {
        LogOutLine( "Lookup option connect_exe error.", 1 );
        uci_unload( p_context, p_pkg );
        uci_free_context( p_context );
        return false;
    }
    else
    {
        ss.str( "" );ss.clear();
        ss << p_connect_exe;
        getline( ss, conf.connect_exe );
    }
    LogOutLine( "connect_exe: " + conf.connect_exe, 2 );

    const char* p_wwan_dev = uci_lookup_option_string( p_context, p_section, "wwan_dev" );
    if( !p_wwan_dev )
    {
        LogOutLine( "Lookup option wwan_dev error.", 1 );
        uci_unload( p_context, p_pkg );
        uci_free_context( p_context );
        return false;
    }
    else
    {
        ss.str( "" );ss.clear();
        ss << p_wwan_dev;
        getline( ss, conf.wwan_dev );
    }
    LogOutLine( "wwan_dev: " + conf.wwan_dev, 2 );

    const char* p_usb_dev = uci_lookup_option_string( p_context, p_section, "usb_dev" );
    if( !p_usb_dev )
    {
        LogOutLine( "Lookup option usb_dev error.", 1 );
        uci_unload( p_context, p_pkg );
        uci_free_context( p_context );
        return false;
    }
    else
    {
        ss.str( "" );ss.clear();
        ss << p_usb_dev;
        getline( ss, conf.usb_dev );
    }
    LogOutLine( "usb_dev: " + conf.usb_dev, 2 );

    uci_unload( p_context, p_pkg );
    uci_free_context( p_context );
    return true;
}

int main( int argc, char* argv[] )
{
    //log文件和log等级
    if( argc > 2 )
    {
        if ( !SetLogFile( argv[2] ) ) return 1;
    }
    else
    {
        if ( !SetLogFile( "./lte_status.log" ) ) return 1;
    }

    if( argc > 3 )
        SetOutLevel( atoi( argv[3] ) );
    else
        SetOutLevel( 0 );

    //读配置文件
    string conf_file_name = ( argc > 1 ) ? argv[1] : "./LTEServiceConf";
    ConfItems conf;
    if( !LoadConfigFile( conf_file_name, conf ) )
    {
        LogOutLine( "Load config file failed." );
        return 0;
    }
    if( !conf.enabled )
    {
        LogOutLine( "LTEService disabled." );
        return 0;
    }

    LogOutLine( "LTEService start." );

    int ping_interval = 30;  //ping时间间隔
    pid_t cm_pid = -1;

    while( conf.enabled )
    {
        //网络测试。
        int ping_res{-1};
        int ping_count = 4;
        ping_res = Ping( conf.ping_target.c_str(), ping_count );
        LogOutLine( "ping_res: " + to_string( ping_res ), 1 );

        //状态良好就直接退出
        if( ( ping_res < 4 ) && ( ping_res >= 0 ) )
        {
            sleep( ping_interval );
            continue;
        }

        //连接
        CLTEModemIf& modem = CLTEModemIf::GetInstance();
        if( !modem.InitModem( conf.usb_dev ) )
        {
            LogOutLine( "Modem offline." );
            break;
        }

        //网速不良，尝试重新连接基站并拨号
        if( ( ping_res >= 4 ) && ( ping_res <= 7 ) )
        {
            LogOutLine( "Too slow. Test reconnected." );

            if( ( modem.DeregisterFromLTE() == 0 ) && ( modem.AutomaticRegisterLTE() == 0 ) )
            {
                kill( cm_pid, 15 );
                cm_pid = -1;

                if( ( cm_pid = fork() ) < 0 )
                {
                    LogOutLine( "Connect manager exec failed." );
                    continue;
                }

                if( cm_pid == 0 )
                {
                    LogOutLine( "Connect manager exec start.", 1 );
                    int s = conf.connect_exe.find_last_of( '/' ) + 1;
                    LogOutChars( &conf.connect_exe[s], 3 );
                    execl( conf.connect_exe.c_str(), conf.connect_exe.c_str() + s, NULL ); //--------todo: args?
                }

                sleep( 20 );

                ping_res = Ping( conf.ping_target.c_str(), ping_count );

                //还过慢的话，恐怕也就这样了
                if( ( ping_res <= 7 ) && ( ping_res >= 0 ) )
                {
                    sleep( ping_interval );
                    continue;
                }
            }
        }

        //可以认为网已经没了。记录日志，检查状态，重新拨号
        if( cm_pid != -1 )
        {
            kill( cm_pid, 15 );
            cm_pid = -1;
            LogOutLine( "Last connect manager over." );
        }

        //socket获取失败了
        if( ping_res == -2 )
        {
            LogOutLine( "Create socket failed." );
            break;
        }

        //wwan设备脱线否
        if( !modem.CheckWwanDevStatus( conf.wwan_dev ) )
        {
            LogOutLine( "Wwan device lost." );
            break;
        }

        //sim卡状态检查
        bool sim_aru = false;
        for(int i = 0; i < 3; i++ )
        {
            sim_aru |= modem.CheckSimCardStatus();
            if( sim_aru ) break;
            else sleep( 2 );
        }
        if( !sim_aru )
        {
            LogOutLine( "SimCard lost." );
            break;
        }

        //信号强度检查
        int i = 0;
        for( ; i < 30; i++ )
        {
            if( modem.GetSignalStrengthLevel() > 0 )
            {
                break;
            }
            else
            {
                LogOutLine( "Signal weak." );
                sleep( 10 );
            }
        }

        //一直检查?
        while( i == 30 )
        {
            if( modem.GetSignalStrengthLevel() > 0 )
            {
                break;
            }
            else
            {
                LogOutLine( "Signal weak.", 1 );
                sleep( 120 );
            }
        }

        //拨号
        if( ( cm_pid = fork() ) < 0 )
        {
            LogOutLine( "Connect manager exec failed." );
            break;
        }
        if( cm_pid == 0 )
        {
            LogOutLine( "Connect manager exec start.", 1 );
            int s = conf.connect_exe.find_last_of( '/' ) + 1;
            LogOutChars( &conf.connect_exe[s], 3 );
            execl( conf.connect_exe.c_str(), conf.connect_exe.c_str() + s, NULL ); //--------todo: args?
        }
        sleep( 20 );
    }
    //while( conf.enabled ) end

    if( cm_pid != -1 )
    {
        kill( cm_pid, 15 );
        cm_pid = -1;

        LogOutLine( "Connect manager over." );
    }

    CloseLogFile();
    return 0;
}


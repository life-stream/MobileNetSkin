#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace std;

static int dbg_lv;
static ofstream log_file;
static stringstream ss;

void LogOutLine( const string& out_info, int log_lv )
{
    if( dbg_lv < log_lv ) return;
    if( !log_file.is_open() ) return;

    time_t timep;
    struct tm *p;
    time( &timep );
    p = localtime( &timep ); //取得当地时间

    log_file << std::setfill( '0' );
    log_file << "[" << 1900+p->tm_year << "-" << std::setw( 2 ) << 1+p->tm_mon << "-" << std::setw( 2 ) << p->tm_mday << " ";
    log_file << p->tm_hour << ":" << std::setw( 2 ) << p->tm_min << ":" << std::setw( 2 ) << p->tm_sec << "] ";
    log_file << out_info << endl;

    return;
}

void LogOutCharAsc( const char* out_info, unsigned int out_length, int log_lv )
{
    if( dbg_lv < log_lv ) return;
    if( !log_file.is_open() ) return;
    if( !out_info ) return;

    time_t timep;
    struct tm *p;
    time( &timep );
    p = localtime( &timep ); //取得当地时间

    log_file << std::setfill( '0' );
    log_file << "[" << 1900+p->tm_year << "-" << std::setw( 2 ) << 1+p->tm_mon << "-" << std::setw( 2 ) << p->tm_mday << " ";
    log_file << p->tm_hour << ":" << std::setw( 2 ) << p->tm_min << ":" << std::setw( 2 ) << p->tm_sec << "] " << endl;

    for( unsigned int i {0}; i < out_length; i++ )
        log_file << ( int )out_info[i] << " ";

    log_file << endl;
    log_file << out_length << endl;

    return;
}

void LogOutChars( const char* out_info, int log_lv )
{
    if( dbg_lv < log_lv ) return;
    if( !log_file.is_open() ) return;
    if( !out_info ) return;

    time_t timep;
    struct tm *p;
    time( &timep );
    p=localtime( &timep ); //取得当地时间

    log_file << std::setfill( '0' );
    log_file << "[" << 1900+p->tm_year << "-" << std::setw( 2 ) << 1+p->tm_mon << "-" << std::setw( 2 ) << p->tm_mday << " ";
    log_file << p->tm_hour << ":" << std::setw( 2 ) << p->tm_min << ":" << std::setw( 2 ) << p->tm_sec << "] ";

    ss.clear();ss.str( "" );
    ss << out_info;
    log_file << ss.str() << endl;

    return;
}

void SetOutLevel( int lv )
{
    dbg_lv = lv;
    if( dbg_lv > 3 ) dbg_lv = 1;
}

bool SetLogFile( string& file_name )
{
    if( file_name.empty() ) return false;

    log_file.open( file_name );
    if( log_file.fail() ) 
        return false;

    return true;
}

void CloseLogFile()
{
    log_file.close();
}

void DirectOutLine( const string& info )
{
    cout << info << endl;
}

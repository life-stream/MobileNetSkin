#ifndef __LOGOUT_H__
#define __LOGOUT_H__

#include <fstream>
#include <string>

extern void LogOutLine( std::string out_info, int log_lv = 0 );

extern void LogOutCharAsc( const char* out_info, unsigned int out_length, int log_lv = 0 );

extern void LogOutChars( const char* out_info, int log_lv = 0 );

extern void SetOutLevel( int lv );

extern bool SetLogFile( const char* p_file_name );

extern void CloseLogFile();

#endif

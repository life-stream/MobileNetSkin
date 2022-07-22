#ifndef __LTEMODEMIF_H__
#define __LTEMODEMIF_H__
#include <string>

class CLTEModemIf
{
public:
    static CLTEModemIf& GetInstance();
    virtual ~CLTEModemIf();

    bool InitModem( const std::string& dev_name );
    int  GetSignalStrengthLevel();
    bool CheckSimCardStatus();
    bool CheckWwanDevStatus( const std::string& dev_name );
    int  DeregisterFromMNet();
    int  AutoRegisterMNet();
    int  ResetUserEquipment();

    //读串口线程函数
    std::string GetAtCmdReply( const std::string& target );

private:
    CLTEModemIf();
    CLTEModemIf( const CLTEModemIf& );

    int m_fd;   //串口设备文件描述符

    static bool ms_get_reply;
};
#endif

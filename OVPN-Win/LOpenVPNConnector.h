/*
* Created on Dec 4, 2015
*
* @author: luodichen
*/

#pragma once

#include "LTAPManager.h"
#include "LVPNConnector.h"
#include "LStreamParser.h"
#include <string>

class LOpenVPNConnector : public LVPNConnector
{
public:
    typedef struct _CONFIG
    {
        typedef enum _MODE
        {
            TUN = 0,
            TAP = 1
        } MODE;

        typedef enum _PROTOCOL
        {
            TCP = 0,
            UDP = 1
        } PROTOCOL;

        MODE m_enMode;
        PROTOCOL m_enProtocol;
        std::string m_strServer;
        uint16_t m_uPort;
        std::string m_strCAFilePath;

        _CONFIG()
            : m_enMode(TUN)
            , m_enProtocol(TCP)
            , m_strServer("")
            , m_uPort(0)
            , m_strCAFilePath("")
        {

        }
    } CONFIG;

    typedef struct _ROUTE
    {
        bool m_blAddRoute;
        DWORD m_dwDest;
        DWORD m_dwMask;

        _ROUTE()
            : m_blAddRoute(false)
            , m_dwDest(0)
            , m_dwMask(0) 
        {

        }

        _ROUTE(DWORD dwDest, DWORD dwMask)
            : m_blAddRoute(true)
            , m_dwDest(dwDest)
            , m_dwMask(dwMask)
        {

        }
    } ROUTE;

    class StdOutParser : public LStreamParser
    {
    public:
        static const uint32_t M_LINE = 1;
        static const uint32_t M_CONNECTED = 2;
        static const uint32_t M_CONNECTION_RESET = 3;

    protected:
        virtual std::vector<MATCH_RULE> GetMatchRules();
        virtual void Matched(const char *szLine, uint32_t uRuleId);

    public:
        StdOutParser(LOpenVPNConnector *pConnector);

    private:
        LOpenVPNConnector *m_pConnector;
    };

public:
    LOpenVPNConnector(const char *szServer, Callback &callback, CONFIG config, ROUTE route);
    virtual ~LOpenVPNConnector();

public:
    std::string GetInterfaceName();
    NET_IFINDEX GetInterfaceIndex();

private:
    CONFIG m_config;
    ROUTE m_route;
    HANDLE m_hStdOutRd;
    HANDLE m_hStdErrRd;
    HANDLE m_hOVPNProcess;
    DWORD m_dwOVPNProcessId;
    StdOutParser m_objStdOutParser;
    LTAPManager::IF_INFO m_objIfInfo;

protected:
    virtual void RealConnect();
    virtual void RealDisconnect();

private:
    std::string MakeCommand(const char *szAdapter);
    static void ReadStdOutThread(void *data);
    static void ReadStdErrThread(void *data);

    int AddRoute();
    int RemoveRoute();
};


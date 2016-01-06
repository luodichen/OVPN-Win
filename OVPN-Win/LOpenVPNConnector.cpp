/*
* Created on Dec 4, 2015
*
* @author: luodichen
*/

#include "stdafx.h"
#include "lerror.h"
#include "LOpenVPNConnector.h"
#include "utils.h"

#include <atlbase.h>
#include <atlconv.h>
#include <string>

LOpenVPNConnector::LOpenVPNConnector(const char *szServer, Callback &callback, 
    LOpenVPNConnector::CONFIG config, LOpenVPNConnector::ROUTE route)
    : LVPNConnector(szServer, callback)
    , m_config(config)
    , m_route(route)
    , m_hStdOutRd(INVALID_HANDLE_VALUE)
    , m_hStdErrRd(INVALID_HANDLE_VALUE)
    , m_hOVPNProcess(INVALID_HANDLE_VALUE)
    , m_dwOVPNProcessId(0)
    , m_objStdOutParser(this)
    , m_objIfInfo()
{

}

LOpenVPNConnector::~LOpenVPNConnector()
{
    SyncDisconnect();
}

std::string LOpenVPNConnector::GetInterfaceName()
{
    return m_objIfInfo.m_name;
}

NET_IFINDEX LOpenVPNConnector::GetInterfaceIndex()
{
    return m_objIfInfo.m_index;
}

void LOpenVPNConnector::RealConnect()
{
    int nErrCode = err::NOERR;

    do 
    {
        LTAPManager manager;
        std::string strAdapterName;
        std::vector<LTAPManager::IF_INFO> vtAdapters;

        if (err::NOERR != (nErrCode = manager.GetAvailableAdapters(1, vtAdapters, false)))
            break;

        m_objIfInfo = vtAdapters[0];
        strAdapterName = m_objIfInfo.m_name;
        char szModulePath[MAX_PATH] = { 0 };
        utils::GetAppPath(szModulePath, sizeof(szModulePath));
        std::string strOVPNPath = std::string(szModulePath) + "\\openvpn.exe";

        SECURITY_ATTRIBUTES sa = { 0 };
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        HANDLE hStdOutRd = INVALID_HANDLE_VALUE, hStdOutWr = INVALID_HANDLE_VALUE;
        HANDLE hStdErrRd = INVALID_HANDLE_VALUE, hStdErrWr = INVALID_HANDLE_VALUE;
        HANDLE hStdInWr = INVALID_HANDLE_VALUE, hStdInRd = INVALID_HANDLE_VALUE;

        if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0)
            || !SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0)
            || !CreatePipe(&hStdErrRd, &hStdErrWr, &sa, 0)
            || !SetHandleInformation(hStdErrRd, HANDLE_FLAG_INHERIT, 0)
            || !CreatePipe(&hStdInRd, &hStdInWr, &sa, 0)
            || !SetHandleInformation(hStdInWr, HANDLE_FLAG_INHERIT, 0))
        {
            nErrCode = err::CREATE_HANDLE_FAIL;
            break;
        }

        PROCESS_INFORMATION pi = { 0 };
        STARTUPINFO si = { 0 };

        si.cb = sizeof(STARTUPINFO);
        si.hStdError = hStdErrWr;
        si.hStdOutput = hStdOutWr;
        si.hStdInput = hStdInRd;
        si.dwFlags |= STARTF_USESTDHANDLES;

        SetStatus(CONNECTING);
        if (!CreateProcess(CA2W(strOVPNPath.c_str()),
            CA2W(MakeCommand(strAdapterName.c_str()).c_str()),
            NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, CA2W(szModulePath), &si, &pi))
        {
            nErrCode = err::CREATE_PROCESS_FAIL;
            break;
        }

        SetHandleInformation(hStdOutWr, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdErrWr, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdInRd, HANDLE_FLAG_INHERIT, 0);

        m_hStdOutRd = hStdOutRd;
        m_hStdErrRd = hStdErrRd;
        m_hOVPNProcess = pi.hProcess;
        m_dwOVPNProcessId = pi.dwProcessId;

        _beginthread(ReadStdOutThread, 0, this);
        _beginthread(ReadStdErrThread, 0, this);

        std::string strUsernameInput = m_strUsername + "\n";
        std::string strPasswordInput = m_strPassword + "\n";

        DWORD dwWritten = 0;
        Sleep(1000);
        WriteFile(hStdInWr, strUsernameInput.c_str(), strUsernameInput.length(), &dwWritten, NULL);
        Sleep(500);
        WriteFile(hStdInWr, strPasswordInput.c_str(), strPasswordInput.length(), &dwWritten, NULL);

        WaitForSingleObject(pi.hProcess, INFINITE);

        m_hOVPNProcess = INVALID_HANDLE_VALUE;
        m_dwOVPNProcessId = 0;

        // Magic! The order of those handles is very important!
        HANDLE handles[] = {
            hStdOutWr, hStdInRd, hStdErrWr, hStdOutRd, 
            hStdInWr, hStdErrRd, pi.hProcess, pi.hThread
        };

        for (int i = 0; i < sizeof(handles) / sizeof(handles[0]); i++)
            CloseHandle(handles[i]);

    } while (false);

    OutputDebugStringA("Connector main thread exited\n");
    SetError(nErrCode);
    SetStatus(DISCONNECTED);

    if (err::NOERR != nErrCode)
        m_objCallback.OnFailed(*this);
}

void LOpenVPNConnector::RealDisconnect()
{
    SetStatus(DISCONNECTING);

    if (m_route.m_blAddRoute)
        utils::RemoveRoute(m_route.m_dwDest);

    TerminateProcess(m_hOVPNProcess, 0);

    /*
    FreeConsole();

    if (AttachConsole(m_dwOVPNProcessId))
    {
        SetConsoleCtrlHandler(NULL, true);
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);

        Sleep(500);
        FreeConsole();
        SetConsoleCtrlHandler(NULL, false);
    }
    */
}

std::string LOpenVPNConnector::MakeCommand(const char *szAdapter)
{
    std::string ret = "openvpn.exe --client ";
    if (LOpenVPNConnector::CONFIG::TUN == m_config.m_enMode)
        ret += "--dev tun ";
    else if (LOpenVPNConnector::CONFIG::TAP == m_config.m_enMode)
        ret += "--dev tap ";

    ret += "--dev-node " + std::string(szAdapter) + " ";

    if (LOpenVPNConnector::CONFIG::TCP == m_config.m_enProtocol)
        ret += "--proto tcp ";
    else if (LOpenVPNConnector::CONFIG::UDP == m_config.m_enProtocol)
        ret += "--proto udp ";

    ret += "--remote " + m_config.m_strServer + " ";
    if (m_config.m_uPort > 0)
    {
        char port[32] = { 0 };
        sprintf_s(port, sizeof(port), "%u", m_config.m_uPort);
        ret += std::string(port) + " ";
    }

    ret += "--ca \"" + m_config.m_strCAFilePath + "\" ";
    ret += "--keepalive 10 120 --cipher AES-256-CBC --auth SHA1 --auth-user-pass --verb 5";

    return ret;
}

void LOpenVPNConnector::ReadStdOutThread(void *data)
{
    LOpenVPNConnector *pSelf = (LOpenVPNConnector *)data;

    for (;;)
    {
        uint8_t buffer[16] = { 0 };
        DWORD dwBytesRead = 0;

        if (!ReadFile(pSelf->m_hStdOutRd, buffer, sizeof(buffer), &dwBytesRead, NULL)
            || 0 == dwBytesRead)
        {
            break;
        }

        pSelf->m_objStdOutParser.WriteStream((const char*)buffer, dwBytesRead);
    }

    OutputDebugStringA("ReadStdOutThread exited\n");
    pSelf->m_hStdOutRd = INVALID_HANDLE_VALUE;
}

void LOpenVPNConnector::ReadStdErrThread(void *data)
{
    LOpenVPNConnector *pSelf = (LOpenVPNConnector *)data;

    for (;;)
    {
        uint8_t buffer[16] = { 0 };
        DWORD dwBytesRead = 0;

        if (!ReadFile(pSelf->m_hStdErrRd, buffer, sizeof(buffer), &dwBytesRead, NULL)
            || 0 == dwBytesRead)
        {
            break;
        }
    }

    OutputDebugStringA("ReadStdErrThread exited\n");
    pSelf->m_hStdErrRd = INVALID_HANDLE_VALUE;
}

std::vector<LStreamParser::MATCH_RULE> LOpenVPNConnector::StdOutParser::GetMatchRules()
{
    static auto rules = std::vector<LStreamParser::MATCH_RULE>({
        LStreamParser::MATCH_RULE(M_LINE, "^.*$"),
        LStreamParser::MATCH_RULE(
            M_CONNECTED, "^.*Initialization Sequence Completed.*$"),
        LStreamParser::MATCH_RULE(
            M_CONNECTION_RESET, "^.*Connection reset, restarting.*$")
    });

    return rules;
}

void LOpenVPNConnector::StdOutParser::Matched(const char * szLine, uint32_t uRuleId)
{
    int nErrCode = err::NOERR;
    switch (uRuleId)
    {
    case M_LINE:
        OutputDebugStringA((std::string("[stdout] ") + szLine + "\n").c_str());
        m_pConnector->m_objCallback.MsgReceived(szLine);
        break;

    case M_CONNECTED:
        if (err::NOERR != (nErrCode = m_pConnector->AddRoute()))
        {
            m_pConnector->m_objCallback.OnFailed(*m_pConnector);
            m_pConnector->Disconnect();
            break;
        }

        m_pConnector->SetStatus(LVPNConnector::CONNECTED);
        break;

    case M_CONNECTION_RESET:
        if (LVPNConnector::CONNECTING == m_pConnector->GetStatus())
        {
            m_pConnector->SetError(err::USER_PASS_ERROR);
            m_pConnector->m_objCallback.OnFailed(*m_pConnector);
            m_pConnector->Disconnect();
        }
        break;
    }
}

LOpenVPNConnector::StdOutParser::StdOutParser(LOpenVPNConnector *pConnector)
    : LStreamParser()
    , m_pConnector(pConnector)
{

}

int LOpenVPNConnector::AddRoute()
{
    if (!m_route.m_blAddRoute)
        return err::NOERR;

    int nErrCode = err::NOERR;
    DWORD dwIndex = GetInterfaceIndex();

    DWORD dwDest = m_route.m_dwDest;
    DWORD dwMask = m_route.m_dwMask;

    if (err::NOERR != (nErrCode = utils::ForceAddIfRoute(dwDest, dwMask, dwIndex)))
    {
        return nErrCode;
    }

    return nErrCode;
}

int LOpenVPNConnector::RemoveRoute()
{
    return err::NOERR;
}
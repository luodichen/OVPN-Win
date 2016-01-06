/*
 * Created on Dec 4, 2015
 *
 * @author: luodichen
 */

#include "stdafx.h"

#include "utils.h"
#include <atlbase.h>
#include <atlconv.h>
#include <string>
#include <memory>
#include <Shlobj.h>
#include <regex>

bool utils::LoadStringResource(const wchar_t *wszCustomResName, int nResId, std::string &str)
{
    HGLOBAL hResourceLoaded;
    HRSRC   hRes;
    char    *lpResLock;
    DWORD   dwSizeRes;

    hRes = FindResource(NULL, MAKEINTRESOURCE(nResId), wszCustomResName);
    if (NULL == hRes)
        return false;

    hResourceLoaded = LoadResource(NULL, hRes);
    if (NULL == hResourceLoaded)
        return false;

    lpResLock = (char *)LockResource(hResourceLoaded);
    if (NULL == lpResLock)
        return false;

    dwSizeRes = SizeofResource(NULL, hRes);

    str.append(lpResLock, dwSizeRes);

    return true;
}

bool utils::ExtractBinResource(const wchar_t *wszCustomResName,
                                int nResId,
                                const wchar_t *wszOutputName)
{
    std::string data;
    if (!LoadStringResource(wszCustomResName, nResId, data))
        return false;

    FILE *fp = NULL;
    if (0 != fopen_s(&fp, CW2A(wszOutputName), "wb"))
        return false;

    fwrite((const void *)(data.c_str()), data.length(), 1, fp);
    fclose(fp);

    return true;
}

int utils::GetAppPath(char *szPath, int nLength)
{
    char szModuleName[MAX_PATH] = { 0 };
    HMODULE hModule = GetModuleHandleA(NULL);
    if (0 == hModule)
        return -1;

    if (0 == GetModuleFileNameA(hModule, szModuleName, sizeof(szModuleName)))
        return -1;

    std::string strModuleName = szModuleName;
    std::string strModulePath = strModuleName.substr(0, strModuleName.rfind("\\"));
    strcpy_s(szPath, nLength, strModulePath.c_str());

    return 0;
}

int utils::GetIfAdapterAddress(DWORD dwIfIndex, struct in_addr *pAddr)
{
    return GetIfAdapterInfo<struct in_addr *>(
        dwIfIndex, pAddr, [](auto pAddr, auto pAdapterAddresses) {

        do
        {
            int nErrCode = err::NOERR;
            auto pFirstUnicastAddress = pAdapterAddresses->FirstUnicastAddress;
            if (NULL == pFirstUnicastAddress
                || NULL == pFirstUnicastAddress->Address.lpSockaddr)
            {
                nErrCode = err::ADDRESS_NOT_FOUND;
                break;
            }
        
            sockaddr_in *pAddr_in = (sockaddr_in *)(pFirstUnicastAddress->Address.lpSockaddr);
            *pAddr = pAddr_in->sin_addr;
        } while (false);
        return 0;
    });
}

int utils::GetIfAdapterMetric(DWORD dwIfIndex, DWORD *pMetric)
{
    return GetIfAdapterInfo<DWORD *>(dwIfIndex, pMetric, 
        [](auto pMetric, auto pIpAdapterAddress) {
        *pMetric = pIpAdapterAddress->Ipv4Metric;
        return err::NOERR;
    });
}

int utils::FindIpForwardTableRow(DWORD dwDest, PMIB_IPFORWARDROW pRow, bool *pFound)
{
    int nErrCode = err::NOERR;
    do
    {
        ULONG ulSize = 0;
        if (ERROR_INSUFFICIENT_BUFFER != GetIpForwardTable(NULL, &ulSize, true))
        {
            nErrCode = err::READ_ROUTE_TABLE_FAIL;
            break;
        }

        auto pIpForwardTable = (PMIB_IPFORWARDTABLE)(new uint8_t[ulSize]);

        if (NULL == pIpForwardTable)
        {
            nErrCode = err::MEMORY_ALLOC_FAIL;
            break;
        }

        if (NO_ERROR != GetIpForwardTable(pIpForwardTable, &ulSize, true))
        {
            nErrCode = err::READ_ROUTE_TABLE_FAIL;
            delete[] pIpForwardTable;
            break;
        }

        *pFound = false;

        for (int i = 0; i < pIpForwardTable->dwNumEntries; i++)
        {
            if (pIpForwardTable->table[i].dwForwardDest == dwDest)
            {
                memcpy(pRow, &(pIpForwardTable->table[i]), sizeof(MIB_IPFORWARDROW));
                *pFound = true;
                break;
            }
        }

        delete[] pIpForwardTable;
    } while (false);

    return nErrCode;
}

int utils::RemoveRoute(DWORD dwDest)
{
    int nErrCode = err::NOERR;
    do
    {
        MIB_IPFORWARDROW row = { 0 };
        bool blExists = false;

        if (err::NOERR != (nErrCode = FindIpForwardTableRow(dwDest, &row, &blExists)))
            break;

        if (blExists)
            DeleteIpForwardEntry(&row);
    } while (false);

    return nErrCode;
}

int utils::ForceAddRoute(DWORD dwDestination, DWORD dwSubnetMask, DWORD dwGateway)
{
    int nErrCode = err::NOERR;
    IF_INDEX ifindex = 0;

    do 
    {
        if (NO_ERROR != GetBestInterface(dwGateway, &ifindex))
        {
            nErrCode = err::GET_BEST_INTERFACE_FAIL;
            break;
        }

        if (err::NOERR != (nErrCode = RemoveRoute(dwDestination)))
            break;

        DWORD dwMetric = 0;
        if (err::NOERR != (nErrCode = GetIfAdapterMetric(ifindex, &dwMetric)))
        {
            break;
        }

        MIB_IPFORWARDROW row = { 0 };

        row.dwForwardDest = dwDestination;
        row.dwForwardMask = dwSubnetMask;
        row.dwForwardNextHop = dwGateway;
        row.dwForwardIfIndex = ifindex;
        row.dwForwardMetric1 = dwMetric;
        row.dwForwardProto = MIB_IPPROTO_NETMGMT;

        if (NO_ERROR != CreateIpForwardEntry(&row))
        {
            nErrCode = err::CREATE_ROUTE_FAIL;
            break;
        }
    } while (false);

    return nErrCode;
}

int utils::ForceAddIfRoute(DWORD dwDestination, DWORD dwSubnetMask, DWORD dwIfIndex)
{
    int nErrCode = err::NOERR;
    in_addr addr = { 0 };

    if (err::NOERR != (nErrCode = utils::GetIfAdapterAddress(dwIfIndex, &addr)))
    {
        return nErrCode;
    }

    DWORD dwGateway = htonl(ntohl(*((DWORD*)&addr)) - 1);

    if (err::NOERR != (nErrCode = utils::ForceAddRoute(dwDestination, 
        dwSubnetMask, dwGateway)))
    {
        return nErrCode;
    }

    return nErrCode;
}

std::string utils::GetCAFilePath(const char * szName)
{
    char szTempPath[MAX_PATH] = { 0 };
    GetTempPathA(sizeof(szTempPath), szTempPath);
    
    std::string strPath = std::string(szTempPath) + "\\hs-tunnel";

    if (!PathFileExistsA(strPath.c_str()))
    {
        CreateDirectoryA(strPath.c_str(), NULL);
    }

    return strPath + "\\" + szName;
}

int utils::RemoveRasEntryDefaultRoute(const char *szEntryName)
{
    int nErrCode = err::NOERR;
    DWORD dwRet = 0;

    wchar_t wszPath[MAX_PATH] = { 0 };
    if (S_OK != (dwRet = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, wszPath)))
    {
        nErrCode = err::GET_FILE_PATH_FAIL | dwRet;
        return nErrCode;
    }

    std::string strPbkPath = std::string(CW2A(wszPath)) + "\\Microsoft\\Network\\Connections\\Pbk\\rasphone.pbk";
    
    int nValue = GetPrivateProfileInt(CA2W(szEntryName), 
        _T("IpPrioritizeRemote"), 1, CA2W(strPbkPath.c_str()));

    if (0 != nValue)
    {
        if (!WritePrivateProfileString(CA2W(szEntryName), _T("IpPrioritizeRemote"),
            _T("0"), CA2W(strPbkPath.c_str())))
        {
            nErrCode = err::WRITE_ENTRY_PROFILE_FAIL | GetLastError();
            return nErrCode;
        }
    }

    return nErrCode;
}

bool utils::IsVaildIpAddressString(const char * szTest)
{
    const std::regex pattern("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
    return std::regex_match(szTest, pattern);
}

std::string utils::GBKToUTF8(const std::string &strGBK)
{
    std::string strOutUTF8 = "";
    WCHAR * str1;
    int n = MultiByteToWideChar(CP_ACP, 0, strGBK.c_str(), -1, NULL, 0);
    str1 = new WCHAR[n];
    MultiByteToWideChar(CP_ACP, 0, strGBK.c_str(), -1, str1, n);
    n = WideCharToMultiByte(CP_UTF8, 0, str1, -1, NULL, 0, NULL, NULL);
    char *str2 = new char[n];
    WideCharToMultiByte(CP_UTF8, 0, str1, -1, str2, n, NULL, NULL);
    strOutUTF8 = str2;
    delete[]str1;
    str1 = NULL;
    delete[]str2;
    str2 = NULL;
    return strOutUTF8;
}

std::string utils::UTF8ToGBK(const std::string &strUTF8)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, strUTF8.c_str(), -1, NULL, 0);
    wchar_t *wszGBK = new wchar_t[len + 1];
    memset(wszGBK, 0, len * 2 + 2);
    MultiByteToWideChar(CP_UTF8, 0, strUTF8.c_str(), -1, wszGBK, len);

    len = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);
    char *szGBK = new char[len + 1];
    memset(szGBK, 0, len + 1);
    WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, len, NULL, NULL);

    std::string strTemp(szGBK);
    delete[]szGBK;
    delete[]wszGBK;
    return strTemp;
}
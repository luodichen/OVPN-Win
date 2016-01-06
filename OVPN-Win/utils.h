/*
* Created on Dec 4, 2015
*
* @author: luodichen
*/

#pragma once

#include <string>
#include <functional>
#include "lerror.h"

namespace utils
{
    bool LoadStringResource(const wchar_t *wszCustomResName, int nResId, std::string &str);

    bool ExtractBinResource(const wchar_t *wszCustomResName,
        int nResourceId,
        const wchar_t *wszOutputName);

    int GetAppPath(char *szPath, int nLength);
    int GetIfAdapterAddress(DWORD dwIfIndex, struct in_addr *pAddr);
    int GetIfAdapterMetric(DWORD dwIfIndex, DWORD *pMetric);
    int FindIpForwardTableRow(DWORD dwDest, PMIB_IPFORWARDROW pRow, bool *pFound);
    int RemoveRoute(DWORD dwDest);
    int ForceAddRoute(DWORD dwDestination, DWORD dwSubnetMask, DWORD dwGateway);
    int ForceAddIfRoute(DWORD dwDestination, DWORD dwSubnetMask, DWORD dwIfIndex);
    std::string GetCAFilePath(const char *szName);
    int RemoveRasEntryDefaultRoute(const char *szEntryName);
    bool IsVaildIpAddressString(const char *szTest);

    template<class T>
    int GetIfAdapterInfo(DWORD dwIfIndex, T out, std::function<int(T, PIP_ADAPTER_ADDRESSES)> fPick)
    {
        int nErrCode = err::NOERR;
        ULONG ulSize = 0;
        PIP_ADAPTER_ADDRESSES pResult = NULL;

        if (ERROR_BUFFER_OVERFLOW != GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &ulSize))
            return err::GET_ADAPTER_ADDRESS_FAIL;

        pResult = (PIP_ADAPTER_ADDRESSES)(new uint8_t[ulSize]);
        if (NULL == pResult)
            return err::MEMORY_ALLOC_FAIL;

        do
        {
            if (NO_ERROR != GetAdaptersAddresses(AF_INET, 0, NULL, pResult, &ulSize))
            {
                nErrCode = err::GET_ADAPTER_ADDRESS_FAIL;
                break;
            }

            auto pCur = pResult;
            nErrCode = err::ADAPTER_NOT_FOUND;

            while (NULL != pCur)
            {
                if (dwIfIndex == pCur->IfIndex)
                {
                    nErrCode = fPick(out, pCur);
                    break;
                }
                pCur = pCur->Next;
            }
        } while (false);

        delete[] pResult;

        return nErrCode;
    }

    std::string GBKToUTF8(const std::string &strGBK);
    std::string UTF8ToGBK(const std::string &strUTF8);
}

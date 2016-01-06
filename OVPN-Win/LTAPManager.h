/*
* Created on Dec 4, 2015
*
* @author: luodichen
*/

#pragma once

#include <Windows.h>
#include <Iphlpapi.h>
#include <string>
#include <vector>


class LTAPManager
{
public:
    LTAPManager();
    virtual ~LTAPManager();

public:
    typedef struct _IF_INFO
    {
        NET_IFINDEX m_index;
        std::string m_name;

        _IF_INFO() {}

        _IF_INFO(NET_IFINDEX index, const char *szName)
            : m_index(index), m_name(szName)
        {

        }
    } IF_INFO;

public:
    int GetAvailableAdapter(PMIB_IFROW pTable);
    int GetAvailableAdapterName(std::string &strName);

    int GetAvailableAdapters(int nCount, 
        std::vector<IF_INFO> &vtAdapters, bool blForce);

    static std::string GetAdapterNameFromIfRow(const PMIB_IFROW pRow);
    static std::string GetAdapterNameFromIndex(NET_IFINDEX ulIndex);

private:
    int DetectFreeAdapter(PMIB_IFROW pTable, bool *pFound);
    int DetectFreeAdapter(std::vector<IF_INFO> &vtAdapters);
    int CreateNewAdapter();

private:
    static const char *TARGET_INTERFACE;
};

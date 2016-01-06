/*
* Created on Dec 6, 2015
*
* @author: luodichen
*/

#pragma once

#include <vector>
#include <string>

class LStreamParser
{
public:
    typedef struct _MATCH_RULE
    {
        uint32_t m_uRuleId;
        std::string m_strRegex;

        _MATCH_RULE()
            : m_uRuleId((uint32_t)-1)
            , m_strRegex("")
        {

        }

        _MATCH_RULE(uint32_t uRuleId, const char *szRegex)
            : m_uRuleId(uRuleId)
            , m_strRegex(szRegex)
        {

        }
    } MATCH_RULE;

public:
    LStreamParser();
    virtual ~LStreamParser();

public:
    virtual void WriteStream(const char *pData, size_t size);

protected:
    virtual void Matched(const char *szLine, uint32_t uRuleId) = 0;
    virtual std::vector<MATCH_RULE> GetMatchRules();

private:
    void DetectMatch(std::string strLine);

private:
    std::string m_strLineBuffer;
};


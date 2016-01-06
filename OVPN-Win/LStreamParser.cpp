/*
* Created on Dec 6, 2015
*
* @author: luodichen
*/

#include "stdafx.h"
#include "LStreamParser.h"

#include <regex>

LStreamParser::LStreamParser()
    : m_strLineBuffer()
{

}

LStreamParser::~LStreamParser()
{

}

std::vector<LStreamParser::MATCH_RULE> LStreamParser::GetMatchRules()
{
    return std::vector<LStreamParser::MATCH_RULE>();
}

void LStreamParser::WriteStream(const char *pData, size_t size)
{
    for (int i = 0; i < size; i++)
    {
        if (('\r' == pData[i] || '\n' == pData[i]) && m_strLineBuffer.length() > 0)
        {
            m_strLineBuffer.append("\0", 1);
            DetectMatch(m_strLineBuffer);
            m_strLineBuffer.clear();
        }
        else if ('\r' != pData[i] && '\n' != pData[i])
        {
            m_strLineBuffer.append(pData + i, 1);
        }
    }
}

void LStreamParser::DetectMatch(std::string strLine)
{
    for (auto rule : GetMatchRules())
    {
        const std::regex pattern(rule.m_strRegex);
        if (std::regex_match(strLine, pattern))
        {
            Matched(strLine.c_str(), rule.m_uRuleId);
        }
    }
}

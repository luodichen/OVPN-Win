
// OVPN-Win.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// LOVPNWinApp: 
// �йش����ʵ�֣������ OVPN-Win.cpp
//

class LOVPNWinApp : public CWinApp
{
public:
	LOVPNWinApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern LOVPNWinApp theApp;
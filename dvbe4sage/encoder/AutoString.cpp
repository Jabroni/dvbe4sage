#include "StdAfx.h"
#include "AutoString.h"

CAutoString::CAutoString(int len)
{
	m_pBuffer = new char[len];
	memset(m_pBuffer,0,len);
}

CAutoString::~CAutoString(void)
{
	delete [] m_pBuffer;
}

char *CAutoString::GetBuffer() 
{
	return m_pBuffer;
}

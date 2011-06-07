#pragma once

class CAutoString
{
public:
	CAutoString(int len);
	~CAutoString(void);

	char *GetBuffer() ;

private:
	char *m_pBuffer;
};

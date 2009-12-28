#pragma once

// This class is reponsible for decryption via calling to FFDECSA dll
class Decrypter
{
private:
	void*						m_KeySet;
	int							m_Parallelism;
public:
	Decrypter(void);
	virtual ~Decrypter(void);
	void setKeys(const BYTE* const oddKey, const BYTE* const evenKey);
	void decrypt(BYTE* packets, ULONG count);
};

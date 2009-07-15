#pragma once

#include "si_tables.h"

// This class is reponsible for decryption via calling to FFDECSA dll
class Decrypter
{
private:
	// Structures used by FFDECSA
	struct TFFDeCsaCluster
	{
		BYTE* startBuffer;
		BYTE* endBuffer;
	};

	// Functionss of FFDECSA
	typedef TFFDeCsaCluster TFFDeCsaClusters[8192];
	typedef TFFDeCsaClusters *PFFDeCsaClusters;
	typedef void* (*get_key_struct)(void);
	typedef void (*free_key_struct)(void*);
	typedef int (*get_internal_parallelism)(void);
	typedef int (*get_suggested_cluster_size)(void);
	typedef void (*set_control_words)(void *keys, const BYTE* const even, const BYTE* const odd);
	typedef int (*decrypt_packets)(void *keys, unsigned char **cluster);

	HMODULE						m_hDeCSADll;
	set_control_words			m_pf_set_control_words;
	decrypt_packets				m_pf_decrypt_packets;
	get_key_struct				m_pf_get_key_struct;
	free_key_struct				m_pf_free_key_struct;
	get_internal_parallelism	m_pf_get_internal_parallelism;

	void*						m_KeySet;
	int							m_Parallelism;

	bool						m_isInitialized;

public:
	Decrypter(void);
	virtual ~Decrypter(void);
	void setKeys(const BYTE* const oddKey, const BYTE* const evenKey);
	void decrypt(BYTE* packets, ULONG count);
};

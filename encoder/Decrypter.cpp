#include "StdAfx.h"
#include "Decrypter.h"
#include "Logger.h"
#include "configuration.h"

Decrypter::Decrypter(void) :
	m_KeySet(NULL),
	m_Parallelism(1),
	m_isInitialized(false)
{
	// Load FFDECSA dll
	m_hDeCSADll = LoadLibrary(g_Configuration.getDECSADllName());
	if(m_hDeCSADll != NULL)
	{
		// Get all function addresses
		m_pf_set_control_words = (set_control_words)GetProcAddress(m_hDeCSADll, "set_control_words");
		m_pf_decrypt_packets = (decrypt_packets)GetProcAddress(m_hDeCSADll, "decrypt_packets");
		m_pf_get_key_struct = (get_key_struct)GetProcAddress(m_hDeCSADll, "get_key_struct");
		m_pf_free_key_struct = (free_key_struct)GetProcAddress(m_hDeCSADll, "free_key_struct");
		m_pf_get_internal_parallelism = (get_internal_parallelism)GetProcAddress(m_hDeCSADll, "get_internal_parallelism");

		// If everything is OK
		if(m_pf_set_control_words != NULL && m_pf_decrypt_packets != NULL && m_pf_get_key_struct != NULL && m_pf_free_key_struct != NULL && m_pf_get_internal_parallelism != NULL)
		{
			// Indicate the decrypter is initialized
			m_isInitialized = true;
			// Allocate key set
			m_KeySet = m_pf_get_key_struct();
			// Get parallelism
			m_Parallelism = m_pf_get_internal_parallelism();
			g_Logger.log(1, true, TEXT("DECSA DLL=\"%s\", parallelism = %d\n"), g_Configuration.getDECSADllName(), m_Parallelism);	
		}
		BYTE key[8];
		key[0] = 0x14; key[1] = 0x89; key[2] = 0x5E; key[3] = 0xFB;
		key[4] = 0x61; key[5] = 0xB5; key[6] = 0x31; key[7] = 0x47;
		m_pf_set_control_words(m_KeySet, key, key);
	}
}

Decrypter::~Decrypter(void)
{
	if(m_isInitialized)
		// Delete the key set
		m_pf_free_key_struct(m_KeySet);
	// Unload library
	if(m_hDeCSADll != NULL)
		FreeLibrary(m_hDeCSADll);
}

// This function sets the keys for the decryptor
void Decrypter::setKeys(const BYTE* oddKey,
						const BYTE* evenKey)
{
	// Check if initialized
	if(m_isInitialized)
		// Set the keys
		m_pf_set_control_words(m_KeySet, evenKey, oddKey);
}

// This function decrypts
void Decrypter::decrypt(BYTE* packets,
						ULONG count)
{
	// Check if initialized
	if(m_isInitialized)
	{
		// Initialize clusters
		TFFDeCsaCluster* cluster = new TFFDeCsaCluster[count + 1];
		for(ULONG i = 0; i < count; i++)
		{
			cluster[i].startBuffer = packets + i * TS_PACKET_LEN;
			cluster[i].endBuffer = cluster[i].startBuffer + TS_PACKET_LEN;
		}
		
		// Indicate the end
		cluster[count].startBuffer = NULL;
		cluster[count].endBuffer = NULL;

		// Loop through decryptor till done
		int decrypted = 0;
		do
		{
			decrypted = m_pf_decrypt_packets(m_KeySet, (BYTE**)cluster);
		}
		while(decrypted != 0);

		// Delete the cluster
		delete [] cluster;
	}
}

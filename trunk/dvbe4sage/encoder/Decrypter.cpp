#include "StdAfx.h"

#include "../FFdecsa/FFdecsa.h"
#include "si_tables.h"
#include "Decrypter.h"
#include "Logger.h"
#include "configuration.h"

// Structures used by FFDECSA
struct TFFDeCsaCluster
{
	BYTE* startBuffer;
	BYTE* endBuffer;
};

Decrypter::Decrypter(void) :
	m_KeySet(NULL),
	m_Parallelism(1)
{
	// Allocate key set
	m_KeySet = get_key_struct();
	// Get parallelism
	m_Parallelism = get_internal_parallelism();
}

Decrypter::~Decrypter(void)
{
	// Delete the key set
	free_key_struct(m_KeySet);
}

// This function sets the keys for the decryptor
void Decrypter::setKeys(const BYTE* const oddKey,
						const BYTE* const evenKey)
{
	// Set the keys
	set_control_words(m_KeySet, evenKey, oddKey);
}

// This function decrypts
void Decrypter::decrypt(BYTE* packets,
						ULONG count)
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
		decrypted = decrypt_packets(m_KeySet, (BYTE**)cluster);
	}
	while(decrypted != 0);

	// Delete the cluster
	delete [] cluster;
}

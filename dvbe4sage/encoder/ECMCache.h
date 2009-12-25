#pragma once

#define PACKET_SIZE	184

using namespace std;
using namespace stdext;

union Dcw
{
	__int64 number;
	BYTE key[8];
};

// An auxiliaury structure defining the basic data piece to be store in the cache
struct ECMDCWPair
{
	BYTE		ecm[PACKET_SIZE];		// The ECM packet content
	Dcw			dcw;					// The corresponding DCW
	bool		isOddKey;				// TRUE is the key is ODD, FALSE if EVEN
	time_t		timeStamp;				// The time stamp - for size keeping

	ECMDCWPair()
	{
		memset(ecm, (int)'\xFF', sizeof(ecm));
		dcw.number = 0;
		isOddKey = false;
		timeStamp = 0;
	}

	bool operator==(const ECMDCWPair& other) const
	{
		return memcmp(ecm, other.ecm, sizeof(ecm)) == 0 && dcw.number == other.dcw.number && isOddKey == other.isOddKey;
	}
};

class ECMCache
{
private:
	// Here we keep all the cached data, the key is CRC32 of the ECM packet
	hash_multimap<unsigned __int32, ECMDCWPair>		m_Data;

	// Here we keep the order in which the ECMs in questions came in, for size keeping
	list<unsigned __int32>							m_OrderOfData;

	// Max cache size
	USHORT											m_MaxCacheSize;

	// Autodelete chunk size
	USHORT											m_AutodeleteChunkSize;

	// We don't need either default or copy constructors
	ECMCache();
	ECMCache(const ECMCache&);

public:
	// The only valid constructor
	ECMCache(USHORT maxCacheSize, USHORT autodeleteChunkSize) : m_MaxCacheSize(maxCacheSize), m_AutodeleteChunkSize(autodeleteChunkSize) {}

	// Add a pair of ECM packet and DCW to the cache
	void add(const BYTE* ecmPacket, const Dcw& dcw, bool isOddKey);

	// Find if the packet is in the cache (if no, retrieve a reference to dummy Dcw)
	bool find(const BYTE* ecmPacket, list<const ECMDCWPair*>& result) const;

	// Dump the content of the cache to a file
	bool DumpToFile(LPCTSTR fileName, LPTSTR reason) const;

	// Read the content of the cache from a file
	bool ReadFromFile(LPCTSTR fileName, LPTSTR reason);
};

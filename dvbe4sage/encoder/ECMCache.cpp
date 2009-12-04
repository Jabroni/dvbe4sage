#include "StdAfx.h"

#include "ECMCache.h"
#include "crc32.h"

const Dcw dummyDcw = { 0 };

// Add a pair of ECM packet and DCW to the cache
void ECMCache::add(const BYTE* ecmPacket,
				   const Dcw& dcw,
				   bool isOddKey)
{
	// Let's see if we need to make some space in the cache by removing old elements
	if(m_OrderOfData.size() == m_MaxCacheSize)
	{
		// Do this for number of times equal to m_AutodeleteChunkSize
		for(USHORT i = 0; i < m_AutodeleteChunkSize; i++)
		{
			// Find the first hash value to remove
			__int32 hashToRemove = m_OrderOfData.front();

			// Get the current time stamp
			time_t earliestTimeStamp = 0;
			time(&earliestTimeStamp);

			// Initialize iterator with the place for removal
			hash_multimap<__int32, ECMDCWPair>::iterator removeIt = m_Data.end();

			// Find the entry with the matching hash and the earliest time stamp
			for(hash_multimap<__int32, ECMDCWPair>::iterator it = m_Data.find(hashToRemove); it != m_Data.end(); it++)
				if(difftime(it->second.timeStamp, earliestTimeStamp) < 0)
				{
					// If the earliest time stamp, remember it
					earliestTimeStamp = it->second.timeStamp;
					// And save the place where the pair needed to be removed is located
					removeIt = it;
				}

			// Now we can remove the entry
			if(removeIt != m_Data.end())
				m_Data.erase(removeIt);

			// And remove the bookkeeping entry
			m_OrderOfData.pop_front();
		}
	}

	// Now we can add the new data pair
	ECMDCWPair newPair;

	// Copy the data
	memcpy(newPair.ecm, ecmPacket, PACKET_SIZE);
	newPair.dcw = dcw;
	newPair.isOddKey = isOddKey;

	// Compute the hash
	__int32 newHash = _dvb_crc32(ecmPacket, PACKET_SIZE);

	// Get the current time stamp
	time_t now = 0;
	time(&now);

	// And, finally, add the pair to the map
	m_Data.insert(pair<__int32, ECMDCWPair>(newHash, newPair));
}

// Find if the packet is in the cache (if no, retrieve a reference to dummy Dcw)
const Dcw& ECMCache::find(const BYTE* ecmPacket,
						  bool& isOddKey) const
{
	// Compute the incoming packet hash
	__int32 hashInQuestion = _dvb_crc32(ecmPacket, PACKET_SIZE);

	// Find the entry with the matching hash
	for(hash_multimap<__int32, ECMDCWPair>::const_iterator it = m_Data.find(hashInQuestion); it != m_Data.end(); it++)
		// Now, check whether the actual packet really matches
		if(memcmp(it->second.ecm, ecmPacket, PACKET_SIZE) == 0)
			{
				// If yes, return the corresponding DCW
				isOddKey = it->second.isOddKey;
				return it->second.dcw;
			}

	// We found nothing, return the dummy DCW
	return dummyDcw;
}

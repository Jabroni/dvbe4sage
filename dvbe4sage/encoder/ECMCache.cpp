#include "StdAfx.h"

#include "ECMCache.h"
#include "crc32.h"
#include "ecmcache-pskel.hxx"

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
			unsigned __int32 hashToRemove = m_OrderOfData.front();

			// Get the current time stamp
			time_t earliestTimeStamp = 0;
			time(&earliestTimeStamp);

			// Initialize iterator with the place for removal
			hash_multimap<unsigned __int32, ECMDCWPair>::iterator removeIt = m_Data.end();

			// Find the entry with the matching hash and the earliest time stamp
			for(hash_multimap<unsigned __int32, ECMDCWPair>::iterator it = m_Data.find(hashToRemove); it != m_Data.end(); it++)
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
	unsigned __int32 newHash = _dvb_crc32(ecmPacket, PACKET_SIZE);

	// Let's see if we don't already have this ECM packet
	for(hash_multimap<unsigned __int32, ECMDCWPair>::const_iterator it = m_Data.find(newHash); it != m_Data.end(); it++)
		// Now, check whether the actual packet really matches
		if(memcmp(it->second.ecm, ecmPacket, PACKET_SIZE) == 0)
			return;

	// Now we're sure the pair is indeed new
	// Get the current time stamp
	time_t now = 0;
	time(&now);

	// And, finally, add the pair to the map
	m_Data.insert(pair<unsigned __int32, ECMDCWPair>(newHash, newPair));
}

// Find if the packet is in the cache (if no, retrieve a reference to dummy Dcw)
const Dcw& ECMCache::find(const BYTE* ecmPacket,
						  bool& isOddKey) const
{
	// Compute the incoming packet hash
	__int32 hashInQuestion = _dvb_crc32(ecmPacket, PACKET_SIZE);

	// Find the entry with the matching hash
	for(hash_multimap<unsigned __int32, ECMDCWPair>::const_iterator it = m_Data.find(hashInQuestion); it != m_Data.end(); it++)
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

// Dump the content of the cache to a file
bool ECMCache::DumpToFile(LPCTSTR fileName,
						  string& reason) const
{
	// Try opening the file
	FILE* outFile = NULL;
	_tfopen_s(&outFile, fileName, TEXT("wt"));
	if(outFile != NULL)
	{
		_ftprintf(outFile, TEXT("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));
		_ftprintf(outFile, TEXT("<cache>\n"));
		for(hash_multimap<unsigned __int32, ECMDCWPair>::const_iterator it = m_Data.begin(); it != m_Data.end(); it++)
		{
			_ftprintf(outFile, TEXT("\t<keypair>\n"));
			_ftprintf(outFile, TEXT("\t\t<ecm>"));
			// Fixme : change it to the real ECM packet size!
			for(int i = 0; i < PACKET_SIZE; i++)
				_ftprintf(outFile, TEXT("%.02X"), (UINT)it->second.ecm[i]);
			_ftprintf(outFile, TEXT("</ecm>\n"));
			_ftprintf(outFile, TEXT("\t\t<dcw>"));
			for(int i = 0; i < 8; i++)
				_ftprintf(outFile, TEXT("%.02X"), (UINT)it->second.dcw.key[i]);
			_ftprintf(outFile, TEXT("</dcw>\n"));
			_ftprintf(outFile, TEXT("\t\t<parity>%c</parity>\n"), it->second.isOddKey ? TCHAR('O') : TCHAR('E'));
			_ftprintf(outFile, TEXT("\t</keypair>\n"));
		}
		_ftprintf(outFile, TEXT("</cache>\n"));
		fclose(outFile);
		return true;
	}
	else
	{
		// We get here if for some reason we're unable to open the file
		stringstream result;
		result << "Cannot open the output file \"" << fileName << "\", error code = " << errno;
		reason = result.str();
		return false;
	}
}

// Implementation classes for XML schema elements
class ecm_pimpl : public ecm_pskel, public xml_schema::string_pimpl
{
private:
	string	m_String;
public:
	virtual void post_ecm()				{ m_String = post_string(); }
	const string& getString() const		{ return m_String; }
};

class dcw_pimpl : public dcw_pskel, public xml_schema::string_pimpl
{
private:
	string	m_String;
public:
	virtual void post_dcw()				{ m_String = post_string(); }
	const string& getString() const		{ return m_String; }
};

class parity_pimpl : public parity_pskel, public xml_schema::string_pimpl
{
private:
	string	m_String;
public:
	virtual void post_parity()			{ m_String = post_string(); }
	const string& getString() const		{ return m_String; }
};

class Keypair_pimpl : public Keypair_pskel
{
public:
	Keypair_pimpl()
	{
		ecm_parser(m_EcmParser);
		dcw_parser(m_DcwParser);
		parity_parser(m_ParityParser);
	}
	virtual void ecm()
	{
		const string& content = ((ecm_pimpl*)ecm_parser_)->getString();
		for(UINT i = 0; i < content.length() - 1; i += 2)
		{
			UINT number;
			static char buf[5] = { '0', '0', '\0', '\0', '\0' };
			buf[2] = content[i];
			buf[3] = content[i + 1];
			sscanf_s(buf, "%x", &number);
			m_Pair.ecm[i >> 1] = (BYTE)number;
		}
	}
	virtual void dcw()
	{
		const string& content = ((dcw_pimpl*)dcw_parser_)->getString();
		for(UINT i = 0; i < content.length() - 1; i += 2)
		{
			UINT number;
			static char buf[5] = { '0', '0', '\0', '\0', '\0' };
			buf[2] = content[i];
			buf[3] = content[i + 1];
			sscanf_s(buf, "%x", &number);
			m_Pair.dcw.key[i >> 1] = (BYTE)number;
		}
	}
	virtual void parity()
	{
		const string& content = ((parity_pimpl*)parity_parser_)->getString();
		m_Pair.isOddKey = (content[0] == 'O');
	}
	const BYTE* const getEcm() const	{ return m_Pair.ecm; }
	const Dcw& getDcw() const			{ return m_Pair.dcw; }
	bool getIsOddKey() const			{ return m_Pair.isOddKey; } 
private:
	ecm_pimpl		m_EcmParser;
	dcw_pimpl		m_DcwParser;
	parity_pimpl	m_ParityParser;
	ECMDCWPair		m_Pair;
};

// Helper class for reading the saved XML cache file
class cache_pimpl : public cache_pskel
{
private:
	ECMCache&		m_Cache;
	Keypair_pimpl	m_KeypairParser;

	// We don't need default and copy constructors
	cache_pimpl();
	cache_pimpl(const cache_pimpl&);
public:
	cache_pimpl(ECMCache& cache) : m_Cache(cache)
	{
		keypair_parser(m_KeypairParser);
	}
	virtual void keypair()
	{
		m_Cache.add(((Keypair_pimpl*)keypair_parser_)->getEcm(),
					((Keypair_pimpl*)keypair_parser_)->getDcw(),
					((Keypair_pimpl*)keypair_parser_)->getIsOddKey());
	}
};

// Read the content of the cache from a file
bool ECMCache::ReadFromFile(LPCTSTR fileName,
							string& reason)
{
	try
	{
		// Cache parser implementation needs the ECMCache to add key pairs
		cache_pimpl cacheParser(*this);
		// The root element is supposed to be "<cache>"
		xml_schema::document doc(cacheParser, "cache");
		// Do the usual parser procedure
		cacheParser.pre();
		// Now, make sure it uses the right schema file
		xml_schema::properties props;
		char path[MAXSHORT];
		GetModuleFileNameA(NULL, path, sizeof(path) / sizeof(path[0]));
		char* end = (char*)_mbsrchr((const unsigned char*)path, '\\');
		end[0] = '\0';
		props.no_namespace_schema_location(string(path) + "\\ecmcache.xsd");
		// Do the parsing
		doc.parse(fileName, 0, props);
		// Final cleanup
		cacheParser.post_cache();
		return true;
	}
	catch(const xml_schema::exception& e)
	{
		// Get the error message from the exception
		stringstream str;
		str << e;
		reason = str.str();
		return false;
	}
}

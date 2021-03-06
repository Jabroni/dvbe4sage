#pragma once

using namespace std;
using namespace stdext;

#define MAX_ERROR_MESSAGE_SIZE	2048

// Auxiliary structure for transponders
struct Transponder
{
	USHORT							onid;				// Original network ID
	USHORT							tid;				// Transponder ID
	ULONG							frequency;			// Transponder frequency
	ULONG							symbolRate;			// Transponder symbol rate
	Polarisation					polarization;		// Transponder polarization
	ModulationType					modulation;			// Transponder modulation
	BinaryConvolutionCodeRate		fec;				// Transponder FEC rate
};

// Auxiliary structure for services
struct Service
{
	USHORT							sid;				// Service SID
	USHORT							onid;				// Original network ID
	USHORT							tid;				// Transponder ID
	short							channelNumber;		// Remote channel number
	BYTE							serviceType;		// Service type
	BYTE							runningStatus;		// Running status
	hash_map<string, string>		serviceNames;		// Language to service name map

	Service(): channelNumber(-1) {}
};



class NetworkProvider
{
	// PSIParser is our friend
	friend class PSIParser;
	friend class EIT;

	// This is provide-wide data
	hash_map<UINT32, Transponder>					m_Transponders;			// ONID&TID to Transponder map
	hash_map<UINT32, Service>						m_Services;				// ONID&SID to Service descriptor map
	hash_map<USHORT, UINT32>						m_Channels;				// Channel number to ONID&SID map
	USHORT											m_DefaultONID;			// Default ONID

	// Critical section for locking and unlocking
	CCritSec										m_cs;

public:
	NetworkProvider() : m_DefaultONID(0)										{}
	// Common query methods
	bool getServiceName(UINT32 sid, LPTSTR output, int outputLength) const;
	USHORT getChannelForSid(UINT32 usid) const;
	bool isServiceExist(UINT32 usid) const;
	bool isChNoExist(USHORT channel) const;
	bool getSidForChannel(USHORT channel, UINT32& sid) const;
	UINT32 getUSIDFromChannelONID(USHORT channel, USHORT onid) const;
	bool getTransponderForSid(UINT32 sid, Transponder& transponder) const;
	bool getOnidForSid(USHORT sid, USHORT& onid) const;
	bool canBeCopied() const													{ return !m_Transponders.empty() && !m_Services.empty(); }
	const hash_map<UINT32, Transponder>& getTransponders() const				{ return m_Transponders; }
	USHORT getDefaultONID() const												{ return m_DefaultONID; }
	bool dumpChannels(LPCTSTR fileName, LPTSTR reason) const;
	bool dumpServices(LPCTSTR fileName, LPTSTR reason) const;
	bool dumpTransponders(LPCTSTR fileName, LPTSTR reason) const;
	bool logEPG(LPTSTR reason) const;
	void logEPGEntry(USHORT sid, USHORT onid, USHORT tid, string serviceName) const;

	// Clear all internal structures
	void clear();

	// Dump to xml 
	bool dumpNetworkProvider(LPTSTR reason) const;
	//bool loadXmlNetworkProvider(LPTSTR reason) const;

	// Copy all the internal data
	void copy(const NetworkProvider& other);

	// Lock and unlock
	void lock()																	{ m_cs.Lock(); }
	void unlock()																{ m_cs.Unlock(); }

	// Static function for getting unique SID manipulation
	static UINT32 getUniqueSID(USHORT onid, USHORT sid)							{ return (((UINT32)onid) << 16) + (UINT32)sid; }
	static USHORT getSIDFromUniqueSID(UINT32 usid)								{ return (USHORT)(usid & 0xFFFF); }
	static USHORT getONIDFromUniqueSID(UINT32 usid)								{ return (USHORT)(usid >> 16); }
	
};

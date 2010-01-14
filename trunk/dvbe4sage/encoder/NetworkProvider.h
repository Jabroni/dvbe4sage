#pragma once

using namespace std;
using namespace stdext;

// Auxiliary structure for transponders
struct Transponder
{
	USHORT							onid;
	USHORT							tid;
	ULONG							frequency;
	ULONG							symbolRate;
	Polarisation					polarization;
	ModulationType					modulation;
	BinaryConvolutionCodeRate		fec;
};

// Auxiliary structure for services
struct Service
{
	USHORT							TSID;				// Transport stream ID
	USHORT							ONID;				// Original network ID
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

	// This is provide-wide data
	hash_map<USHORT, Transponder>					m_Transponders;			// TID to Transponder map
	hash_map<USHORT, Service>						m_Services;				// SID to Service descriptor map
	hash_map<USHORT, USHORT>						m_Channels;				// Channel number to SID map
	USHORT											m_CurrentNid;			// Current network ID

	// Critical section for locking and unlocking
	CCritSec										m_cs;

public:
	// Common query methods
	bool getServiceName(USHORT sid, LPTSTR output, int outputLength) const;
	bool getSidForChannel(USHORT channel, USHORT& sid) const;
	bool getTransponderForSid(USHORT sid, Transponder& transponder) const;
	USHORT getNid() const														{ return m_CurrentNid; }
	bool canBeCopied() const													{ return !m_Transponders.empty() && !m_Services.empty(); }
	const hash_map<USHORT, Transponder>& getTransponders() const				{ return m_Transponders; }

	// Clear all internal structures
	void clear();

	// Copy all the internal data
	void copy(const NetworkProvider& other);

	// Lock and unlock
	void lock()																	{ m_cs.Lock(); }
	void unlock()																{ m_cs.Unlock(); }
};

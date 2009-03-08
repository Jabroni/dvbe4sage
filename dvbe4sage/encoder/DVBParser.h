#pragma once

#include "si_tables.h"
#include "decrypter.h"
#include "pluginshandler.h"

#define MAX_PSI_SECTION_LENGTH	4096

using namespace std;
using namespace stdext;

class DVBParser;

// Abstract class for TS packet parser
class TSPacketParser
{
public:
	// This is the only common function for all derived parsers
	virtual void parseTSPacket(const ts_t* const packet, USHORT pid, bool& abandonPacket) = 0;
};

// Auxiliary structure for transpoders
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

// PSI tables parser
class PSIParser : public TSPacketParser
{
private:
	// Auxiliary structure for services
	struct Service
	{
		USHORT							TSID;				// Transport stream ID
		USHORT							ONID;				// Original network ID
		USHORT							channelNumber;		// Remote channel number
		BYTE							serviceType;		// Service type
		BYTE							runningStatus;		// Running status
		hash_map<string, string>		serviceNames;		// Language to service name map
	};

	// Auxiliary structure for parsing tables
	struct SectionBuffer
	{
		USHORT							offset;							// Offset of the last write into the buffer
		USHORT							expectedLength;					// Expected length of the section
		BYTE							lastContinuityCounter;			// The value of last continuity counter
		BYTE							buffer[MAX_PSI_SECTION_LENGTH];	// The buffer itself

		SectionBuffer() : offset(0), expectedLength(0), lastContinuityCounter((BYTE)'\xFF') {}	// Constructor (making sure the offset is 0 in the beginning)
	};

	// Routines for handling varous tables parsing
	void parseTable(const pat_t* const table,  int remainingLength, bool& abandonPacket);
	void parsePATTable(const pat_t* const table, int remainingLength, bool& abandonPacket);
	void parsePMTTable(const pmt_t* const table, int remainingLength);
	void parseSDTTable(const sdt_t* const table, int remainingLength);
	void parseBATTable(const nit_t* const table, int remainingLength);
	void parseNITTable(const nit_t* const table, int remainingLength);
	void parseCATTable(const cat_t* const table, int remainingLength);
	void parseUnknownTable(const pat_t* const table, const int remainingLength) const;

	// Other important data
	hash_map<USHORT, Transponder>		m_Transponders;		// TID to Transponder map
	hash_map<USHORT, Service>			m_Services;			// SID to Service descriptor map
	hash_map<USHORT, USHORT>			m_Channels;			// Channel number to SID map
	hash_map<USHORT, USHORT>			m_PMTPids;			// SID to PMT PID map
	hash_map<USHORT, hash_set<USHORT>>	m_CATypesForSid;	// SID to CA types map
	hash_map<USHORT, hash_set<USHORT>>	m_ESPidsForSid;		// SID to ES PIDs map
	hash_map<USHORT, hash_set<USHORT>>	m_CAPidsForSid;		// SID to CA PIDs map
	hash_map<USHORT, SectionBuffer>		m_BufferForPid;		// PID to Table map
	USHORT								m_CurrentNid;		// Current network ID
	USHORT								m_CurrentTid;		// Current transponder ID
	USHORT								m_PMTCounter;		// Number of PMT packets encountered before any CAT packet
	USHORT								m_EMMPid;			// EMM PID
	DVBParser* const					m_pParent;			// Parent stream parser objects
	bool								m_AllowParsing;		// Becomes false before stopping the graph
	time_t								m_TimeStamp;		// Last update time stamp

	// Disallow default and copy constructors
	PSIParser();
	PSIParser(PSIParser&);

public:
	// Constructor
	PSIParser(DVBParser* const pParent) : 
		m_CurrentTid(0),
		m_pParent(pParent),
		m_AllowParsing(true),
		m_EMMPid(0),
		m_PMTCounter(0)
	{
		time(&m_TimeStamp);
	}

	// Query methods
	bool getPMTPidForSid(USHORT sid, USHORT& pmtPid) const;
	bool getESPidsForSid(USHORT sid, hash_set<USHORT>& esPids) const;
	bool getCAPidsForSid(USHORT sid, hash_set<USHORT>& caPids) const;
	bool getSidForChannel(USHORT channel, USHORT& sid) const;
	bool getTransponderForSid(USHORT sid, Transponder& transponder) const;
	bool getCATypesForSid(USHORT sid, hash_set<USHORT>& caTypes) const;
	time_t getTimeStamp() const		{ return m_TimeStamp; }
	USHORT getEMMPid() const		{ return m_EMMPid; }
	USHORT getNid() const			{ return m_CurrentNid; }

	// Clear method
	void clear();

	// This method disallows parsing just before stopping the graph to avoid buffers corruption
	void disallowParsing()	{ m_AllowParsing = false; }
	// This method allows parsing just before stopping the graph to avoid buffers corruption
	void allowParsing()		{ m_AllowParsing = true; }

	// Overridden parsing method
	virtual void parseTSPacket(const ts_t* const packet, USHORT pid, bool& abandonPacket);
};

class Recorder;

// ES and CA packets parser
class ESCAParser : public TSPacketParser
{
	// This is the worker thread routine
	friend DWORD WINAPI parserWorkerThreadRoutine(LPVOID param);
private:
	// Put the packet to the output buffer for worker thread (for ES packets)
	void putToOutputBuffer(const BYTE* const packet);
	// Decrypt and write pending packets, called from both main and worker threads
	void decryptAndWritePending(bool immediately);
	// Send the CA packet to the CAM
	void sendToCam(const BYTE* const currentPacket, USHORT caPid);

	// Data members
	FILE* const				m_pOutFile;							// Output file stream
	bool					m_ExitWorkerThread;					// Flag for graceful exitting of worker thread
	Recorder* const			m_pRecorder;						// The owining recorder
	
	// Decryption stuff
	BYTE					m_OddKey[8];						// CSA odd DCW
	BYTE					m_EvenKey[8];						// CSA even DCW
	bool					m_HasOddKey;						// True if has odd DCW
	bool					m_HasEvenKey;						// True if has even DCW
	Decrypter				m_Decrypter;						// Decrypter object
	PluginsHandler*	const	m_pPluginsHandler;					// Plugins handler object
	BYTE					m_LastECMPacket[PACKET_SIZE];		// Last new ECM packet

	// Output buffer stuff
	BYTE* const				m_OutputBuffer;						// The output buffer itself
	ULONG					m_PacketsInOutputBuffer;			// Current number of packets in it
	CCritSec				m_csOutputBuffer;					// Critical section on output buffer
	HANDLE					m_WorkerThread;						// The worker thread handling decryption
	HANDLE					m_SignallingEvent;					// Event for signalling we have new work for the worker thread
	bool					m_DeferWriting;						// True when there is new ECM packet but no keys have been received yet
	bool					m_FoundPATPacket;					// Becomes true when the first PAT packet is found
	bool					m_IsEncrypted;						// True if the ES are encrypted
	__int64					m_FileLength;						// Contains the output file length so far
	const __int64			m_MaxFileLength;					// Max file length for cyclic buffer, for regular file -1
	__int64					m_CurrentPosition;					// Current position in the cyclic buffer, otherwise unused

	// Assigned PID differentiator
	hash_map<USHORT, bool>	m_IsESPid;							// PID to bool map, true for ES, false for CA
	const USHORT			m_Sid;								// SID of the program being recorded
	const USHORT			m_PmtPid;							// PID of PMT of the program being recorded
	const hash_set<USHORT>	m_CATypes;							// CA type of the program being recorded
	const USHORT			m_EMMPid;							// EMM PID

	// Different dilution stuff
	USHORT					m_PATCounter;						// Running PAT packet counter
	USHORT					m_PATContinuityCounter;				// Current PAT continuity counter
	USHORT					m_PMTCounter;						// Running PMT packet counter
	USHORT					m_PMTContinuityCounter;				// Current PMT continuity counter

	// Internal write method to handle cyclic buffers
	int write(int bytesToWrite);

	// Check audio language in ES descriptor
	static bool matchAudioLanguage(const BYTE* const buffer, const int bufferLength, LPCTSTR language);

	// Default and copy constructors are disallowed
	ESCAParser();
	ESCAParser(ESCAParser&);

public:
	// The only valid constructor
	ESCAParser(Recorder* const pRecorder, FILE* const pFile, PluginsHandler* const pPluginsHandler, USHORT sid, USHORT pmtPid, const hash_set<USHORT>& caTypes, USHORT emmPid, __int64 maxFileLength);

	// Destructor
	virtual ~ESCAParser();

	// Overridden parsing method
	virtual void parseTSPacket(const ts_t* const packet, USHORT pid, bool& abandonPacket);

	// This routine is called by the plugins handler
	void setKey(bool isOddKey, const BYTE* const key);

	// Tell PID meaning
	void setESPid(USHORT pid, bool isESPid);

	// Reset the parser
	void reset();

	// Get the file length
	__int64 getFileLength() const	{ return m_FileLength; }
};

// TS stream parser
class DVBParser
{
private:
	// These are to handle augmented TS packets
	long	m_LeftoverLength;
	BYTE	m_LeftoverBuffer[TS_PACKET_LEN];

	// Stream PID to Parser map
	hash_map<USHORT, hash_set<TSPacketParser*>>	m_ParserForPid;

	// We have only one PSI parser
	PSIParser m_PSIParser;

	// Critical section for locking and unlocking
	CCritSec	m_cs;

	// Flag saying no clients connected yet
	bool		m_HasConnectedClients;

	// Disallow copy constructor
	DVBParser(DVBParser&);

public:
	// Constructor and destructor
	DVBParser() : m_PSIParser(this), m_HasConnectedClients(false), m_LeftoverLength(0) {}

	// Reset the parser map
	void resetParser(bool clearPSIParser);

	// Assign parser to a specific PID
	void assignParserToPid(USHORT pid, TSPacketParser* parser);

	// Stop listening on a specific parser
	void dropParser(TSPacketParser* parser);

	// Generic function for parsing a bunch of TS packets
	void parseTSStream(const BYTE* inputBuffer, int inputBufferLength);	

	// Query methods - delegated to the internal PSI parser
	bool getPMTPidForSid(USHORT sid, USHORT& pmtPid) const					{ return m_PSIParser.getPMTPidForSid(sid, pmtPid); }
	bool getESPidsForSid(USHORT sid, hash_set<USHORT>& esPids) const		{ return m_PSIParser.getESPidsForSid(sid, esPids); }
	bool getCAPidsForSid(USHORT sid, hash_set<USHORT>& caPids) const		{ return m_PSIParser.getCAPidsForSid(sid, caPids); }
	bool getSidForChannel(USHORT channel, USHORT& sid) const				{ return m_PSIParser.getSidForChannel(channel, sid); }
	bool getTransponderForSid(USHORT sid, Transponder& transponder) const	{ return m_PSIParser.getTransponderForSid(sid, transponder); }
	bool getCATypesForSid(USHORT sid, hash_set<USHORT>& caTypes) const		{ return m_PSIParser.getCATypesForSid(sid, caTypes); }
	time_t getTimeStamp() const												{ return m_PSIParser.getTimeStamp(); }
	USHORT getEMMPid() const												{ return m_PSIParser.getEMMPid(); }

	// Lock and unlock
	void lock();
	void unlock();

	// This method stop PSI packets parsing just before stopping the graph to avoid corruption
	void stopPSIPackets()											{ m_PSIParser.disallowParsing(); }
	// This method resumes PSI packets parsing just before stopping the graph to avoid corruption
	void resumePSIPackets()											{ m_PSIParser.allowParsing(); }

	// Get HasConnectedClients flag
	bool getHasConnectedClients() const								{ return m_HasConnectedClients; }

	// Set m_HasConnectedClients flag
	void setHasConnectedClients()									{ m_HasConnectedClients = true; }
};

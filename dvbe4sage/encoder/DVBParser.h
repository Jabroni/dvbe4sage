#pragma once

#include "si_tables.h"
#include "decrypter.h"
#include "pluginshandler.h"
#include "configuration.h"
#include "NetworkProvider.h"

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

// Elementary stream types
enum ESType { ES_TYPE_VIDEO, ES_TYPE_AUDIO, ES_TYPE_DVB_SUBTITLE, ES_TYPE_TELETEXT_SUBTITLE, ES_TYPE_OTHER };

// Elementary Stream Information
struct ESInfo
{
	ESType							type;
	string							language;
	BYTE							streamType;
	list<BYTE[256]>					descriptors;
};

// PSI tables parser
class PSIParser : public TSPacketParser
{
private:
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
	void parseTable(const pat_t* const table,  short remainingLength, bool& abandonPacket);
	void parsePATTable(const pat_t* const table, short remainingLength, bool& abandonPacket);
	void parsePMTTable(const pmt_t* const table, short remainingLength);
	void parseSDTTable(const sdt_t* const table, short remainingLength);
	void parseBATTable(const nit_t* const table, short remainingLength);
	void parseNITTable(const nit_t* const table, short remainingLength);
	void parseCATTable(const cat_t* const table, short remainingLength);
	void parseUnknownTable(const pat_t* const table, const short remainingLength) const;

	// This is provide-wide data
	NetworkProvider									m_Provider;						// All provider-wide data goes here

	// This is transponder-wide data
	hash_map<USHORT, USHORT>						m_PMTPids;						// SID to PMT PID map
	hash_map<USHORT, hash_set<CAScheme>>			m_CATypesForSid;				// SID to CA types map
	hash_map<USHORT, hash_set<USHORT>>				m_ESPidsForSid;					// SID to ES PIDs map
	hash_map<USHORT, hash_set<USHORT>>				m_CAPidsForSid;					// SID to CA PIDs map
	hash_map<USHORT, SectionBuffer>					m_BufferForPid;					// PID to Table map
	USHORT											m_CurrentTid;					// Current transponder ID
	EMMInfo											m_EMMPids;						// EMM PID to EMM CA types map
	
	// Other important data
	USHORT											m_PMTCounter;					// Number of PMT packets encountered before any CAT packet
	DVBParser* const								m_pParent;						// Parent stream parser objects
	bool											m_AllowParsing;					// Becomes false before stopping the graph
	time_t											m_TimeStamp;					// Last update time stamp
	bool											m_ProviderInfoHasBeenCopied;	// True if the tables have been copied to the encoder

	// Disallow default and copy constructors
	PSIParser();
	PSIParser(const PSIParser&);

public:
	// Constructor
	PSIParser(DVBParser* const pParent) : 
		m_CurrentTid(0),
		m_pParent(pParent),
		m_AllowParsing(true),
		m_PMTCounter(0),
		m_TimeStamp(0),
		m_ProviderInfoHasBeenCopied(false)
	{
	}

	// Query methods
	const NetworkProvider& getNetworkProvider() const							{ return m_Provider; }
	bool getPMTPidForSid(USHORT sid, USHORT& pmtPid) const;
	bool getESPidsForSid(USHORT sid, hash_set<USHORT>& esPids) const;
	bool getCAPidsForSid(USHORT sid, hash_set<USHORT>& caPids) const;
	bool getECMCATypesForSid(USHORT sid, hash_set<CAScheme>& ecmCATypes) const;
	void getEMMCATypes(EMMInfo& emmCATypes) const								{ emmCATypes = m_EMMPids; }
	time_t getTimeStamp() const													{ return m_TimeStamp; }
	bool providerInfoHasBeenCopied() const										{ return m_ProviderInfoHasBeenCopied; }
	USHORT getCurrentTid() const												{ return m_CurrentTid; }

	// Delegated to the network provider
	bool getSidForChannel(USHORT channel, USHORT& sid) const					{ return m_Provider.getSidForChannel(channel, sid); }
	bool getTransponderForSid(USHORT sid, Transponder& transponder) const		{ return m_Provider.getTransponderForSid(sid, transponder); }
	bool getServiceName(USHORT sid, LPTSTR output, int outputLength) const		{ return m_Provider.getServiceName(sid, output, outputLength); }
	USHORT getNid() const														{ return m_Provider.getNid(); }
	bool canBeCopied() const													{ return m_Provider.canBeCopied(); }

	// Setter for "Has been copied" flag
	void setProviderInfoHasBeenCopied()											{ m_ProviderInfoHasBeenCopied = true; }

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
	// This structure defines an output buffer to be decrypted together
	struct OutputBuffer
	{
		BYTE* const	buffer;										// The output buffer itself
		ULONG		numberOfPackets;							// Current number of packets in it
		BYTE		oddKey[8];									// CSA odd DCW
		BYTE		evenKey[8];									// CSA even DCW
		bool		hasKey;										// True if has one of the keys

		// Default constructor
		OutputBuffer() : 
			buffer(new BYTE[TS_PACKET_LEN * g_pConfiguration->getTSPacketsPerOutputBuffer()]),
			numberOfPackets(0),
			hasKey(false)
		{
			// Set the initial values for keys
			oddKey[0] = evenKey[0] = 0x14;
			oddKey[1] = evenKey[1] = 0x89;
			oddKey[2] = evenKey[2] = 0x5E;
			oddKey[3] = evenKey[3] = 0xFB;
			oddKey[4] = evenKey[4] = 0x61;
			oddKey[5] = evenKey[5] = 0xB5;
			oddKey[6] = evenKey[6] = 0x31;
			oddKey[7] = evenKey[7] = 0x47;
		}

		// Copy constructor
		OutputBuffer(const OutputBuffer& other) : 
			buffer(new BYTE[TS_PACKET_LEN * g_pConfiguration->getTSPacketsPerOutputBuffer()])
		{
			numberOfPackets = other.numberOfPackets;
			hasKey = other.hasKey;
			memcpy(buffer, other.buffer, TS_PACKET_LEN * g_pConfiguration->getTSPacketsPerOutputBuffer());
			memcpy(oddKey, other.oddKey, sizeof(oddKey));
			memcpy(evenKey, other.evenKey, sizeof(evenKey));
		}

		virtual ~OutputBuffer()
		{
			delete [] buffer;
		}
	};

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
	FILE* const						m_pOutFile;							// Output file stream
	bool							m_ExitWorkerThread;					// Flag for graceful exitting of worker thread
	Recorder* const					m_pRecorder;						// The owining recorder
	LPCTSTR							m_ChannelName;						// The channel name
	
	// Decryption stuff
	Decrypter						m_Decrypter;						// Decrypter object
	PluginsHandler*	const			m_pPluginsHandler;					// Plugins handler object
	BYTE							m_LastECMPacket[PACKET_SIZE];		// Last new ECM packet
	bool							m_NoECMPacketsYet;					// True as long as no ECM packets have been received
	bool							m_NoDCWYet;							// True as long as no DCW has been received

	// Output buffer stuff
	deque<OutputBuffer* const>		m_OutputBuffers;					// Vector of output buffers (one per distinct ECM packet)
	CCritSec						m_csOutputBuffer;					// Critical section on output buffers structure
	HANDLE							m_WorkerThread;						// The worker thread handling decryption
	HANDLE							m_SignallingEvent;					// Event for signalling we have new work for the worker thread
	const bool						m_IsEncrypted;						// True if the ES are encrypted
	__int64							m_FileLength;						// Contains the output file length so far
	const __int64					m_MaxFileLength;					// Max file length for cyclic buffer, for regular file -1
	__int64							m_CurrentPosition;					// Current position in the cyclic buffer, otherwise unused
	USHORT							m_ResetCounter;						// How many subsequent resets have been encountered?

	// Assigned PID differentiator
	hash_map<USHORT, bool>			m_IsESPid;							// PID to bool map, true for ES, false for CA
	hash_map<USHORT, bool>			m_ValidPacketFound;					// PIT to bool map, true when a first valid packet was found for an ES PID
	const USHORT					m_Sid;								// SID of the program being recorded
	const USHORT					m_PmtPid;							// PID of PMT of the program being recorded
	const hash_set<CAScheme>		m_ECMCATypes;						// ECM CA types of the program being recorded
	const EMMInfo					m_EMMCATypes;						// EMM PIDs to EMM CA types map

	// Different dilution stuff
	USHORT							m_PATCounter;						// Running PAT packet counter
	USHORT							m_PATContinuityCounter;				// Current PAT continuity counter
	USHORT							m_PMTCounter;						// Running PMT packet counter
	USHORT							m_PMTContinuityCounter;				// Current PMT continuity counter

	// Internal write method to handle cyclic buffers
	int write(const BYTE* const buffer, int bytesToWrite);

	// Check audio language in ES descriptor
	static bool matchAudioLanguage(const BYTE* const buffer, const int bufferLength, const char* language);

	// Check if a key can decrypt the current buffer
	bool isCorrectKey(const OutputBuffer* const currentBuffer, const bool isOddKey, const BYTE* const key);

	// Default and copy constructors are disallowed
	ESCAParser();
	ESCAParser(const ESCAParser&);

public:
	// The only valid constructor
	ESCAParser(Recorder* const pRecorder,
			   FILE* const pFile,
			   PluginsHandler* const pPluginsHandler,
			   LPCTSTR channelName,
			   USHORT sid,
			   USHORT pmtPid,
			   const hash_set<CAScheme>& ecmCATypes,
			   const EMMInfo& emmCATypes,
			   __int64 maxFileLength);

	// Destructor
	virtual ~ESCAParser();

	// Overridden parsing method
	virtual void parseTSPacket(const ts_t* const packet, USHORT pid, bool& abandonPacket);

	// This routine is called by the plugins handler
	bool setKey(bool isOddKey, const BYTE* const key);

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

	// Tuner ordinal number
	const UINT	m_TunerOrdinal;

	// Dump file (created only on request)
	FILE* m_FullTransponderFile;

	// Disallow default and copy constructor
	DVBParser();
	DVBParser(const DVBParser&);

public:
	// The only available constructor
	DVBParser(UINT ordinal) :	
	  m_PSIParser(this),	m_HasConnectedClients(false), m_LeftoverLength(0), m_TunerOrdinal(ordinal),	m_FullTransponderFile(NULL) {}
	
	// Destructor
	virtual ~DVBParser()															{ stopTransponderDump(); }

	// Tuner ordinal getter
	UINT getTunerOrdinal() const													{ return m_TunerOrdinal; }

	// Reset the parser map
	void resetParser(bool clearPSIParser);

	// Assign parser to a specific PID
	void assignParserToPid(USHORT pid, TSPacketParser* parser);

	// Stop listening on a specific parser
	void dropParser(TSPacketParser* parser);

	// Generic function for parsing a bunch of TS packets
	void parseTSStream(const BYTE* inputBuffer, int inputBufferLength);	

	// Query methods - delegated to the internal PSI parser
	const NetworkProvider& getNetworkProvider() const								{ return m_PSIParser.getNetworkProvider(); }
	bool getPMTPidForSid(USHORT sid, USHORT& pmtPid) const							{ return m_PSIParser.getPMTPidForSid(sid, pmtPid); }
	bool getESPidsForSid(USHORT sid, hash_set<USHORT>& esPids) const				{ return m_PSIParser.getESPidsForSid(sid, esPids); }
	bool getCAPidsForSid(USHORT sid, hash_set<USHORT>& caPids) const				{ return m_PSIParser.getCAPidsForSid(sid, caPids); }
	bool getSidForChannel(USHORT channel, USHORT& sid) const						{ return m_PSIParser.getSidForChannel(channel, sid); }
	bool getTransponderForSid(USHORT sid, Transponder& transponder) const			{ return m_PSIParser.getTransponderForSid(sid, transponder); }
	bool getECMCATypesForSid(USHORT sid, hash_set<CAScheme>& ecmCATypes) const		{ return m_PSIParser.getECMCATypesForSid(sid, ecmCATypes); }
	void getEMMCATypes(EMMInfo& emmCATypes) const									{ m_PSIParser.getEMMCATypes(emmCATypes); }
	time_t getTimeStamp() const														{ return m_PSIParser.getTimeStamp(); }
	bool getServiceName(USHORT sid, LPTSTR output, int outputLength) const			{ return m_PSIParser.getServiceName(sid, output, outputLength); }
	bool providerInfoHasBeenCopied() const											{ return m_PSIParser.providerInfoHasBeenCopied(); }
	USHORT getCurrentTid() const													{ return m_PSIParser.getCurrentTid(); }


	// Setter for the internal parser "HasBeenCopied" flag
	void setProviderInfoHasBeenCopied()												{ m_PSIParser.setProviderInfoHasBeenCopied(); }

	// Lock and unlock
	void lock();
	void unlock();

	// This method stop PSI packets parsing just before stopping the graph to avoid corruption
	void stopPSIPackets()															{ m_PSIParser.disallowParsing(); }
	// This method resumes PSI packets parsing just before stopping the graph to avoid corruption
	void resumePSIPackets()															{ m_PSIParser.allowParsing(); }

	// Get HasConnectedClients flag
	bool getHasConnectedClients() const												{ return m_HasConnectedClients; }

	// Set m_HasConnectedClients flag
	void setHasConnectedClients()													{ m_HasConnectedClients = true; }

	// Start dumping the full transponder
	void startTransponderDump();

	// Stop dumping the full transponder
	void stopTransponderDump();
};

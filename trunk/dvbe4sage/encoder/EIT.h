#pragma once

using namespace std;
using namespace stdext;

#include "si_tables.h"
class DVBParser;

//This is the language code for english. We need this for DISH Network EPG because it doesn't contain one
#define langENG	6647399

// Need this for time calculations
#define bcdtoint(i) ((((i & 0xF0) >> 4) * 10) + (i & 0x0F))

#define MAX_DATE_LENGTH						256
#define MAX_EXPANDED_DESCRIPTION			(256 * 6 * 2 + 1)

// Records of the ini file
struct eitRecord
{
	int ONID;
	string lineup;
	string tuner;
	ULONG chan;
//	bool useLongChennelName;
	hash_set<USHORT> includedSIDs;
};

typedef  struct stEPGLanguage
{
	DWORD language;
	string eventText;
	string text;
	string shortDescription;
	string longDescription;
	unsigned int parentalRating;
	bool CR_added;
} EPGLanguage;

// Auxiliary structure for EIT events
struct EITEvent
{
	USHORT eventID;
	int SID;
	int ONID;
	time_t startTime;
	time_t stopTime;
	string shortDescription;
	string longDescription;
	string startDateTime;			// Event start date and time (local time)
	string stopDateTime;			// Event stop date and time (local time)
	int    parentalRating;			// Note: Not the North American rating
	string starRating;
	string mpaaRating;				// North American rating
	string vchipRating;				// Alternate North American rating and advisory
	int vchipAdvisory;
	int mpaaAdvisory;
	string category;
	string dishCategory1;
	string dishCategory2;
	string programID;
	string seriesID;
	int	   aspect;
	int    year;					// Year first aired or 0 for new
	bool   stereo;
	bool   CC;
	bool   teletext;
	bool   onscreen;
	vector<EPGLanguage> vecLanguages;
	typedef vector<EPGLanguage>::iterator ivecLanguages;
};

class EIT
{
public:
	EIT(DVBParser* const pParent);
	~EIT(void);

	// Collect EIT data or not
	bool collectEIT() const									{ return m_CollectEIT; }

	// Stop collecting EIT data
	void stopCollectingEIT();

	// Parse the EIT table
	void parseEITTable(const eit_t* const table, int remainingLength);

	void RealEitCollectionCallback();
	void StartEitMonitoring();

private:
	time_t						m_Time;
	HANDLE						m_EitCollectionThread;
	DWORD						m_EitCollectionThreadId;
	bool						m_EitCollectionThreadCanEnd;

	DVBParser* m_pDVBParser;

	struct eitRecord GetEitRecord(int onid);
	EPGLanguage GetDescriptionRecord(struct EITEvent rec);


	void ParseIniFile(void);
	vector<eitRecord> m_eitRecords;
	string m_SageEitIP;
	USHORT m_SageEitPort;
	string m_SaveXmltvFileLocation;
	string m_TempFileLocation;
	int m_CollectionTimeHour;
	int m_CollectionTimeMin;
	int m_CollectionDurationMinutes;
	bool m_CollectEIT;
	hash_map<UINT32, EITEvent>	m_EITevents;			// All events for the service. The key is sid and event ID, the value is an EITEvent structure
	bool						m_TimerInitialized;

	// Dump EIT data to a xmltv file
	void dumpXmltvFile(int onid);
	void dumpXmltvAdvisory(FILE* outFile, string value);
	string getAspect(int aspect);
	string getQuality(int aspect);
	string getParentalRating(int rating);

	// Send data to SageTV
	void sendToSage(int onid);

	void lock();
	void unlock();

	// Static function for m_EITevents key handling
	static UINT32 getUniqueKey(USHORT eventID, USHORT sid)			{ return (((UINT32)eventID) << 16) + (UINT32)sid; }
	static USHORT getSIDFromUniqueKey(UINT32 key)					{ return (USHORT)(key & 0xFFFF); }
	static USHORT getEventIdFromUniqueKey(UINT32 key)				{ return (USHORT)(key >> 16); }

	// Critical section for locking and unlocking
	CCritSec	m_cs;

	bool SendSocketCommand(char *command);

	void parseEventDescriptors(const BYTE* inputBuffer, int remainingLength, const eit_t* const table, EITEvent* newEvent);
	void decodeShortEventDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeExtendedEvent(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeContentDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeContentDescriptorDish(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeComponentDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	//void decodeProviderIdDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeParentalRatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeCombinedStarRating_MPAARatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeDishShortDescription(const BYTE* inputBuffer, int tnum, EITEvent* newEvent);
	void decodeDishLongDescription(const BYTE* inputBuffer, int tnum, EITEvent* newEvent);	
	void decodeDishCcStereoDescriptor(const BYTE* inputBuffer, int tnum, EITEvent* newEvent);
	void decodeDishVChipRatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeDishSeriesDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	std::string ReplaceAll(std::string result, const std::string& replaceWhat, const std::string& replaceWithWhat);
};

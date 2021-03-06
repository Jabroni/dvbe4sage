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

typedef enum
{
	EIT_PROVIDER_STANDARD = 1,
	EIT_PROVIDER_DISH = 2,
	EIT_PROVIDER_SKY = 3
} EIT_PROVIDER;

typedef struct stRanges {
	int from;
	int to;
	int offset;
} Ranges;


// Records of the ini file
struct eitRecord
{
	int ONID;
	ULONG chan;
	EIT_PROVIDER eitProvider;
	vector<Ranges> includedSIDs;
	vector<Ranges> excludedSIDs;
	int useBouquets;
	vector<string> filterText;
	vector<Ranges> logicalChannelOffset;
	int useUSIDasSID;
	int disabled;
};


typedef struct stEPGLanguage
{
	DWORD language;
	string eventText;
	string text;
	string shortDescription;
	string longDescription;
	unsigned int parentalRating;
	bool CR_added;
} EPGLanguage;

typedef struct stEPGSubtitle
{
	string language;	
	bool   teletext;
	bool   onscreen;
} EPGSubtitle;

// Auxiliary structure for EIT eventsd
struct EITEvent
{
	USHORT eventID;
	UINT32 SID;
	UINT32 ONID;
	time_t startTime;
	time_t stopTime;
	string shortDescription;
	string longDescription;
	string startDateTime;			// Event start date and time (local time)
	string startDateTimeSage;			// Event start date and time (local time)
	string stopDateTime;			// Event stop date and time (local time)
	string durationTime;			// Event duration time
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
	bool   HD;
	bool	isNew;
	vector<EPGSubtitle> vecSubtitles;
	typedef vector<EPGSubtitle>::iterator ivecSubtitles;
	vector<EPGLanguage> vecLanguages;
	typedef vector<EPGLanguage>::iterator ivecLanguages;
};


class EITtimer
{
public:
	EITtimer();
	~EITtimer(void);

	// Stop collecting EIT data
	void stopCollectingEIT();

	void RealEitCollectionCallback();
	void StartEitMonitoring();

private:
	time_t	m_Time;
	HANDLE	m_EitTimerThread;
	DWORD	m_EitTimerThreadId;
	bool	m_EitTimerThreadCanEnd;
	string	m_TempFileLocation;

	void ParseIniFile(void);
	int m_CollectionTimeHour;
	int m_CollectionTimeMin;
	int m_CollectionDurationMinutes;
	string m_SageEitLocalPath;
	string m_SageEitRemotePath;
	string	m_SageEitLineup;
	string m_SageEitIP;
	int m_debugEpg;

	vector<eitRecord> m_eitRecords;

	bool SendSocketCommand(char *command);
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
	int							m_onid;
	EIT_PROVIDER				m_provider;
	time_t						m_Time;
	HANDLE						m_EitCollectionThread;
	DWORD						m_EitCollectionThreadId;
	bool						m_EitCollectionThreadCanEnd;

	DVBParser* m_pDVBParser;

	FILE *m_eitEventFile;
	bool OpenEitEventFile(bool read);
	void CloseEitEventFile();
	void DeleteEitEventFile();
	bool WriteEitEventRecord(struct EITEvent *rec);
	bool ReadNextEitEventRecord(struct EITEvent *rec);
	void ResetEventVars(struct EITEvent *rec);
	ULONG m_eventsCount;
	hash_set<__int64>	m_eitEventIDs;

	struct eitRecord GetEitRecord(int onid);
	EPGLanguage GetDescriptionRecord(struct EITEvent rec);


	void ParseIniFile(void);
	vector<eitRecord> m_eitRecords;
	string m_SageEitIP;
	USHORT m_SageEitPort;
	string m_SaveXmltvFileLocation;
	string m_TempFileLocation;
	string m_SageEitRemotePath;
	string	m_SageEitLineup;
	eitRecord m_eitRec;
	
	int m_CollectionDurationMinutes;
	bool m_CollectEIT;
//	hash_map<UINT32, EITEvent>	m_EITevents;			// All events for the service. The key is sid and event ID, the value is an EITEvent structure
	bool						m_TimerInitialized;

	// Dump EIT data to a xmltv file
	void dumpXmltvFile(int onid);
	void dumpXmltvAdvisory(FILE* outFile, string value);
	string getAspect(int aspect);
	string getQuality(int aspect);
	string getParentalRating(int rating);

	// Send data to SageTV
	void sendToSage(int onid, bool savefile);
	int getSageLogicalChannel( USHORT onid , USHORT sid, USHORT fromONID, USHORT chno);
	int getONIDfromSID(string epgname);
	bool IsSIDAllowed(USHORT sid);

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
	void decodeSkyDescriptor(const BYTE* inputBuffer, EITEvent* newEvent, int length);
	void decodeSkyDescriptor2(const BYTE* inputBuffer, EITEvent* newEvent, int length);
	void decodeShortEventDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeExtendedEvent(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeContentDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeContentDescriptorDish(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeContentDescriptorSky(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeComponentDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	//void decodeProviderIdDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeParentalRatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeCombinedStarRating_MPAARatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeDishShortDescription(const BYTE* inputBuffer, int tnum, EITEvent* newEvent);
	void decodeDishLongDescription(const BYTE* inputBuffer, int tnum, EITEvent* newEvent);	
	void decodeDishCcStereoDescriptor(const BYTE* inputBuffer, int tnum, EITEvent* newEvent);
	void decodeDishVChipRatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void decodeDishSeriesDescriptor(const BYTE* inputBuffer, EITEvent* newEvent);
	void determineProgramCapabilities(string descriptionText, EITEvent* newEvent);
	std::string ReplaceAll(std::string result, const std::string& replaceWhat, const std::string& replaceWithWhat);
	std::string ReplaceLatinChars(std::string text);
	
};

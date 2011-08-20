#include "StdAfx.h"
#include "EIT.h"
#include "IniFile.h"
#include "Logger.h"
#include "graph.h"
#include "extern.h"
#include "DishNetworkHelper.h"
#include "AutoString.h"
#include "Decompress.h"
#include "DVBParser.h"
#include "encoder.h"

#define	CRC_LENGTH			4
#define TABLE_PREFIX_LEN	3

extern Encoder*	g_pEncoder;
int g_onid = 0;

EITtimer::EITtimer() :
m_EitTimerThreadCanEnd(false)
{
	ParseIniFile();

	StartEitMonitoring();
}

EITtimer::~EITtimer(void)
{
	stopCollectingEIT();
	Sleep(6000);
}

void EITtimer::ParseIniFile(void)
{
	string buffer;
	LPTSTR context;
	TCHAR tbuffer[1024];
	vector<string> sections;

	TCHAR iniFileFullPath[MAXSHORT];
	DWORD iniFileFullPathLen = GetCurrentDirectory(sizeof(iniFileFullPath) / sizeof(iniFileFullPath[0]), iniFileFullPath);
	_tcscpy_s(&iniFileFullPath[iniFileFullPathLen], sizeof(iniFileFullPath) / sizeof(iniFileFullPath[0]) - iniFileFullPathLen, "\\eit.ini");
	string fileName = iniFileFullPath;

	sections = CIniFile::GetSectionNames(fileName);

	// Log info
	log(0, false, 0, TEXT("\n"));
	log(0, true, 0, TEXT("Loading EIT Timer info from \"%s\"\n"), iniFileFullPath);
	log(0, false, 0, TEXT("=========================================================\n"));
	log(0, false, 0, TEXT("EIT Timer file dump:\n\n"));

	for(int i = 0; i < (int)sections.size(); i++)
	{
		if(sections[i].compare("General") == 0)
		{
			log(0, false, 0, TEXT("[General]\n"));

			m_CollectionTimeHour = atoi(CIniFile::GetValue("CollectionTimeHour", sections[i], fileName).c_str());
			log(0, false, 0, TEXT("CollectionTimeHour = %d\n"), m_CollectionTimeHour);
						
			m_CollectionTimeMin = atoi(CIniFile::GetValue("CollectionTimeMin", sections[i], fileName).c_str());
			log(0, false, 0, TEXT("CollectionTimeMin = %d\n"), m_CollectionTimeMin);

			m_TempFileLocation = CIniFile::GetValue("TempFileLocation", sections[i], fileName);
			log(0, false, 0, TEXT("TempFileLocation = %s\n"), m_TempFileLocation.c_str());

			m_CollectionDurationMinutes = atoi(CIniFile::GetValue("CollectionDurationMinutes", sections[i], fileName).c_str());
			log(0, false, 0, TEXT("CollectionDurationMinutes = %d\n"), m_CollectionDurationMinutes);

			log(0, false, 0, TEXT("\n"));
		}
		else
		{
			struct eitRecord newrec;
			newrec.ONID = atoi(sections[i].c_str());
			newrec.lineup = CIniFile::GetValue("Lineup", sections[i], fileName);	
			newrec.chan = atol(CIniFile::GetValue("ChanOrSID", sections[i], fileName).c_str());

			newrec.includedSIDs.clear();
			buffer = CIniFile::GetValue("IncludedSIDs", sections[i], fileName);
#pragma warning(disable:4996)
			size_t length = buffer.copy(tbuffer, buffer.length(), 0);
#pragma warning(default:4996)
			buffer[length] = '\0';
			if(buffer[0] != TCHAR(0))
			{
				for(LPCTSTR token = _tcstok_s(tbuffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
					newrec.includedSIDs.insert((USHORT)_ttoi(token));
			}

			log(0, false, 0, TEXT("[%d]\n"), newrec.ONID);
			log(0, false, 0, TEXT("Lineup = %s\n"), newrec.lineup.c_str());
			log(0, false, 0, TEXT("ChanOrSID = %lu\n"), newrec.chan);

			log(0, false, 0, TEXT("IncludedSIDs="));
			for(hash_set<USHORT>::const_iterator it = newrec.includedSIDs.begin(); it != newrec.includedSIDs.end();)
			{
				log(0, false, 0, TEXT("%d"), *it);
				if(++it != newrec.includedSIDs.end())
					log(0, false, 0, TEXT(","));
			}
			log(0, false, 0, TEXT("\n"));
			log(0, false, 0, TEXT("\n"));

			m_eitRecords.push_back(newrec);
		}
	}

	log(0, false, 0, TEXT("\n"));

	log(0, false, 0, TEXT("End of EIT Timer file dump\n"));
	log(0, false, 0, TEXT("=========================================================\n\n"));
}

DWORD WINAPI EitTimerCallback(LPVOID vpEit)
{
	EITtimer* eitObj = (EITtimer*)vpEit;

	// Jump back up into the object
	eitObj->RealEitCollectionCallback();

	return 0;
}

void EITtimer::StartEitMonitoring()
{
	// Start the timer thread
	m_EitTimerThreadCanEnd = false;
	m_EitTimerThread = CreateThread(NULL, 0, EitTimerCallback, this, 0, &m_EitTimerThreadId);
}

void EITtimer::RealEitCollectionCallback()
{
	struct tm lTime;
	time_t now;
	char command[MAX_PATH];
	char tempFile[MAX_PATH];

	sprintf_s(tempFile, sizeof(tempFile), "%s\\TempEitGathering.ts",  m_TempFileLocation.c_str());

	log(3, true, 0, TEXT("Waiting until %02d:%02d to collect and process EIT data.\n"), m_CollectionTimeHour, m_CollectionTimeMin);
	
	while(1 == 1)
	{
		// Wait until the start time or told to quit
		while (1 == 1)
		{
			Sleep(5000);
			if(m_EitTimerThreadCanEnd == true)
				return;
			
			time(&now);
			localtime_s(&lTime, &now);
			if(lTime.tm_hour == m_CollectionTimeHour && lTime.tm_min == m_CollectionTimeMin)
				break;
		}

		// Loop for all configured providers
		for (vector<eitRecord>::iterator eIter = m_eitRecords.begin(); eIter != m_eitRecords.end(); eIter++)
		{
			g_onid = eIter->ONID;

			// "START SageTV DVB-S2 Enhancer 1 Digital TV Tuner|1576479146|268969251|2599483936192|D:\tivo\DontForgettheLyrics-8312556-0.ts|Fair"
			// Send socket command make to us tune and lock the tuner
			sprintf_s(command, sizeof(command), "START ***EITGATHERING***|0|%d|%d|%s|Fair\r\n", eIter->chan, m_CollectionDurationMinutes * 60 * 1000, tempFile);
			SendSocketCommand(command);

			time(&now);
			time(&m_Time);

			// Allow for collection of the data
			while(difftime(now, m_Time) < ((m_CollectionDurationMinutes + 5) * 60))
			{
				Sleep(10000);
				time(&now);
			}

			Sleep(10000);
		}

		// Wait for 2 minutes then start over
		for(int i = 0; i < 24; i++)
		{
			Sleep(5000);
			if(m_EitTimerThreadCanEnd == true)
				return;			
		}
	}
}

bool EITtimer::SendSocketCommand(char *command)
{
	int port = g_pConfiguration->getListeningPort();

	SOCKET s; 
    SOCKADDR_IN target; 
    target.sin_family = AF_INET; 
    target.sin_port = htons((USHORT)port); 
    target.sin_addr.s_addr = inet_addr ("127.0.0.1"); 

	log(3, true, 0, TEXT("EIT Timer is attempting to send the following command to DVBE4SAGE:\n"));
	log(3, false, 0, TEXT("%s\n"), command);

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
        return false; 

    if (connect(s, (SOCKADDR *)&target, sizeof(target)) == SOCKET_ERROR)
        return false; 
	
	if(send(s, command, strlen(command), 0) == SOCKET_ERROR)
	{
		shutdown(s, SD_SEND);
		closesocket(s);
        return false; 
	}

	shutdown(s, SD_SEND);
	closesocket(s);

	return true;
}

// Stop collecting EIT data
void EITtimer::stopCollectingEIT()
{
	m_EitTimerThreadCanEnd = true;
}

EIT::EIT(DVBParser* const pParent) :
m_CollectEIT(false)
, m_TimerInitialized(false)
, m_EitCollectionThreadCanEnd(false)
, m_onid(g_onid)
{
	m_pDVBParser = pParent;

	ParseIniFile();

	StartEitMonitoring();
}

EIT::~EIT(void)
{
	stopCollectingEIT();
	Sleep(6000);
}

void EIT::ParseIniFile(void)
{
	string buffer;
	LPTSTR context;
	TCHAR tbuffer[1024];
	vector<string> sections;

	TCHAR iniFileFullPath[MAXSHORT];
	DWORD iniFileFullPathLen = GetCurrentDirectory(sizeof(iniFileFullPath) / sizeof(iniFileFullPath[0]), iniFileFullPath);
	_tcscpy_s(&iniFileFullPath[iniFileFullPathLen], sizeof(iniFileFullPath) / sizeof(iniFileFullPath[0]) - iniFileFullPathLen, "\\eit.ini");
	string fileName = iniFileFullPath;

	sections = CIniFile::GetSectionNames(fileName);

	// Log info
	log(0, false, 0, TEXT("\n"));
	log(0, true, 0, TEXT("Loading EIT info from \"%s\"\n"), iniFileFullPath);
	log(0, false, 0, TEXT("=========================================================\n"));
	log(0, false, 0, TEXT("EIT file dump:\n\n"));

	for(int i = 0; i < (int)sections.size(); i++)
	{
		if(sections[i].compare("General") == 0)
		{
			log(0, false, 0, TEXT("[General]\n"));

			m_SageEitIP = CIniFile::GetValue("SageEitIP", sections[i], fileName);
			log(0, false, 0, TEXT("SageEitIP = %s\n"), m_SageEitIP.c_str());

			m_SageEitPort = (USHORT)atoi(CIniFile::GetValue("SageEitPort", sections[i], fileName).c_str());
			log(0, false, 0, TEXT("SageEitPort = %u\n"), m_SageEitPort);

			m_SaveXmltvFileLocation = CIniFile::GetValue("SaveXmltvFileLocation", sections[i], fileName);
			log(0, false, 0, TEXT("SaveXmltvFileLocation = %s\n"), m_SaveXmltvFileLocation.c_str());

			m_TempFileLocation = CIniFile::GetValue("TempFileLocation", sections[i], fileName);
			log(0, false, 0, TEXT("TempFileLocation = %s\n"), m_TempFileLocation.c_str());

			m_CollectionDurationMinutes = atoi(CIniFile::GetValue("CollectionDurationMinutes", sections[i], fileName).c_str());
			log(0, false, 0, TEXT("CollectionDurationMinutes = %d\n"), m_CollectionDurationMinutes);

			log(0, false, 0, TEXT("\n"));
		}
		else
		{
			struct eitRecord newrec;
			newrec.ONID = atoi(sections[i].c_str());
			newrec.lineup = CIniFile::GetValue("Lineup", sections[i], fileName);	
			newrec.chan = atol(CIniFile::GetValue("ChanOrSID", sections[i], fileName).c_str());

			newrec.includedSIDs.clear();
			buffer = CIniFile::GetValue("IncludedSIDs", sections[i], fileName);
#pragma warning(disable:4996)
			size_t length = buffer.copy(tbuffer, buffer.length(), 0);
#pragma warning(default:4996)
			buffer[length] = '\0';
			if(buffer[0] != TCHAR(0))
			{
				for(LPCTSTR token = _tcstok_s(tbuffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
					newrec.includedSIDs.insert((USHORT)_ttoi(token));
			}

			log(0, false, 0, TEXT("[%d]\n"), newrec.ONID);
			log(0, false, 0, TEXT("Lineup = %s\n"), newrec.lineup.c_str());
			log(0, false, 0, TEXT("ChanOrSID = %lu\n"), newrec.chan);

			log(0, false, 0, TEXT("IncludedSIDs="));
			for(hash_set<USHORT>::const_iterator it = newrec.includedSIDs.begin(); it != newrec.includedSIDs.end();)
			{
				log(0, false, 0, TEXT("%d"), *it);
				if(++it != newrec.includedSIDs.end())
					log(0, false, 0, TEXT(","));
			}
			log(0, false, 0, TEXT("\n"));
			log(0, false, 0, TEXT("\n"));

			m_eitRecords.push_back(newrec);
		}
	}

	log(0, false, 0, TEXT("\n"));

	log(0, false, 0, TEXT("End of EIT file dump\n"));
	log(0, false, 0, TEXT("=========================================================\n\n"));
}

DWORD WINAPI EitCollectionCallback(LPVOID vpEit)
{
	EIT* eitObj = (EIT*)vpEit;

	// Jump back up into the object
	eitObj->RealEitCollectionCallback();

	return 0;
}

void EIT::StartEitMonitoring()
{
	// Start the timer thread
	m_EitCollectionThreadCanEnd = false;
	m_EitCollectionThread = CreateThread(NULL, 0, EitCollectionCallback, this, 0, &m_EitCollectionThreadId);
}

void EIT::RealEitCollectionCallback()
{
	time_t now;
	char tempFile[MAX_PATH];

	sprintf_s(tempFile, sizeof(tempFile), "%s\\TempEitGathering.ts",  m_TempFileLocation.c_str());

	log(2, true, 0, TEXT("Collecting EIT Data for onid %hu.\n"), m_onid);
			
	lock();
	// Get rid of the old stuff
	m_EITevents.clear();
	m_CollectEIT = true;
	unlock();

	time(&now);
	time(&m_Time);

	// Allow for collection of the data
	while(difftime(now, m_Time) < ( (m_CollectionDurationMinutes-1) * 60))
	{
		Sleep(5000);
		if(m_EitCollectionThreadCanEnd == true)
		{
			SendSocketCommand("STOP ***EITGATHERING***\r\n");
			lock();
			m_CollectEIT = false;
			unlock();
			Sleep(2000);
			DeleteFile(tempFile);
			return;
		}

		time(&now);
	}


	//lock();
	m_CollectEIT = false;
	//unlock();


	// Send socket command make to stop the recording and release the tuner
	// "STOP SageTV DVB-S2 Enhancer 1 Digital TV Tuner"
	//SendSocketCommand("STOP ***EITGATHERING***\r\n");

	//Sleep(5000);


	// Dump data to xmltv file if configured to do so
	if(m_SaveXmltvFileLocation.length() > 0) {
		log(3, true, 0, TEXT("tries to dump xml\n"));
		dumpXmltvFile(m_onid);
	}

	// Send data to SageTV server if configured to do so
	if(m_SageEitIP.length() > 0) {
		log(3, true, 0, TEXT("tries to send to sage\n"));
		sendToSage(m_onid);
	}
	// Clean up after ourselves
	DeleteFile(tempFile);

}

void EIT::sendToSage(int onid)
{

}

bool EIT::SendSocketCommand(char *command)
{
	int port = g_pConfiguration->getListeningPort();

	SOCKET s; 
    SOCKADDR_IN target; 
    target.sin_family = AF_INET; 
    target.sin_port = htons((USHORT)port); 
    target.sin_addr.s_addr = inet_addr ("127.0.0.1"); 

	log(3, true, 0, TEXT("EIT is attempting to send the following command to DVBE4SAGE:\n"));
	log(3, false, 0, TEXT("%s\n"), command);

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
        return false; 

    if (connect(s, (SOCKADDR *)&target, sizeof(target)) == SOCKET_ERROR)
        return false; 
	
	if(send(s, command, strlen(command), 0) == SOCKET_ERROR)
	{
		shutdown(s, SD_SEND);
		closesocket(s);
        return false; 
	}

	shutdown(s, SD_SEND);
	closesocket(s);

	return true;
}

void EIT::dumpXmltvFile(int onid)
{
	TCHAR channelName[256];
	FILE* outFile = NULL;


		NetworkProvider& encoderNetworkProvider =  g_pEncoder->getNetworkProvider();

		eitRecord eitRec = GetEitRecord(onid);

		string fileName = m_SaveXmltvFileLocation + "\\" + eitRec.lineup + ".xml";

		log(3, true, 0, TEXT("EIT is attempting to dump XMLTV data to %s.\n"), fileName.c_str());
		
		if(_tfopen_s(&outFile, fileName.c_str(), TEXT("wt")) == 0)
		{
			// Standard header
			_ftprintf(outFile, TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"));
			_ftprintf(outFile, TEXT("<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\n"));
			_ftprintf(outFile, TEXT("<tv source-info-name=\"%s\" generator-info-name=\"DVBE4SAGE\" generator-info-url=\"http://code.google.com/p/dvbe4sage/\">\n"), eitRec.lineup.c_str());

			// Loop through the services and create all the channel records
			for(hash_map<UINT32, Service>::const_iterator it = encoderNetworkProvider.m_Services.begin(); it != encoderNetworkProvider.m_Services.end(); it++)
			{
				hash_set<USHORT>::const_iterator it2 = eitRec.includedSIDs.find(it->second.sid); 
			//	if(onid == it->second.onid && (eitRec.includedSIDs.empty() == true || it2 != eitRec.includedSIDs.end()))
				if(eitRec.includedSIDs.empty() == true || it2 != eitRec.includedSIDs.end())
				{
					// Get the channel name and the mapped number 
					channelName[0] = TCHAR('\0');
					const UINT32 usid = NetworkProvider::getUniqueSID(it->second.onid, it->second.sid);

					int chanNo = (encoderNetworkProvider.getChannelForSid(usid) == NULL) ? it->second.sid : it->second.channelNumber;
					encoderNetworkProvider.getServiceName(usid, channelName, sizeof(channelName) / sizeof(channelName[0]));

					string chName = ReplaceAll((string)channelName, "&", "&amp;");

					_ftprintf(outFile, TEXT("\t<channel id=\"I%d.%d.DVBE4SAGE\">\n"), it->second.sid, it->second.onid);
					_ftprintf(outFile, TEXT("\t\t<display-name>%d %s</display-name>\n"), chanNo, chName.c_str());
					_ftprintf(outFile, TEXT("\t\t<display-name>%d</display-name>\n"), chanNo);
					_ftprintf(outFile, TEXT("\t\t<display-name>%s</display-name>\n"), chName.c_str());

					_ftprintf(outFile, TEXT("\t</channel>\n"));
				}
			}

	
			// Loop through the EIT data and create all of the programme records
			for(hash_map<UINT32, EITEvent>::const_iterator it = m_EITevents.begin(); it != m_EITevents.end(); it++)
			{
				hash_set<USHORT>::const_iterator it2 = eitRec.includedSIDs.find((USHORT)it->second.SID); 
			//	if(onid == it->second.ONID && (eitRec.includedSIDs.empty() == true || it2 != eitRec.includedSIDs.end()))
				if(eitRec.includedSIDs.empty() == true || it2 != eitRec.includedSIDs.end())
				{
					EPGLanguage lang = GetDescriptionRecord(it->second);

					_ftprintf(outFile, TEXT("\t<programme start=\"%s\" stop=\"%s\" channel=\"I%d.%d.DVBE4SAGE\">\n"), it->second.startDateTime.c_str(), it->second.stopDateTime.c_str(), it->second.SID, it->second.ONID);

				
				
					lang.shortDescription = ReplaceAll(lang.shortDescription, "&", "&amp;");
					_ftprintf(outFile, TEXT("\t\t<title lang=\"%s\">%s</title>\n"), lang.text.c_str(), lang.shortDescription.c_str());

					lang.longDescription = ReplaceAll(lang.longDescription, "&", "&amp;");
					lang.eventText = ReplaceAll(lang.eventText, "&", "&amp;");
					_ftprintf(outFile, TEXT("\t\t<desc lang=\"%s\">%s</desc>\n"), lang.text.c_str(), (lang.longDescription.empty() == false) ? lang.longDescription.c_str() : lang.eventText.c_str());
										
					if(it->second.category.empty() == false)
						_ftprintf(outFile, TEXT("\t\t<category lang=\"%s\">%s</category>\n"), lang.text.c_str(), it->second.category.c_str());

					if(it->second.dishCategory1.empty() == false)
						_ftprintf(outFile, TEXT("\t\t<category lang=\"%s\">%s</category>\n"), lang.text.c_str(), it->second.dishCategory1.c_str());

					if(it->second.dishCategory2.empty() == false)
						_ftprintf(outFile, TEXT("\t\t<category lang=\"%s\">%s</category>\n"), lang.text.c_str(), it->second.dishCategory2.c_str());

					if(it->second.seriesID.empty() == false)
						_ftprintf(outFile, TEXT("\t\t<episode-num system=\"dd_progid\">%s</episode-num>\n"), it->second.seriesID.c_str());

					if(it->second.year != 0)
						_ftprintf(outFile, TEXT("\t\t<date>%d</date>\n"), it->second.year);

					if(it->second.stereo == true)
					{
						_ftprintf(outFile, TEXT("\t\t<audio>\n"));
						_ftprintf(outFile, TEXT("\t\t\t<stereo>stereo</stereo>\n"));
						_ftprintf(outFile, TEXT("\t\t</audio>\n"));
					}

					if(it->second.aspect != 0)
					{
						_ftprintf(outFile, TEXT("\t\t<video>\n"));
						_ftprintf(outFile, TEXT("\t\t\t<aspect>%s</aspect>\n"), getAspect(it->second.aspect).c_str());
						_ftprintf(outFile, TEXT("\t\t\t<quality>%s</quality>\n"), getQuality(it->second.aspect).c_str());
						_ftprintf(outFile, TEXT("\t\t</video>\n"));
					}

					if(it->second.CC == true)
						_ftprintf(outFile, TEXT("\t\t<subtitles type=\"closed caption\"/>\n"));

					if(it->second.teletext == true)
						_ftprintf(outFile, TEXT("\t\t<subtitles type=\"teletext\"/>\n"));

					if(it->second.onscreen == true)
						_ftprintf(outFile, TEXT("\t\t<subtitles type=\"on screen\"/>\n"));

					if(it->second.vchipRating.empty() == false)
					{
						_ftprintf(outFile, TEXT("\t\t<rating system=\"VCHIP\">\n"));
						_ftprintf(outFile, TEXT("\t\t\t<value>%s</value>\n"), it->second.vchipRating.c_str());
						_ftprintf(outFile, TEXT("\t\t</rating>\n"));
					}

					if(it->second.vchipAdvisory != 0)
					{
						if(it->second.vchipAdvisory & 0x01) dumpXmltvAdvisory(outFile, "Fantasy Violence");
						if(it->second.vchipAdvisory & 0x02) dumpXmltvAdvisory(outFile, "Violence");
						if(it->second.vchipAdvisory & 0x04) dumpXmltvAdvisory(outFile, "Sexual Situations");
						if(it->second.vchipAdvisory & 0x08) dumpXmltvAdvisory(outFile, "Crude Language");
						if(it->second.vchipAdvisory & 0x10) dumpXmltvAdvisory(outFile, "Suggestive Dialog");
					}

					if(it->second.mpaaRating.empty() == false)
					{
						_ftprintf(outFile, TEXT("\t\t<rating system=\"MPAA\">\n"));
						_ftprintf(outFile, TEXT("\t\t\t<value>%s</value>\n"), it->second.mpaaRating.c_str());
						_ftprintf(outFile, TEXT("\t\t</rating>\n"));
					}

					if(it->second.mpaaAdvisory != 0)
					{
						if(it->second.mpaaAdvisory & 0x01) dumpXmltvAdvisory(outFile, "Sexual Content");
						if(it->second.mpaaAdvisory & 0x02) dumpXmltvAdvisory(outFile, "Language");
						if(it->second.mpaaAdvisory & 0x04) dumpXmltvAdvisory(outFile, "Mild Sensuality");
						if(it->second.mpaaAdvisory & 0x08) dumpXmltvAdvisory(outFile, "Fantasy Violence");
						if(it->second.mpaaAdvisory & 0x10) dumpXmltvAdvisory(outFile, "Violence");
						if(it->second.mpaaAdvisory & 0x20) dumpXmltvAdvisory(outFile, "Mild Peril");
						if(it->second.mpaaAdvisory & 0x40) dumpXmltvAdvisory(outFile, "Nudity");
					}

					if(it->second.starRating.empty() == false)
					{
						_ftprintf(outFile, TEXT("\t\t<star-rating>\n"));
						_ftprintf(outFile, TEXT("\t\t\t<value>%s</value>\n"), it->second.starRating.c_str());
						_ftprintf(outFile, TEXT("\t\t</star-rating>\n"));
					}

					if(it->second.parentalRating != 0)
					{
						_ftprintf(outFile, TEXT("\t\t<rating>\n"));
						_ftprintf(outFile, TEXT("\t\t\t<value>%s</value>\n"), getParentalRating(it->second.parentalRating).c_str());
						_ftprintf(outFile, TEXT("\t\t</rating>\n"));
					}
					_ftprintf(outFile, TEXT("\t</programme>\n"));
				}
			}

			_ftprintf(outFile, TEXT("</tv>\n"));
			fclose(outFile);
	}
			log(3, true, 0, TEXT("EIT dump of XMLTV data to %s is complete.\n"), fileName.c_str());
}

void EIT::dumpXmltvAdvisory(FILE* outFile, string value)
{
		_ftprintf(outFile, TEXT("\t\t<rating system=\"advisory\">\n"));
		_ftprintf(outFile, TEXT("\t\t\t<value>%s</value>\n"), value.c_str());
		_ftprintf(outFile, TEXT("\t\t</rating>\n"));
}

// Convert the DVB standrad mininum age to a rating.  The age breakdowns are my guess
string EIT::getParentalRating(int rating)
{
	string ret = "";

	if(rating < 7)
		ret = "G";
	else
	if(rating <= 10)
		ret = "PG";	
	else
	if(rating <= 13)
		ret = "PG-13";
	else
	if(rating <= 15)
		ret = "R";
	else
	if(rating <= 17)
		ret = "NC-17";
	else
		ret = "AO";

	return ret;
}

string EIT::getAspect(int aspect)
{
	string ret = "";

	switch (aspect)
	{
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0E:
		case 0x0F:
		case 0x10:
			ret = "16:9";
			break;

		default:
			ret = "4:3";
			break;
	}

	return ret;
}

string EIT::getQuality(int aspect)
{
	string ret = "";

	if(aspect < 9)
		ret = "SDTV";
	else
		ret = "HDTV";

	return ret;
}

// Stop collecting EIT data
void EIT::stopCollectingEIT()
{
	m_EitCollectionThreadCanEnd = true;
}

void EIT::lock()
{
	m_cs.Lock();
}

void EIT::unlock()
{
	m_cs.Unlock();
}

// Parse the guide data out of the EIT
void EIT::parseEITTable(const eit_t* const table, int remainingLength)
{
	if(m_CollectEIT == false)
		return;

	const USHORT serviceID = HILO(table->service_id);
	const USHORT networkID = HILO(table->original_network_id);

	// Get hold on the input buffer
	const BYTE* inputBuffer = (const BYTE*)table + EIT_LEN;

	// Update remaining length by removing the CRC and the header
	remainingLength -= CRC_LENGTH + EIT_LEN;

	// Loop through the events
	while(remainingLength != 0)
	{
		// This is a new event we need to take care of
		const eit_event_t* const currentEvent = (const eit_event_t*)inputBuffer;

		// Get descriptor loop length
		const UINT descriptorLoopLength = HILO(currentEvent->descriptors_loop_length);

		// Get the event ID
		const USHORT eventID = HILO(currentEvent->event_id);

		// Get the version
//				const BYTE version = table->version_number;

		lock();

		// Let's try to find the event first
		stdext::hash_map<UINT32, EITEvent>::iterator it = m_EITevents.find(getUniqueKey(eventID, serviceID));

		// If not found or different version, go ahead
		if(it == m_EITevents.end())
		{
			EITEvent newEvent;

			// Take care of dates
			const int mjd = HILO(currentEvent->mjd);

			/*
			*  * Use the routine specified in ETSI EN 300 468 V1.8.1,
			*  * "Specification for Service Information in Digital Video Broadcasting"
			*  * to convert from Modified Julian Date to Year, Month, Day.
			*  */
			int year = (int)((mjd - 15078.2) / 365.25);

	//int month = int((odate - 14956.1 - int(year * 365.25)) / 30.6001);
	//int day = odate - 14956 - int(year * 365.25) - int(month * 30.6001);
	//int offset = 0;
	//if ((month == 14) || (month == 15))    
	//		offset = 1;

	//year = year + offset + 1900;
	//month = month - 1 - (offset * 12);



			int month = (int) ((mjd - 14956.1 - (int) (year * 365.25)) / 30.6001);
			const int day = mjd - 14956 - (int)(year * 365.25) - (int)(month * 30.6001);
			int k = (month == 14 || month == 15) ? 1 : 0;
			year += k;
			month = month - 1 - (k * 12);

			tm dvb_time;
			dvb_time.tm_mday = day;
			dvb_time.tm_mon = month;
			dvb_time.tm_year = year;
			dvb_time.tm_isdst = 0;
			dvb_time.tm_wday = dvb_time.tm_yday = 0;

			// Get program start time
			dvb_time.tm_sec =  bcdtoint(currentEvent->start_time_s);
			dvb_time.tm_min =  bcdtoint(currentEvent->start_time_m);
			dvb_time.tm_hour = bcdtoint(currentEvent->start_time_h);
			const time_t startTime = _mkgmtime(&dvb_time);

			// Get program end time
			dvb_time.tm_sec  += bcdtoint(currentEvent->duration_s);
			dvb_time.tm_min  += bcdtoint(currentEvent->duration_m);
			dvb_time.tm_hour += bcdtoint(currentEvent->duration_h);
			const time_t stopTime = _mkgmtime(&dvb_time);

			// Get time of right now
			time_t now;
			time(&now);

			// Basic bad date check. if the program ends before this time yesterday, 
			// or two weeks from today, forget it.
			if((difftime(stopTime, now) >= -86400 && difftime(now, stopTime) <= 86400 * 14))
			{
				// Log the program info
				log(3, false, 0, TEXT("<program start - NID: %d (0x%x)  SID: %d (0x%x) "), networkID, networkID, serviceID, serviceID);

				// Save the raw start and stop times for pruning and sorting purposes later
				newEvent.startTime = startTime;
				newEvent.stopTime = stopTime;

				// Start and stop times
				tm nuTime;
				localtime_s(&nuTime, &startTime);					
				static char	date_strbuf[MAX_DATE_LENGTH];
				strftime(date_strbuf, sizeof(date_strbuf), "%Y.%m.%d %H:%M:%S", &nuTime);

				log(3, false, 0, TEXT("start=%s +0000 "), date_strbuf);
				newEvent.startDateTime = date_strbuf;

				localtime_s(&nuTime, &stopTime);
				strftime(date_strbuf, sizeof(date_strbuf), "%Y.%m.%d %H:%M:%S", &nuTime);

				log(3, false, 0, TEXT("stop=%s +0000>\n"), date_strbuf);
				newEvent.stopDateTime = date_strbuf;
			
				// Event ID
				//log(3, false, 0, TEXT("\tEvent ID = 0x%x\n"), eventID);
				newEvent.eventID = eventID;

				// Service ID and Network ID
				newEvent.SID = serviceID;
				newEvent.ONID = networkID;

				// Descriptions and everything else
				parseEventDescriptors(inputBuffer + EIT_EVENT_LEN, descriptorLoopLength, table, &newEvent);

				//log(3, false, 0, TEXT("<program end>\n"));

				// Add the newly created event to the list
				m_EITevents[getUniqueKey(eventID, serviceID)] = newEvent;
				unlock();
			}
		}
		else
			log(3, false, 0, TEXT("Duplicate event found!  Event ID: 0x%x\n"), eventID);

		// Adjust input buffer
		inputBuffer += descriptorLoopLength + EIT_EVENT_LEN;

		// Adjust remaining length
		remainingLength -= descriptorLoopLength + EIT_EVENT_LEN;
	}
}

void EIT::parseEventDescriptors(const BYTE* inputBuffer,
									  int remainingLength,
									  const eit_t* const table, EITEvent* newEvent)
{
	// Loop through the descriptors till the end
	while(remainingLength != 0)
	{
		// Get handle on general descriptor first
		const descr_gen_t* const generalDescriptor = CastGenericDescriptor(inputBuffer);
		const int descriptorLength = generalDescriptor->descriptor_length;

		switch(generalDescriptor->descriptor_tag)
		{
			case 0x4D: decodeShortEventDescriptor(inputBuffer, newEvent); break;

			case 0x4E: decodeExtendedEvent(inputBuffer, newEvent); break;

			case 0x54: decodeContentDescriptor(inputBuffer, newEvent); break;

			case 0x50: decodeComponentDescriptor(inputBuffer, newEvent); break;

			case 0x55: decodeParentalRatingDescriptor(inputBuffer, newEvent); break;
			////
			////case 0x5F: /* Private Data Descriptor */ break;

			////case 0x86: DecodeProviderIdDescriptor(inputBuffer, newEvent); break;

			case 0x89: decodeCombinedStarRating_MPAARatingDescriptor(inputBuffer, newEvent); break;

			case 0x91: decodeDishShortDescription(inputBuffer, (table->table_id > 0x80 ? 2 : 1), newEvent); break;

			case 0x92: decodeDishLongDescription(inputBuffer, (table->table_id > 0x80 ? 2 : 1), newEvent); break;

			case 0x94: decodeDishCcStereoDescriptor(inputBuffer, (table->table_id > 0x80 ? 2 : 1), newEvent); break;

			case 0x95: decodeDishVChipRatingDescriptor(inputBuffer, newEvent); break;

			case 0x96: decodeDishSeriesDescriptor(inputBuffer, newEvent); break;

			default: break;
		}

		// Adjust input Buffer
		inputBuffer += descriptorLength + DESCR_GEN_LEN;

		// Adjust remaining length
		remainingLength -= descriptorLength + DESCR_GEN_LEN;	
	}
}

void EIT::decodeShortEventDescriptor(const BYTE* inputBuffer, EITEvent* newEvent)
{
	const descr_short_event_t* const shortDescription = CastShortEventDescriptor(inputBuffer);

	// Let's see first what language it is in
	const std::string language((const char*)inputBuffer + DESCR_GEN_LEN, 3);

	// Decompress it
	CAutoString buffer(shortDescription->event_name_length + 10);
	CDecompress::GetString468A((BYTE*)inputBuffer + DESCR_SHORT_EVENT_LEN, shortDescription->event_name_length, buffer.GetBuffer());
	std::string eventText(buffer.GetBuffer()); 

	// Sky seems to have these characters as some sort of delimiter
	eventText = ReplaceAll(eventText, "", "");

	// log the title of the event
	log(3, false, 0, TEXT("\tTitle: !%s!\n"), eventText.c_str());
	log(3, false, 0, TEXT("\tLanguage: !%.3s!\n"), language.c_str());

	// Now let's get the length of the description
	int descriptionLength = *(inputBuffer + DESCR_SHORT_EVENT_LEN + shortDescription->event_name_length);

	// Decompress it
	CAutoString buffer2(descriptionLength + 10);
	CDecompress::GetString468A((BYTE*)inputBuffer + DESCR_SHORT_EVENT_LEN + shortDescription->event_name_length + 1, descriptionLength, buffer2.GetBuffer());
	std::string descriptionText(buffer2.GetBuffer()); 

	// Sky seems to have these characters as some sort of delimiter
	descriptionText = ReplaceAll(descriptionText, "", "");

	unsigned long ISO_639_language_code = (shortDescription->lang_code1<<16) + (shortDescription->lang_code2<<8) + shortDescription->lang_code3;

	EITEvent::ivecLanguages it = newEvent->vecLanguages.begin();
	for (it = newEvent->vecLanguages.begin(); it != newEvent->vecLanguages.end(); ++it)
	{
		EPGLanguage& lang = *it;
		if (lang.language == ISO_639_language_code)
		{
			lang.shortDescription = eventText;
			lang.longDescription = descriptionText;
			lang.text = language;
			return;
		}
	}
	EPGLanguage lang;
	lang.CR_added = false;
	lang.shortDescription = eventText;
	lang.longDescription = descriptionText;
	lang.parentalRating = 0;
	lang.language = ISO_639_language_code;
	lang.text = language;
	newEvent->vecLanguages.push_back(lang);

	// Log the description of the event
	log(3, false, 0, TEXT("\tDescription: !%s!\n"), descriptionText.c_str());
	log(3, false, 0, TEXT("\tLanguage: !%d!\n"), ISO_639_language_code);
}

// Not used in North America
void EIT::decodeExtendedEvent(const BYTE* inputBuffer, EITEvent* newEvent)
{
	int descriptor_tag;
	int descriptor_length;
	int descriptor_number;
	int last_descriptor_number;
	int text_length;
	int length_of_items;

	string text = "";
	string item = "";
	int pointer = 0;
	int lenB;
	int len1;
	int item_description_length;
	int item_length;

	descriptor_tag = inputBuffer[0];
	descriptor_length = inputBuffer[1];
	descriptor_number = (inputBuffer[2]>>4) & 0xF;
	last_descriptor_number = inputBuffer[2] & 0xF;
	DWORD language=(inputBuffer[3]<<16)+(inputBuffer[4]<<8)+inputBuffer[5];
	length_of_items = inputBuffer[6];
	pointer = 7;
	lenB = descriptor_length - 5;
	len1 = length_of_items;

	while (len1 > 0)
	{
		item_description_length = inputBuffer[pointer];
		if (item_description_length< 0 || pointer+item_description_length > descriptor_length+2) 
		{
			return;
		}

		pointer += (1 + item_description_length);

		item_length = inputBuffer[pointer];
		if (item_length < 0 || pointer+item_length>descriptor_length+2) 
		{
			return;
		}

		if (item_length>0)
		{
			CAutoString buffer2 (item_length*4);
			CDecompress::GetString468A((BYTE*)inputBuffer + pointer+1, item_length, buffer2.GetBuffer());
			item = buffer2.GetBuffer();
		}

		pointer += (1 + item_length);
		len1 -= (2 + item_description_length + item_length);
		lenB -= (2 + item_description_length + item_length);
	};

	pointer=7+length_of_items;
	text_length = inputBuffer[pointer];
	pointer += 1;
	if (text_length< 0 || pointer+text_length>descriptor_length+2) 
	{
		return;
	}

	if (text_length>0)
	{
		CAutoString buffer (text_length*4);
		CDecompress::GetString468A((BYTE*)inputBuffer + pointer, text_length, buffer.GetBuffer());
		text = buffer.GetBuffer();
	}

	//find language...
	EITEvent::ivecLanguages it = newEvent->vecLanguages.begin();
	for (it = newEvent->vecLanguages.begin(); it != newEvent->vecLanguages.end();++it)
	{
		EPGLanguage& lang=*it;
		if (lang.language==language)
		{
			//found.
			if (item.size()>0 && lang.eventText.size()<1)
				lang.eventText+=item;
			if (text.size()>0 && strcmp(item.c_str(),text.c_str())!=0)
			{
				if (lang.text.size()>0)
				{
					if ((BYTE)text[0]<0x20)
					{
						if (!lang.CR_added)
						{
							lang.text+="\n";
							lang.CR_added=true;
						}
						if (text.size()>1)
							lang.text+=text.erase(0,1);
					}
					else
						lang.text+=text;
				}
				else
					lang.text=text;
			}

			return;
		}
	}

	// Add new language...
	EPGLanguage lang;
	lang.CR_added = false;
	lang.language = language;
	if (item.size() > 0)
		lang.eventText = item;
	if (text.size() > 0 && strcmp(item.c_str(),text.c_str()) != 0)
		lang.text = text;
	lang.parentalRating = 0;
	newEvent->vecLanguages.push_back(lang);

	log(3, false, 0, TEXT("\tExtended Event: %s  Language: %d\n"), lang.text.c_str(), lang.language);
}

void EIT::decodeContentDescriptor (const BYTE* inputBuffer, EITEvent* newEvent)
{
	// If this is for dish network or bell express, use the alternate routine
	if(inputBuffer[3] == 0xFF)
	{
		decodeContentDescriptorDish(inputBuffer,newEvent);
		return;
	}

	const descr_content_t* const contentDescriptor = CastContentDescriptor(inputBuffer);
	int nibble=0;

	// Let's loop through the nibbles
	for(int i = 0; i < contentDescriptor->descriptor_length / DESCR_CONTENT_LEN; i++)
	{
		// Get nibbles
		const nibble_content_t* const nc = CastContentNibble(inputBuffer + (i + 1) * DESCR_CONTENT_LEN);

		// Weird, but need to assemble them back
		BYTE c1 = (BYTE)((nc->content_nibble_level_1 << 4) + (BYTE)(nc->content_nibble_level_2));
		BYTE c2 = (BYTE)((nc->user_nibble_1 << 4) + (BYTE)(nc->user_nibble_2));

		std::string genreText;
		nibble = (c1 << 8) | c2;

		// From  Digital Video Broadcasting (DVB); Specification for Service Information (SI) in DVB systems  
		// EN300468v1.11.1.pdf (2010-04)
		// Table 28: Content_nibble level 1 and 2 assignments
		switch(nibble)
		{
				// Movie/Drama
			case 0x0100: genreText = std::string("movie/drama (general)");break;
			case 0x0101: genreText = std::string("detective/thriller");break;
			case 0x0102: genreText = std::string("adventure/western/war");break;
			case 0x0103: genreText = std::string("science fiction/fantasy/horror");break;
			case 0x0104: genreText = std::string("comedy");break;
			case 0x0105: genreText = std::string("soap/melodram/folkloric");break;
			case 0x0106: genreText = std::string("romance");break;
			case 0x0107: genreText = std::string("serious/classical/religious/historical movie/drama");break;
			case 0x0108: genreText = std::string("adult movie/drama");break;

			case 0x010E: genreText = std::string("reserved");break;
			case 0x010F: genreText = std::string("user defined");break;

				// News/Current Affairs
			case 0x0200: genreText = std::string("news/current affairs (general)");break;
			case 0x0201: genreText = std::string("news/weather report");break;
			case 0x0202: genreText = std::string("news magazine");break;
			case 0x0203: genreText = std::string("documentary");break;
			case 0x0204: genreText = std::string("discussion/interview/debate");break;

			case 0x020E: genreText = std::string("reserved");break;
			case 0x020F: genreText = std::string("user defined");break;

				// Show/Game Show
			case 0x0300: genreText = std::string("show/game show (general)");break;
			case 0x0301: genreText = std::string("game show/quiz/contest");break;
			case 0x0302: genreText = std::string("variety show");break;
			case 0x0303: genreText = std::string("talk show");break;

			case 0x030E: genreText = std::string("reserved");break;
			case 0x030F: genreText = std::string("user defined");break;

				// Sports
			case 0x0400: genreText = std::string("sports (general)");break;
			case 0x0401: genreText = std::string("special events");break;
			case 0x0402: genreText = std::string("sports magazine");break;
			case 0x0403: genreText = std::string("football/soccer");break;
			case 0x0404: genreText = std::string("tennis/squash");break;
			case 0x0405: genreText = std::string("team sports");break;
			case 0x0406: genreText = std::string("athletics");break;
			case 0x0407: genreText = std::string("motor sport");break;
			case 0x0408: genreText = std::string("water sport");break;
			case 0x0409: genreText = std::string("winter sport");break;
			case 0x040A: genreText = std::string("equestrian");break;
			case 0x040B: genreText = std::string("martial sports");break;

			case 0x040E: genreText = std::string("reserved");break;
			case 0x040F: genreText = std::string("user defined");break;

				// Children's/Youth Programs
			case 0x0500: genreText = std::string("childrens's/youth program (general)");break;
			case 0x0501: genreText = std::string("pre-school children's program");break;
			case 0x0502: genreText = std::string("entertainment (6-14 year old)");break;
			case 0x0503: genreText = std::string("entertainment (10-16 year old)");break;
			case 0x0504: genreText = std::string("information/education/school program");break;
			case 0x0505: genreText = std::string("cartoon/puppets");break;

			case 0x050E: genreText = std::string("reserved");break;
			case 0x050F: genreText = std::string("user defined");break;

				// Music/Ballet/Dance
			case 0x0600: genreText = std::string("music/ballet/dance (general)");break;
			case 0x0601: genreText = std::string("rock/pop");break;
			case 0x0602: genreText = std::string("serious music/classic music");break;
			case 0x0603: genreText = std::string("folk/traditional music");break;
			case 0x0604: genreText = std::string("jazz");break;
			case 0x0605: genreText = std::string("musical/opera");break;
			case 0x0606: genreText = std::string("ballet");break;

			case 0x060E: genreText = std::string("reserved");break;
			case 0x060F: genreText = std::string("user defined");break;

				// Arts/Culture (without music)
			case 0x0700: genreText = std::string("arts/culture (without music, general)");break;
			case 0x0701: genreText = std::string("performing arts");break;
			case 0x0702: genreText = std::string("fine arts");break;
			case 0x0703: genreText = std::string("religion");break;
			case 0x0704: genreText = std::string("popular culture/traditional arts");break;
			case 0x0705: genreText = std::string("literature");break;
			case 0x0706: genreText = std::string("film/cinema");break;
			case 0x0707: genreText = std::string("experimental film/video");break;
			case 0x0708: genreText = std::string("broadcasting/press");break;
			case 0x0709: genreText = std::string("new media");break;
			case 0x070A: genreText = std::string("arts/culture magazine");break;
			case 0x070B: genreText = std::string("fashion");break;

			case 0x070E: genreText = std::string("reserved");break;
			case 0x070F: genreText = std::string("user defined");break;

				// Social/Political Issues/Economics
			case 0x0800: genreText = std::string("social/political issues/economics (general)");break;
			case 0x0801: genreText = std::string("magazines/reports/documentary");break;
			case 0x0802: genreText = std::string("economics/social advisory");break;
			case 0x0803: genreText = std::string("remarkable people");break;

			case 0x080E: genreText = std::string("reserved");break;
			case 0x080F: genreText = std::string("user defined");break;

				// Education/Science/Factual Topics
			case 0x0900: genreText = std::string("education/science/factual topics (general)");break;
			case 0x0901: genreText = std::string("nature/animals/environment");break;
			case 0x0902: genreText = std::string("technology/natural science");break;
			case 0x0903: genreText = std::string("medicine/physiology/psychology");break;
			case 0x0904: genreText = std::string("foreign countries/expeditions");break;
			case 0x0905: genreText = std::string("social/spiritual science");break;
			case 0x0906: genreText = std::string("further education");break;
			case 0x0907: genreText = std::string("languages");break;

			case 0x090E: genreText = std::string("reserved");break;
			case 0x090F: genreText = std::string("user defined");break;

				// Leisure Hobbies
			case 0x0A00: genreText = std::string("leisure hobbies (general)");break;
			case 0x0A01: genreText = std::string("tourism/travel");break;
			case 0x0A02: genreText = std::string("handicraft");break;
			case 0x0A03: genreText = std::string("motoring");break;
			case 0x0A04: genreText = std::string("fitness & health");break;
			case 0x0A05: genreText = std::string("cooking");break;
			case 0x0A06: genreText = std::string("advertisement/shopping");break;
			case 0x0A07: genreText = std::string("gardening");break;

			case 0x0A0E: genreText = std::string("reserved");break;
			case 0x0A0F: genreText = std::string("user defined");break;

				// Special Characteristics
			case 0x0B00: genreText = std::string("original language");break;
			case 0x0B01: genreText = std::string("black & white");break;
			case 0x0B02: genreText = std::string("unpublished");break;
			case 0x0B03: genreText = std::string("live broadcast");break;

			case 0x0B0E: genreText = std::string("reserved");break;
			case 0x0B0F: genreText = std::string("user defined");break;

			default: genreText = std::string(""); break;
		}

		// Output content if present
		if(genreText.length() > 0)
		{
			log(3, false, 0, TEXT("\tCategory: %s\n"), genreText.c_str());
		}
		newEvent->category = genreText;
	}
}

void EIT::decodeContentDescriptorDish (const BYTE* inputBuffer, EITEvent* newEvent)
{
	const descr_content_t* const contentDescriptor = CastContentDescriptor(inputBuffer);

	// Let's loop through the nibbles
	for(int i = 0; i < contentDescriptor->descriptor_length / DESCR_CONTENT_LEN; i++)
	{
		// Get nibbles
		const nibble_content_t* const nc = CastContentNibble(inputBuffer + (i + 1) * DESCR_CONTENT_LEN);

		std::string genreText;
		std::string genreText2;

		// This is what Dish Network Uses

		// Theme
		switch(nc->content_nibble_level_1)
		{
			case 0x01: genreText = std::string("Movie");break;
			case 0x02: genreText = std::string("Sports");break;
			case 0x03: genreText = std::string("News/Business");break;
			case 0x04: genreText = std::string("Family/Children");break;
			case 0x05: genreText = std::string("Education");break;
			case 0x06: genreText = std::string("Series/Special");break;
			case 0x07: genreText = std::string("Art/Music");break;
			case 0x08: genreText = std::string("Religious");break;

			default: genreText = std::string("Unknown"); break;
		}

		// category
//		BYTE category = (BYTE)((nc->user_nibble_1 << 4) + (BYTE)(nc->user_nibble_2));
		switch(nc->user_nibble_1)
		{
            case 0x00: genreText2 = std::string(""); break;
            case 0x01: genreText2 = std::string(" - Action"); break;
            case 0x02: genreText2 = std::string(" - Adults only"); break;
            case 0x03: genreText2 = std::string(" - Adventure"); break;
            case 0x04: genreText2 = std::string(" - Animals"); break;
            case 0x05: genreText2 = std::string(" - Animated"); break;
            case 0x07: genreText2 = std::string(" - Anthology"); break;
            case 0x08: genreText2 = std::string(" - Art"); break;
            case 0x09: genreText2 = std::string(" - Automobile"); break;
            case 0x0a: genreText2 = std::string(" - Awards"); break;
            case 0x0b: genreText2 = std::string(" - Ballet"); break;
            case 0x0c: genreText2 = std::string(" - Baseball"); break;
            case 0x0d: genreText2 = std::string(" - Basketball"); break;

            case 0x11: genreText2 = std::string(" - Biography"); break;
            case 0x12: genreText2 = std::string(" - Boat"); break;
            case 0x14: genreText2 = std::string(" - Bowling"); break;
            case 0x15: genreText2 = std::string(" - Boxing"); break;
            case 0x16: genreText2 = std::string(" - Business/Financial"); break;
            case 0x1a: genreText2 = std::string(" - Children"); break;
            case 0x1b: genreText2 = std::string(" - Children-Special"); break;
            case 0x1c: genreText2 = std::string(" - Children-News"); break;
            case 0x1d: genreText2 = std::string(" - Children-Music"); break;
            case 0x1f: genreText2 = std::string(" - Collectibles"); break;

            case 0x20: genreText2 = std::string(" - Comedy"); break;
            case 0x21: genreText2 = std::string(" - Comedy-Drama"); break;
            case 0x22: genreText2 = std::string(" - Computers"); break;
            case 0x23: genreText2 = std::string(" - Cooking"); break;
            case 0x24: genreText2 = std::string(" - Crime"); break;
            case 0x25: genreText2 = std::string(" - Crime-Drama"); break;
            case 0x27: genreText2 = std::string(" - Dance"); break;
            case 0x29: genreText2 = std::string(" - Docudrama"); break;
            case 0x2a: genreText2 = std::string(" - Documentary"); break;
            case 0x2b: genreText2 = std::string(" - Drama"); break;
            case 0x2c: genreText2 = std::string(" - Educational"); break;
            case 0x2f: genreText2 = std::string(" - Exercise"); break;

            case 0x31: genreText2 = std::string(" - Fantasy"); break;
            case 0x32: genreText2 = std::string(" - Fashion"); break;
            case 0x34: genreText2 = std::string(" - Fishing"); break;
            case 0x35: genreText2 = std::string(" - Football"); break;
            case 0x36: genreText2 = std::string(" - French"); break;
            case 0x37: genreText2 = std::string(" - Fundraiser"); break;
            case 0x38: genreText2 = std::string(" - Game Show"); break;
            case 0x39: genreText2 = std::string(" - Golf"); break;
            case 0x3a: genreText2 = std::string(" - Gymnastics"); break;
            case 0x3b: genreText2 = std::string(" - Health"); break;
            case 0x3c: genreText2 = std::string(" - History"); break;
            case 0x3d: genreText2 = std::string(" - Historical Drama"); break;
            case 0x3e: genreText2 = std::string(" - Hockey"); break;
            case 0x3f: genreText2 = std::string(" - Holiday"); break;

            case 0x40: genreText2 = std::string(" - Holiday-Children"); break;
            case 0x41: genreText2 = std::string(" - Holiday-Children Special"); break;
            case 0x44: genreText2 = std::string(" - Holiday Special"); break;
            case 0x45: genreText2 = std::string(" - Horror"); break;
            case 0x46: genreText2 = std::string(" - Horse Racing"); break;
            case 0x47: genreText2 = std::string(" - House/Garden"); break;
            case 0x49: genreText2 = std::string(" - How-To"); break;
            case 0x4b: genreText2 = std::string(" - Interview"); break;
            case 0x4d: genreText2 = std::string(" - Lacrosse"); break;
            case 0x4f: genreText2 = std::string(" - Martial Arts"); break;

            case 0x50: genreText2 = std::string(" - Medical"); break;
            case 0x51: genreText2 = std::string(" - Miniseries"); break;
            case 0x52: genreText2 = std::string(" - Motorsports"); break;
            case 0x53: genreText2 = std::string(" - Motorcycle"); break;
            case 0x54: genreText2 = std::string(" - Music"); break;
            case 0x55: genreText2 = std::string(" - Music Special"); break;
            case 0x56: genreText2 = std::string(" - Music Talk"); break;
            case 0x57: genreText2 = std::string(" - Musical"); break;
            case 0x58: genreText2 = std::string(" - Musical Comedy"); break;
            case 0x5a: genreText2 = std::string(" - Mystery"); break;
            case 0x5b: genreText2 = std::string(" - Nature"); break;
            case 0x5c: genreText2 = std::string(" - News"); break;
            case 0x5f: genreText2 = std::string(" - Opera"); break;

            case 0x60: genreText2 = std::string(" - Outdoors"); break;
            case 0x63: genreText2 = std::string(" - Public Affairs"); break;
            case 0x66: genreText2 = std::string(" - Reality"); break;
            case 0x67: genreText2 = std::string(" - Religious"); break;
            case 0x68: genreText2 = std::string(" - Rodeo"); break;
            case 0x69: genreText2 = std::string(" - Romance"); break;
            case 0x6a: genreText2 = std::string(" - Romance-Comedy"); break;
            case 0x6b: genreText2 = std::string(" - Rugby"); break;
            case 0x6c: genreText2 = std::string(" - Running"); break;
            case 0x6e: genreText2 = std::string(" - Science"); break;
            case 0x6f: genreText2 = std::string(" - Science Fiction"); break;

            case 0x70: genreText2 = std::string(" - Self Improvement"); break;
            case 0x71: genreText2 = std::string(" - Shopping"); break;
            case 0x74: genreText2 = std::string(" - Skiing"); break;
            case 0x77: genreText2 = std::string(" - Soap"); break;
            case 0x7b: genreText2 = std::string(" - Soccer"); break;
            case 0x7c: genreText2 = std::string(" - Softball"); break;
            case 0x7d: genreText2 = std::string(" - Spanish"); break;
            case 0x7e: genreText2 = std::string(" - Special"); break;

            case 0x81: genreText2 = std::string(" - Sports Non-Event"); break;
            case 0x82: genreText2 = std::string(" - Sports Talk"); break;
            case 0x83: genreText2 = std::string(" - Suspense"); break;
            case 0x85: genreText2 = std::string(" - Swimming"); break;
            case 0x86: genreText2 = std::string(" - Talk"); break;
            case 0x87: genreText2 = std::string(" - Tennis"); break;
            case 0x89: genreText2 = std::string(" - Track/Field"); break;
            case 0x8a: genreText2 = std::string(" - Travel"); break;
            case 0x8b: genreText2 = std::string(" - Variety"); break;
            case 0x8c: genreText2 = std::string(" - Volleyball"); break;
            case 0x8d: genreText2 = std::string(" - War"); break;
            case 0x8e: genreText2 = std::string(" - Watersports"); break;
            case 0x8f: genreText2 = std::string(" - Weather"); break;

            case 0x90: genreText2 = std::string(" - Western"); break;
            case 0x92: genreText2 = std::string(" - Wrestling"); break;
            case 0x93: genreText2 = std::string(" - Yoga"); break;
            case 0x94: genreText2 = std::string(" - Agriculture"); break;
            case 0x95: genreText2 = std::string(" - Anime"); break;
            case 0x97: genreText2 = std::string(" - Arm Wrestling"); break;
            case 0x98: genreText2 = std::string(" - Arts/Crafts"); break;
            case 0x99: genreText2 = std::string(" - Auction"); break;
            case 0x9a: genreText2 = std::string(" - Auto Racing"); break;
            case 0x9b: genreText2 = std::string(" - Air Racing"); break;
            case 0x9c: genreText2 = std::string(" - Badminton"); break;

            case 0xa0: genreText2 = std::string(" - Bicycle Racing"); break;
            case 0xa1: genreText2 = std::string(" - Boat Racing"); break;
            case 0xa6: genreText2 = std::string(" - Community"); break;
            case 0xa7: genreText2 = std::string(" - Consumer"); break;
            case 0xa8: genreText2 = std::string(" - Cricket"); break;
            case 0xaa: genreText2 = std::string(" - Debate"); break;
            case 0xac: genreText2 = std::string(" - Dog Show"); break;
            case 0xad: genreText2 = std::string(" - Drag Racing"); break;
            case 0xae: genreText2 = std::string(" - Entertainment"); break;
            case 0xaf: genreText2 = std::string(" - Environment"); break;

            case 0xb0: genreText2 = std::string(" - Equestrian"); break;
            case 0xb3: genreText2 = std::string(" - Field Hockey"); break;
            case 0xb5: genreText2 = std::string(" - Football"); break;
            case 0xb6: genreText2 = std::string(" - Gay/Lesbian"); break;
            case 0xb7: genreText2 = std::string(" - Handball"); break;
            case 0xb8: genreText2 = std::string(" - Home Improvement"); break;
            case 0xb9: genreText2 = std::string(" - Hunting"); break;
            case 0xbb: genreText2 = std::string(" - Hydroplane Racing"); break;

            case 0xc1: genreText2 = std::string(" - Law"); break;
            case 0xc3: genreText2 = std::string(" - Motorcycle Racing"); break;
            case 0xc5: genreText2 = std::string(" - News Magazine"); break;
            case 0xc7: genreText2 = std::string(" - Paranormal"); break;
            case 0xc8: genreText2 = std::string(" - Parenting"); break;
            case 0xca: genreText2 = std::string(" - Performing Arts"); break;
            case 0xcc: genreText2 = std::string(" - Politics"); break;
            case 0xcf: genreText2 = std::string(" - Pro wrestling"); break;

            case 0xd3: genreText2 = std::string(" - Sailing"); break;
            case 0xd4: genreText2 = std::string(" - Shooting"); break;
            case 0xd5: genreText2 = std::string(" - Sitcom"); break;
            case 0xd6: genreText2 = std::string(" - Skateboarding"); break;
            case 0xd9: genreText2 = std::string(" - Snowboarding"); break;
            case 0xdd: genreText2 = std::string(" - Standup"); break;
            case 0xdf: genreText2 = std::string(" - Surfing"); break;

            case 0xe0: genreText2 = std::string(" - Tennis"); break;
            case 0xe1: genreText2 = std::string(" - Triathlon"); break;
            case 0xe6: genreText2 = std::string(" - Card Games"); break;
            case 0xe7: genreText2 = std::string(" - Poker"); break;
            case 0xea: genreText2 = std::string(" - Military"); break;
            case 0xeb: genreText2 = std::string(" - Technology"); break;
            case 0xec: genreText2 = std::string(" - Mixed Martial Arts"); break;
            case 0xed: genreText2 = std::string(" - Action Sports"); break;

            case 0xff: genreText2 = std::string(" - Dish Network"); break;

			default:  break;
		}

		// Output content if present
		if(genreText.length() > 0)
		{
			log(3, false, 0, TEXT("\tCategory: %s%s\n"), genreText.c_str(), genreText2.c_str());
		}

		newEvent->dishCategory1 = genreText;
		newEvent->dishCategory2 = genreText2;
	}
}

void EIT::decodeComponentDescriptor(const BYTE* inputBuffer, EITEvent* newEvent)
{
	const descr_component_t* const componentDescriptor = CastComponentDescriptor(inputBuffer);

	// Take care of different kinds of components
	switch(componentDescriptor->stream_content)
	{
		case 1:
		case 5:
			// Video
			log(3, false, 0, TEXT("\tVideo Aspect: 0x%02X\n"), (UINT)componentDescriptor->component_type);
			newEvent->aspect = (int)componentDescriptor->component_type;
			break;
		case 2:
			// Audio
			if(componentDescriptor->component_type == 1 || componentDescriptor->component_type == 2)
				newEvent->stereo = false;
			else
				newEvent->stereo = true;
			log(3, false, 0, TEXT("\tAudio Stereo: 0x%02X\n"), (UINT)componentDescriptor->component_type);
			break;
		case 4:
		case 6:
		case 7:
		case 8:
			// More audio
			newEvent->stereo = true;
			log(3, false, 0, TEXT("\tAudio Stereo: 0x%02X\n"), (UINT)componentDescriptor->component_type);
			break;
		case 3:
			// Subtitles
			log(3, false, 0, TEXT("\tSubtitles Type: %s  Language: %.2s\n"), ((UINT)componentDescriptor->component_type & 0xF0) ? "teletext" : "onscreen", inputBuffer + 5);
			if(componentDescriptor->component_type & 0xF0)
				newEvent->teletext = true;
			else
				newEvent->onscreen = true;

			break;
#ifdef _DEBUG
		default:
			log(3, false, 0, TEXT("??? Unknown type of content %02X\n"), (UINT)componentDescriptor->stream_content);
			break;
#endif
	}
}

// Not used in North America
void EIT::decodeParentalRatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent)
{	
	const parental_rating_t* const parentalRating = CastParentalRating(inputBuffer + DESCR_PARENTAL_RATING_LEN);

	int rating = (unsigned int)parentalRating->rating + 3;

	log(3, false, 0, TEXT("\tParental Rating: %d\n"), rating);

	newEvent->parentalRating = rating;

	DWORD language = (parentalRating->lang_code1 << 16) + (parentalRating->lang_code2 << 8) + parentalRating->lang_code3;
	bool langFound=false;
	EITEvent::ivecLanguages it = newEvent->vecLanguages.begin();
	for (it = newEvent->vecLanguages.begin(); it != newEvent->vecLanguages.end(); ++it)
	{
		EPGLanguage& lang = *it;
		if (lang.language == language)
		{
			lang.parentalRating = rating;
			langFound = true;
			break;
		}
	}
	if (!langFound)
	{
		EPGLanguage lang;
		lang.language = language;
		lang.parentalRating = rating;
		lang.text = "";
		lang.eventText = "";
		newEvent->vecLanguages.push_back(lang);
	}
}

void EIT::decodeCombinedStarRating_MPAARatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent)
{
	std::string starRating;
	std::string mpaaRating;
	std::string contentAdvisory;
	const descr_starmpaa_identifier_struct* const ratingDescriptor = CastStarMpaaDescriptor(inputBuffer);

	int theStarRating=((ratingDescriptor->rating_hi & 0xE0) >> 5); 
	int bPRating=((ratingDescriptor->rating_hi & 0x1C) >> 2);	  
	int theContentAdvisory = ratingDescriptor->rating_lo & 0x1FF;   

	switch(theStarRating)
	{
		case 0x1: starRating = std::string("1.0/4");break;	
		case 0x2: starRating = std::string("1.5/4");break;	
		case 0x3: starRating = std::string("2.0/4");break;	
		case 0x4: starRating = std::string("2.5/4");break;	
		case 0x5: starRating = std::string("3.0/4");break;	
		case 0x6: starRating = std::string("3.5/4");break;	
		case 0x7: starRating = std::string("4.0/4");break;	
		default: starRating = std::string(""); break;
	}

	if(starRating.length() > 0)
	{
		log(3, false, 0, TEXT("\tStar Rating: %s\n"), starRating.c_str());
	}
	newEvent->starRating = starRating;

	switch(bPRating)
	{
		case 0x0: mpaaRating = std::string("NA/NO");break;
		case 0x1: mpaaRating = std::string("G");break;	
		case 0x2: mpaaRating = std::string("PG");break;	
		case 0x3: mpaaRating = std::string("PG-13");break;	
		case 0x4: mpaaRating = std::string("R");break;	
		case 0x5: mpaaRating = std::string("NC-17");break;	
		case 0x6: mpaaRating = std::string("NR");break;	
		case 0x7: mpaaRating = std::string("Unrated");break;	
		default: mpaaRating = std::string(""); break;
	}

	if(mpaaRating.length() > 0)
	{
		log(3, false, 0, TEXT("\tMPAA Rating: %s\n"), mpaaRating.c_str());
	}

	newEvent->mpaaRating = mpaaRating;

	contentAdvisory = std::string("");

	if(theContentAdvisory & 0x01) contentAdvisory.append("Sexual Content,");
	if(theContentAdvisory & 0x02) contentAdvisory.append("Language,");
	if(theContentAdvisory & 0x04) contentAdvisory.append("Mild Sensuality,");
	if(theContentAdvisory & 0x08) contentAdvisory.append("Fantasy Violence,");
	if(theContentAdvisory & 0x10) contentAdvisory.append("Violence,");
	if(theContentAdvisory & 0x20) contentAdvisory.append("Mild Peril,");
	if(theContentAdvisory & 0x40) contentAdvisory.append("Nudity,");

	if(contentAdvisory.length() > 0)
	{
		contentAdvisory[contentAdvisory.length() - 1] = ' ';
		log(3, false, 0, TEXT("\tMPAA Content Advisory: %s\n"), contentAdvisory.c_str());
	}

	newEvent->mpaaAdvisory = theContentAdvisory;
}

void EIT::decodeDishShortDescription(const BYTE* inputBuffer, int tnum, EITEvent* newEvent)
{
	unsigned char *decompressed = CDishDecode::Decompress(&inputBuffer[3], inputBuffer[1]-1, tnum);

	if(decompressed)
	{
		std::string descriptionText((char *)decompressed); 

		// Get rid of annoying carriage returns
		descriptionText = ReplaceAll(descriptionText, "\r", " ");

		if(descriptionText.length() > 0)
			log(3, false, 0, TEXT("\tShort Desc: %s\n"), descriptionText.c_str());

		newEvent->shortDescription = descriptionText;

		free(decompressed);

		EITEvent::ivecLanguages it = newEvent->vecLanguages.begin();
		for (it = newEvent->vecLanguages.begin(); it != newEvent->vecLanguages.end(); ++it)
		{
			EPGLanguage& lang = *it;
			if (lang.language == langENG)
			{
				lang.shortDescription = descriptionText;
				return;
			}
		}
		EPGLanguage lang;
		lang.shortDescription = descriptionText;
		lang.parentalRating = 0;
		lang.language = langENG;
		lang.text = "en";
		newEvent->vecLanguages.push_back(lang);
	}
}

void EIT::decodeDishLongDescription(const BYTE* inputBuffer, int tnum, EITEvent* newEvent)
{
	unsigned char* decompressed=NULL;
	if((inputBuffer[3] & 0xf8) == 0x80)
		decompressed=CDishDecode::Decompress(&inputBuffer[4], inputBuffer[1]-2, tnum);
	else
	    decompressed=CDishDecode::Decompress(&inputBuffer[3], inputBuffer[1]-1, tnum);

	if(decompressed)
	{
		std::string descriptionText((char *)decompressed); 

		// Get rid of annoying carriage returns
		descriptionText = ReplaceAll(descriptionText, "\r", " ");
		
		if(descriptionText.length() > 0)
			log(3, false, 0, TEXT("\tLong Desc: %s\n"), descriptionText.c_str());

		newEvent->longDescription = descriptionText;

		free(decompressed);

		EITEvent::ivecLanguages it = newEvent->vecLanguages.begin();
		for (it = newEvent->vecLanguages.begin(); it != newEvent->vecLanguages.end(); ++it)
		{
			EPGLanguage& lang = *it;
			if (lang.language == langENG)
			{
				lang.longDescription = descriptionText;
				return;
			}
		}
		EPGLanguage lang;
		lang.longDescription = descriptionText;
		lang.parentalRating = 0;
		lang.language = langENG;
		lang.text = "en";
		newEvent->vecLanguages.push_back(lang);
	}
}

std::string EIT::ReplaceAll(
  std::string result, 
  const std::string& replaceWhat, 
  const std::string& replaceWithWhat)
{
  int pos = -1;
  while(1)
  {
    pos = result.find(replaceWhat, pos+1);
    if (pos==-1) break;
    result.replace(pos,replaceWhat.size(),replaceWithWhat);
  }
  return result;
}

void EIT::decodeDishCcStereoDescriptor(const BYTE* inputBuffer, int tnum, EITEvent* newEvent)
{
	unsigned char* decompressed=NULL;
    decompressed=CDishDecode::Decompress(&inputBuffer[4], inputBuffer[1]-2, tnum);

	if(decompressed)
	{
		std::string descriptionText((char *)decompressed); 
		size_t found;
		found = descriptionText.find("|CC");
		if (found == string::npos)
			newEvent->CC = false;
		else
			newEvent->CC = true;

		found = descriptionText.find("|Stereo");
		if (found == string::npos)
			newEvent->stereo = false;
		else
			newEvent->stereo = true;

		log(3, false, 0, TEXT("\tClosed Captioned: %s\n"), (newEvent->CC == true) ? "True" : "False");
		log(3, false, 0, TEXT("\tStereo: %s\n"), (newEvent->stereo == true) ? "True" : "False");
	}
}

void EIT::decodeDishVChipRatingDescriptor(const BYTE* inputBuffer, EITEvent* newEvent)
{
	const descr_vchip_identifier_struct* const vchipDescriptor = CastVChipDescriptor(inputBuffer);

    switch(vchipDescriptor->rating)
    {
        case 0x1: newEvent->vchipRating = "TV-Y"; break;
        case 0x2: newEvent->vchipRating = "TV-Y7"; break;
        case 0x3: newEvent->vchipRating = "TV-G"; break;
        case 0x4: newEvent->vchipRating = "TV-PG"; break;
        case 0x5: newEvent->vchipRating = "TV-14"; break;
        case 0x6: newEvent->vchipRating = "TV-MA"; break;
		default:  newEvent->vchipRating = ""; break;
    }

	if(newEvent->vchipRating.length() > 0)
	{
		log(3, false, 0, TEXT("\tVChip Rating: %s\n"), newEvent->vchipRating.c_str());
	}

	newEvent->vchipAdvisory = vchipDescriptor->advisory;

	std::string theContentAdvisory;
	theContentAdvisory = "";
    if ((vchipDescriptor->advisory & 0x01) != 0) theContentAdvisory += "Fantasy Violence,";
    if ((vchipDescriptor->advisory & 0x02) != 0) theContentAdvisory += "Violence,";
    if ((vchipDescriptor->advisory & 0x04) != 0) theContentAdvisory += "Sexual Situations,";
    if ((vchipDescriptor->advisory & 0x08) != 0) theContentAdvisory += "Crude Language,";
    if ((vchipDescriptor->advisory & 0x10) != 0) theContentAdvisory += "Suggestive Dialog,";

	if(theContentAdvisory.length() > 0)
	{
		theContentAdvisory[theContentAdvisory.length() - 1] = ' ';
		log(3, false, 0, TEXT("\tVChip Content Advisory: %s\n"), theContentAdvisory.c_str());
	}
}

// Field length: minimum 12, maximum 14 (e.g. MV1234560000, SH0123450000)
// Unique description identifier necessary to reference movies, shows, episodes, sports from the programs data. 
// First two digits are alphanumeric and correspond to movies (MV), shows (SH), episodes (EP) and sports (SP). 
// For shows beginning with EP, the next 8 digits represent the series ID, with the last 4 digits representing the episode id. 
// If episode information is not available, the program will appear as type SH, the next 8 digits as the series id and the last 4 digits as zeros.
void EIT::decodeDishSeriesDescriptor(const BYTE* inputBuffer, EITEvent* newEvent)
{
	std::string prefix;
	std::string programID;
	std::string seriesID;

	const descr_series_identifier_struct* const seriesDescriptor = CastSeriesDescriptor(inputBuffer);

	long series = (seriesDescriptor->seriesId_nibble_1 << 0x12) | (seriesDescriptor->seriesId_nibble_2 << 0x0a) |
					(seriesDescriptor->seriesId_nibble_3 << 0x02) | seriesDescriptor->seriesId_nibble_4;

	long episode = (seriesDescriptor->epiosodeNum_hi << 0x08) | seriesDescriptor->epiosodeNum_lo;

	if (seriesDescriptor->seriesInfo == 0x7c)
		 prefix = std::string("MV");
	else if (seriesDescriptor->seriesInfo == 0x7d)
		 prefix = std::string("SP");
	else if (seriesDescriptor->seriesInfo == 0x7e)
		 prefix = std::string("EP");

	char temp[1024];
	sprintf_s(temp, sizeof(temp), "%s%08d%04d", (seriesDescriptor->seriesInfo == 0x7e && episode == 0) ? "SH" : prefix.c_str(), series, episode);
	programID = std::string(temp);
	newEvent->programID = programID;

	log(3, false, 0, TEXT("\tProgram ID BEFORE: %s\n"), programID.c_str());

	if(seriesDescriptor->seriesInfo == 0x7e)
		sprintf_s(temp, sizeof(temp), "%s%08d0000", prefix.c_str(), series);
	else
		sprintf_s(temp, sizeof(temp), "");

	seriesID = std::string(temp);
	newEvent->seriesID = seriesID;


	int odate = seriesDescriptor->originalAirDate_hi << 0x08 | seriesDescriptor->originalAirDate_lo;
	if(odate != 0x9e8b && odate != 0)
	{
		int year = int((odate - 15078.2) / 365.25) + 1900;
		newEvent->year = year;
		//int month = int((odate - 14956.1 - int(year * 365.25)) / 30.6001);
		//int day = odate - 14956 - int(year * 365.25) - int(month * 30.6001);
		//int offset = 0;
		//if ((month == 14) || (month == 15))    
		//		offset = 1;

		//year = year + offset + 1900;
		//month = month - 1 - (offset * 12);

		//tm dvb_time;
		//dvb_time.tm_mday = day;
		//dvb_time.tm_mon = month;
		//dvb_time.tm_year = year;
		//dvb_time.tm_isdst = 0;
		//dvb_time.tm_wday = dvb_time.tm_yday = 0;
		//dvb_time.tm_sec =  0;
		//dvb_time.tm_min =  0;
		//dvb_time.tm_hour = 0;
		//newEvent->originalAirDate = _mkgmtime(&dvb_time);
	}		
	else
		newEvent->year = 0;

	log(3, false, 0, TEXT("\tSeries ID: %s\n"), newEvent->seriesID.c_str());
	log(3, false, 0, TEXT("\tOriginal Airing Year: %d\n"), newEvent->year);
}

struct eitRecord EIT::GetEitRecord(int onid)
{
	struct eitRecord ret;

	ret.ONID = -1;

	bool found = false;
	for (vector<eitRecord>::iterator iter = m_eitRecords.begin(); iter != m_eitRecords.end() && !found; iter++)
	{
		if(iter->ONID == onid)
		{
			found = true;
			ret.ONID = iter->ONID;
			ret.lineup = iter->lineup;
			ret.chan = iter->chan;
//			ret.useLongChennelName = iter->useLongChennelName;

			for(hash_set<USHORT>::const_iterator it = iter->includedSIDs.begin(); it != iter->includedSIDs.end();)
				ret.includedSIDs.insert(*it);
		}
	}

	return ret;	
}


EPGLanguage EIT::GetDescriptionRecord(struct EITEvent rec)
{
	EPGLanguage ret;

	ret.language = 0;

	bool found = false;
	for (vector<EPGLanguage>::iterator iter = rec.vecLanguages.begin(); iter != rec.vecLanguages.end() && !found; iter++)
	{
		// Look for English first
		if(iter->language == langENG)
		{
			found = true;
			ret.language = iter->language;
			ret.eventText = iter->eventText;
			ret.text = iter->text;
			ret.shortDescription = iter->shortDescription;
			ret.longDescription = iter->longDescription;
			ret.parentalRating = iter->parentalRating;
			ret.CR_added = iter->CR_added;
		}
	}

	// If English is not found, take the first language
	if(found == false)
	{
		ret.language = rec.vecLanguages[0].language;
		ret.eventText = rec.vecLanguages[0].eventText;
		ret.text = rec.vecLanguages[0].text;
		ret.shortDescription = rec.vecLanguages[0].shortDescription;
		ret.longDescription = rec.vecLanguages[0].longDescription;
		ret.parentalRating = rec.vecLanguages[0].parentalRating;
		ret.CR_added = rec.vecLanguages[0].CR_added;
	}

	return ret;	
}

#include "StdAfx.h"
#include "NetworkProvider.h"
#include "extern.h"
#include "Logger.h"
#include "SatelliteInfo.h"

bool NetworkProvider::getServiceName(UINT32 usid,
									 LPTSTR output,
									 int outputLength) const
{
	hash_map<UINT32, Service>::const_iterator it = m_Services.find(usid);
	if(it != m_Services.end())
	{
		hash_map<string, string>::const_iterator it1 = it->second.serviceNames.find(string("eng"));
		if(it1 != it->second.serviceNames.end())
		{
			CA2T name(it1->second.c_str());
			_tcscpy_s(output, outputLength, name);
			return true;
		}
		it1 = it->second.serviceNames.begin();
		if(it1 != it->second.serviceNames.end())
		{
			CA2T name(it1->second.c_str());
			_tcscpy_s(output, outputLength, name);
			return true;
		}
		else
			return false;
	}
	else
		return false;
}

bool NetworkProvider::getSidForChannel(USHORT channel,
									   UINT32& usid) const
{
	hash_map<USHORT, UINT32>::const_iterator it = m_Channels.find(channel);
	if(it != m_Channels.end())
	{
		usid = it->second;
		return true;
	}
	else
		return false;
}

bool NetworkProvider::getTransponderForSid(UINT32 usid,
										   Transponder& transponder) const
{
	hash_map<UINT32, Service>::const_iterator it1 = m_Services.find(usid);
	if(it1 != m_Services.end())
	{
		hash_map<UINT32, Transponder>::const_iterator it2 = m_Transponders.find(getUniqueSID(it1->second.onid, it1->second.tid));
		if(it2 != m_Transponders.end())
		{
			transponder = it2->second;
			return true;
		}
		else
			return false;
	}
	else
		return false;
}

// Assumption for this is that SID is unique across all networks
bool NetworkProvider::getOnidForSid(USHORT sid,
									USHORT& onid) const
{
	bool found = false;
	for(hash_map<UINT32, Service>::const_iterator it = m_Services.begin(); it != m_Services.end() && !found; it++)
	{
		if(it->second.sid == sid)
		{
			found = true;
			onid = it->second.onid;
		}
	}

	return found;
}

// Dump the services table to a CSV file suitable for import by Excel or OpenOffice
bool NetworkProvider::dumpServices(LPCTSTR fileName, LPTSTR reason) const
{
	FILE* outFile = NULL;
	TCHAR channelName[256];
	_tfopen_s(&outFile, fileName, TEXT("wt"));
	if(outFile != NULL)
	{
		_ftprintf(outFile, TEXT("\"Name\",\"SID\",\"Network\",\"TID\"\n"));
		for(hash_map<UINT32, Service>::const_iterator it = m_Services.begin(); it != m_Services.end(); it++)
		{
			hash_map<string, string>::const_iterator it1 = it->second.serviceNames.find(string("eng"));
			if(it1 != it->second.serviceNames.end())
			{
				CA2T name(it1->second.c_str());
				_tcscpy_s(channelName, sizeof(channelName), name);
			}
			else
			{
				it1 = it->second.serviceNames.begin();
				if(it1 != it->second.serviceNames.end())
				{
					CA2T name(it1->second.c_str());
					_tcscpy_s(channelName, sizeof(channelName), name);
				}
				else
					_tcscpy_s(channelName, sizeof(channelName), "Unknown");
			}

			_ftprintf(outFile, TEXT("\"%s\",%hu,%hu,%d\n"), channelName, it->second.sid, it->second.onid, it->second.tid);
		}

		fclose(outFile);
		return true;
	}
	else
	{
		// We get here if for some reason we're unable to open the file
		stringstream result;
		result << "Cannot open the output file \"" << fileName << "\", error code = " << errno;
		_tcscpy_s(reason, MAX_ERROR_MESSAGE_SIZE, CA2CT(result.str().c_str()));
		return false;
	}
}

// Dump the transponder table to a CSV file suitable for import by Excel or OpenOffice
bool NetworkProvider::dumpTransponders(LPCTSTR fileName, LPTSTR reason) const
{
	FILE* outFile = NULL;
	_tfopen_s(&outFile, fileName, TEXT("wt"));
	if(outFile != NULL)
	{
		_ftprintf(outFile, TEXT("\"Network ID\",\"TID\",\"Frequency\",\"Symbol Rate\",\"Polarization\",\"Modulation\",\"FEC Rate\"\n"));

		for(hash_map<UINT32, Transponder>::const_iterator it = m_Transponders.begin(); it != m_Transponders.end(); it++)
			_ftprintf(outFile, TEXT("%hu,%hu,%lu,%lu,\"%s\",\"%s\",\"%s\"\n"),  it->second.onid, it->second.tid, it->second.frequency, it->second.symbolRate, printablePolarization(it->second.polarization), printableModulation(it->second.modulation), printableFEC(it->second.fec));

		fclose(outFile);
		return true;
	}
	else
	{
		// We get here if for some reason we're unable to open the file
		stringstream result;
		result << "Cannot open the output file \"" << fileName << "\", error code = " << errno;
		_tcscpy_s(reason, MAX_ERROR_MESSAGE_SIZE, CA2CT(result.str().c_str()));
		return false;
	}
}

// Locate all EPG and EEPG services and log their information
bool NetworkProvider::logEPG(LPTSTR reason) const
{
	log(0, false, 0, TEXT("=========================================================\n"));
	log(0, true, 0, TEXT("EPG Service dump:\n\n"));
	for(hash_map<UINT32, Service>::const_iterator it = m_Services.begin(); it != m_Services.end(); it++)
	{
		hash_map<string, string>::const_iterator it1 = it->second.serviceNames.find(string("eng"));
		if(it1 != it->second.serviceNames.end())
		{
			CA2T name(it1->second.c_str());
			string sName = name;

//			log(0, false, 0, TEXT("(1) service=%s ONID=%hu with TID=%d, SID=%hu\n"), sName.c_str(), it->second.onid, it->second.tid, it->second.sid);

			if(_tcsicmp(name, "DvEPG") == 0)
				logEPGEntry(it->second.sid, it->second.onid, it->second.tid, sName);
			else
			if(_tcsicmp(name, "EPG2") == 0)
				logEPGEntry(it->second.sid, it->second.onid, it->second.tid, sName);
			else
			if(_tcsicmp(name, "EEPG") == 0)
				logEPGEntry(it->second.sid, it->second.onid, it->second.tid, sName);
			else
			if(_tcsicmp(name, "EPG") == 0)
				logEPGEntry(it->second.sid, it->second.onid, it->second.tid, sName);
		}
	}

	log(0, false, 0, TEXT("End of EPG Service dump\n"));
	log(0, false, 0, TEXT("=========================================================\n"));

	return true;
}

void NetworkProvider::logEPGEntry(USHORT sid, USHORT onid, USHORT tid, string serviceName) const
{
//	log(0, false, 0, TEXT("(2) service=%s ONID=%hu with TID=%d, SID=%hu\n"), serviceName.c_str(), onid, tid, sid);

	hash_map<UINT32, Transponder>::const_iterator it3 = m_Transponders.find(getUniqueSID(onid, tid));
	if(it3 != m_Transponders.end())
	{
		Transponder transponder = it3->second;
		string satName = g_pSatelliteInfo->getSatelliteName(onid);

		if(satName.length() == 0)
		{
			log(0, false, 0, TEXT("%s service found on ONID=%hu with TID=%d, SID=%hu, Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s\n"), serviceName.c_str(), onid, tid, sid, transponder.frequency, transponder.symbolRate, 
				printablePolarization(transponder.polarization), printableModulation(transponder.modulation), printableFEC(transponder.fec));
		}
		else
		{
			log(0, false, 0, TEXT("%s service found on ONID=%hu (%s) with TID=%d, SID=%hu, Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s\n"), serviceName.c_str(), onid, satName.c_str(), tid, sid, transponder.frequency, transponder.symbolRate, 
				printablePolarization(transponder.polarization), printableModulation(transponder.modulation), printableFEC(transponder.fec));
		}
	}
}

void NetworkProvider::clear()
{
	m_Transponders.clear();
	m_Services.clear();
	m_Channels.clear();
	
	m_DefaultONID = 0;
}

bool NetworkProvider::dumpNetworkProvider(LPTSTR reason) const
{
	ofstream t("transponders.cache");
	ofstream sat("satelliteInfo.cache");
	ofstream s("services.cache");
	
	int counter = 0;
	// We start generating the transponders.cache
	for(hash_map<UINT32, Transponder>::const_iterator it = m_Transponders.begin(); it != m_Transponders.end(); it++) 
	{
		t << it->second.onid << "," << it->second.tid << "," << it->second.frequency << "," << it->second.symbolRate << "," << printablePolarization(it->second.polarization) 
		  << "," << printableModulation(it->second.modulation) << "," << printableFEC(it->second.fec) << "\n";
		counter++;
	}

	t.close();
	log(1,true, 0, TEXT("%d transponders saved to transponders.cache\n"),counter);

	// We continue with the satelliteInfo.cache
	counter = 0;
	for(hash_map<USHORT, SatelliteInfoData>::const_iterator it = g_pSatelliteInfo->m_SatelliteInfo.begin(); it != g_pSatelliteInfo->m_SatelliteInfo.end(); it++) 
	{
		sat << it->second.satelliteName << "," << it->second.onid << "," << it->second.orbitalLocation << "," << it->second.east << "\n";
		counter++;
	}

	sat.close();
	log(1,true, 0, TEXT("%d satellites info saved to satelliteInfo.cache\n"),counter);

	// We end with the services.cache
	TCHAR channelName[256];
	counter=0;
	for(hash_map<UINT32, Service>::const_iterator it = m_Services.begin(); it != m_Services.end(); it++)
	{
		hash_map<string, string>::const_iterator it1 = it->second.serviceNames.find(string("eng"));
		if(it1 != it->second.serviceNames.end())
		{
			CA2T name(it1->second.c_str());
			_tcscpy_s(channelName, sizeof(channelName), name);
		}
		else
		{
			it1 = it->second.serviceNames.begin();
			if(it1 != it->second.serviceNames.end())
			{
				CA2T name(it1->second.c_str());
				_tcscpy_s(channelName, sizeof(channelName), name);
			}
			else
				_tcscpy_s(channelName, sizeof(channelName), "Unknown");
		}
		s << it->second.sid << "," << it->second.onid << "," << it->second.tid << "," << channelName << "," << (USHORT)it->second.serviceType << "," << (USHORT)it->second.runningStatus << "\n";
		counter++;
	}

	s.close();
	log(1,true, 0, TEXT("%d services saved to services.cache\n"),counter);
		 
	log(1,true, 0, TEXT("Transponders and Services dumped to cache file\n"));
	return true;
}

void NetworkProvider::copy(const NetworkProvider& other)
{
	// Copy transponders
	for(hash_map<UINT32, Transponder>::const_iterator it = other.m_Transponders.begin(); it != other.m_Transponders.end(); it++)
		m_Transponders[it->first] = it->second;

	// Copy services
	for(hash_map<UINT32, Service>::const_iterator it = other.m_Services.begin(); it != other.m_Services.end(); it++)
		m_Services[it->first] = it->second;

	// Copy channels
	for(hash_map<USHORT, UINT32>::const_iterator it = other.m_Channels.begin(); it != other.m_Channels.end(); it++)
		m_Channels[it->first] = it->second;

	if(m_DefaultONID == 0)
		m_DefaultONID = other.m_DefaultONID;
}



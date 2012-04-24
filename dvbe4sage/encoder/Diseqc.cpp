#include "StdAfx.h"
#include "Diseqc.h"
#include "usals.h"
#include "IniFile.h"
#include "Logger.h"
#include "graph.h"
#include "GenpixCustomKsPropertySet.h"
#include "ProfExtensions.h"


DiSEqC::DiSEqC(void) :
m_latitude(0.0),
m_longitude(0.0),
m_diseqcRepeats(0),
m_MovementTimeSec(15),
m_currentOrbitalLocation(-1.0),
m_CurrentPosition(-1)
{
	ParseIniFile();
}

DiSEqC::~DiSEqC(void)
{
	m_rawDiseqcCommands.clear();
	m_diseqcRecords.clear();
}

void DiSEqC::ParseIniFile(void)
{
	vector<string> sections;

	TCHAR iniFileFullPath[MAXSHORT];
	DWORD iniFileFullPathLen = GetCurrentDirectory(sizeof(iniFileFullPath) / sizeof(iniFileFullPath[0]), iniFileFullPath);
	_tcscpy_s(&iniFileFullPath[iniFileFullPathLen], sizeof(iniFileFullPath) / sizeof(iniFileFullPath[0]) - iniFileFullPathLen, "\\diseqc.ini");
	string fileName = iniFileFullPath;

	sections = CIniFile::GetSectionNames(fileName);

	// Log info
	log(0, true, 0, TEXT("Loading DiSEqC info from \"%s\"\n"), iniFileFullPath);
	log(0, false, 0, TEXT("=========================================================\n"));
	log(0, false, 0, TEXT("DiSEqC file dump:\n\n"));

	for(int i = 0; i < (int)sections.size(); i++)
	{
		if(sections[i].compare("General") == 0)
		{
			m_initialONID.clear();
			string strbuffer = CIniFile::GetValue("InitialONID", sections[i], fileName);
			TCHAR buffer[1024];
			LPTSTR context;
			//std::copy(strbuffer.begin(), strbuffer.end(), buffer);
#pragma warning(disable:4996)
			size_t length = strbuffer.copy(buffer, strbuffer.length(), 0);
#pragma warning(default:4996)
			buffer[length] = '\0';
			if(buffer[0] != TCHAR(0))
			{
				int idx = 0;
				for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
				{
					USHORT val;
					_stscanf_s(token, TEXT("%hu"), &val);
					m_initialONID[idx++] = val;
				}
			}
			
			log(0, false, 0, TEXT("[General]\n"));
			log(0, false, 0, TEXT("InitialONID = "));
			for(hash_map<int, USHORT>::iterator it = m_initialONID.begin(); it != m_initialONID.end();)
			{
				log(0, false, 0, TEXT("%hu"), it->second);
				if(++it != m_initialONID.end())
					log(0, false, 0, TEXT(","));
			}			
			log(0, false, 0, TEXT("\n"));

			m_diseqcRepeats = atoi(CIniFile::GetValue("Repeats", sections[i], fileName).c_str());
			log(0, false, 0, TEXT("Repeats = %hu\n"), m_diseqcRepeats);

			m_MovementTimeSec = atoi(CIniFile::GetValue("MovementTimeSec", sections[i], fileName).c_str());
			log(0, false, 0, TEXT("MovementTimeSec = %hu\n"), m_MovementTimeSec);
			log(0, false, 0, TEXT("\n"));
		}
		else
		if(sections[i].compare("USALS") == 0)
		{
			m_latitude = (float)atof(CIniFile::GetValue("Latitude", sections[i], fileName).c_str());
			m_longitude = (float)atof(CIniFile::GetValue("Longitude", sections[i], fileName).c_str());

			log(0, false, 0, TEXT("[USALS]\n"));
			log(0, false, 0, TEXT("Latitude = %.2f\n"), m_latitude);
			log(0, false, 0, TEXT("Longitude = %.2f\n"), m_longitude);
			log(0, false, 0, TEXT("\n"));
		}
		else
		if(sections[i].compare("Raw") != 0)
		{
			struct diseqcRecord newrec;
			newrec.ONID = atoi(sections[i].c_str());
			newrec.name = CIniFile::GetValue("name", sections[i], fileName);			
			newrec.satpos = atoi(CIniFile::GetValue("satpos", sections[i], fileName).c_str());
			newrec.diseqc1 = atoi(CIniFile::GetValue("diseqc1", sections[i], fileName).c_str());
			newrec.diseqc2 = atoi(CIniFile::GetValue("diseqc2", sections[i], fileName).c_str());
			newrec.is22kHz = atoi(CIniFile::GetValue("22KHz", sections[i], fileName).c_str());
			newrec.gotox = atoi(CIniFile::GetValue("gotox", sections[i], fileName).c_str());
			newrec.sw = atol(CIniFile::GetValue("LNB_SW", sections[i], fileName).c_str());
			newrec.lof1 = atol(CIniFile::GetValue("LNB_LOF1", sections[i], fileName).c_str());
			newrec.lof2 = atol(CIniFile::GetValue("LNB_LOF2", sections[i], fileName).c_str());
			newrec.raw = CIniFile::GetValue("raw", sections[i], fileName);

			log(0, false, 0, TEXT("[%d]\n"), newrec.ONID);
			log(0, false, 0, TEXT("Name = %s\n"), newrec.name.c_str());
			log(0, false, 0, TEXT("Satpos = %d\n"), newrec.satpos);
			log(0, false, 0, TEXT("Diseqc 1 = %d\n"), newrec.diseqc1);
			log(0, false, 0, TEXT("Diseqc 2 = %d\n"), newrec.diseqc2);
			log(0, false, 0, TEXT("22kHz = %s\n"), (newrec.is22kHz == 1) ? "On" : "Off");
			log(0, false, 0, TEXT("gotox = %d\n"), newrec.gotox);
			log(0, false, 0, TEXT("LNB_SW = %lu\n"), newrec.sw);
			log(0, false, 0, TEXT("LNB_LOF1 = %lu\n"), newrec.lof1);
			log(0, false, 0, TEXT("LNB_LOF2 = %lu\n"), newrec.lof2);
			log(0, false, 0, TEXT("Raw = %s\n"), newrec.raw.c_str());
			log(0, false, 0, TEXT("\n"));

			m_diseqcRecords.push_back(newrec);
		}
	}

	// Read and validate any specified raw diseqc commands
	log(0, false, 0, TEXT("[Raw]\n"));
	for (vector<diseqcRecord>::iterator iter = m_diseqcRecords.begin(); iter != m_diseqcRecords.end(); iter++)
	{
		if(!(*iter).raw.empty())
		{
			struct rawDiseqcCommand newCommand;
			newCommand.name = (*iter).raw;
			newCommand.command = CIniFile::GetValue((*iter).raw.c_str(), "Raw", fileName);

			if(!(*iter).raw.empty())
			{
				if(!RawRecordExists(newCommand.name))
				{
					m_rawDiseqcCommands.push_back(newCommand);
					log(0, false, 0, TEXT("%s ="), (*iter).raw.c_str());
					log(0, false, 0, TEXT(" %s\n"), newCommand.command.c_str());
				}
			}
			else
			{
				log(0, false, 0, TEXT("!!! Not Found !!!\n"));
			}
		}
	}
	
	log(0, false, 0, TEXT("\n"));

	log(0, false, 0, TEXT("End of DiSEqC file dump\n"));
	log(0, false, 0, TEXT("=========================================================\n\n"));
}

bool DiSEqC::RawRecordExists(string name)
{
	bool found = false;

	for (vector<rawDiseqcCommand>::iterator iter = m_rawDiseqcCommands.begin(); iter != m_rawDiseqcCommands.end() && !found; iter++)
	{
		if((*iter).name == name)
			found = true;
	}

	return found;
}

USHORT DiSEqC::GetInitialONID(int onidIndex)
{
	hash_map<int, USHORT>::iterator it = m_initialONID.find(onidIndex); 
	if(it != m_initialONID.end())
		return it->second;
	else 
		return 0;
}

int DiSEqC::GetInitialONIDCount()
{
	return m_initialONID.size();
}

// Send motor USALS command
bool DiSEqC::SendUsalsCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, double orbitalLocation)
{
	BYTE command[6];
	ZeroMemory(&command, sizeof(command));

	int length = CUSALS::GetUsalsDiseqcCommand(orbitalLocation/10.00, command, m_latitude, m_longitude);

	bool retval = SendRawDiseqcCommandToDriver(ksTunerPropSet, pFilterGraph, pTunerDemodDevice, 1, length, command);

	if(retval == true && m_currentOrbitalLocation != orbitalLocation)
	{
		m_currentOrbitalLocation = orbitalLocation;
		Sleep(m_MovementTimeSec * 1000);
	}

	return retval;		
}

// Send motor goto stored position command
bool DiSEqC::SendPositionCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, int position)
{
    BYTE command[4];
    ZeroMemory(&command, sizeof(command));
    command[0] = 0xE0;   
    command[1] = 0x31;  
	command[2] = 0x6b;  
	command[3] = (BYTE)position;  

	bool retval = SendRawDiseqcCommandToDriver(ksTunerPropSet, pFilterGraph, pTunerDemodDevice, 1, sizeof(command), command);

	if(retval == true && m_CurrentPosition != position)
	{
		m_CurrentPosition = position;
		Sleep(m_MovementTimeSec * 1000);
	}

	return retval;	
}

bool DiSEqC::SendDiseqcCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG requestID, DISEQC_PORT diseqcPort, Polarisation polarity, bool Is22kHzOn)
{
    BYTE command[4];
    ZeroMemory(&command, sizeof(command));
    command[0] = 0xE0;   
    command[1] = 0x10;   
   
   if (diseqcPort >= DISEQC_UNCOMMITTED_1 && diseqcPort <= DISEQC_UNCOMMITTED_16)
   {
		command[2] = 0x39;   // uncommitted
		command[3] = (BYTE)(0xF0 + (diseqcPort - DISEQC_UNCOMMITTED_1));
   }
   else
   {
		command[2] = 0x38;   // committed
		command[3] = 0xF0;

		if (Is22kHzOn)
			 command[3] += 0x01;

		if (polarity == BDA_POLARISATION_LINEAR_H || polarity == BDA_POLARISATION_CIRCULAR_L)
			 command[3] += 0x02;

		command[3] += (BYTE)(0x04 * (diseqcPort - DISEQC_PORT_1));
   }

	return SendRawDiseqcCommandToDriver(ksTunerPropSet, pFilterGraph, pTunerDemodDevice, requestID, 4, command);
}

// Because of this, the windows SDK needs to be >= version 7
bool DiSEqC::SendRawDiseqcCommandToDriver(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter*pTunerDemodDevice, ULONG requestID, ULONG commandLength, BYTE *command)
{
	ULONG nNodesTypeNum = 0;
	ULONG NodesType[10]; 

	log(3, true, 0, TEXT("Attempting to send DiSEqC command (requestID = %u): "), requestID);
	for(unsigned int i = 0; i < commandLength; i++)
		log(3, false, 0, TEXT("%02x "), *(command+i));
	log(3, false, 0, TEXT("\n"));

	CComQIPtr<IBDA_Topology> pBDATopology(pTunerDemodDevice);

	// Locate the IBDA_DiseqCommand interface (it could be on the tuner or demod nodes)
	pBDATopology->GetNodeTypes(&nNodesTypeNum, 10, NodesType); 

	for(unsigned int i = 0; i < nNodesTypeNum; i++)
	{
		CComPtr <IUnknown> pControlNode;
		pBDATopology->GetControlNode(0, 1, NodesType[i], &pControlNode);

		CComQIPtr<IBDA_DiseqCommand> pBDADiseqCommand(pControlNode);
		if (pBDADiseqCommand)
		{
			pBDADiseqCommand->put_EnableDiseqCommands(TRUE);
			pBDADiseqCommand->put_DiseqUseToneBurst(TRUE);
			pBDADiseqCommand->put_DiseqRepeats(m_diseqcRepeats);
			if (FAILED(pBDADiseqCommand->put_DiseqSendCommand(requestID, commandLength, command)))
			{
				log(3, false, 0, TEXT("Diseqc send failed.\n"));
				return false;
			}

			log(3, false, 0, TEXT("Diseqc send succeeded.\n"));
			return true;
		}
	}

	log(3, true, 0, TEXT("\nNo IBDA_DiseqCommand interface could be found (running Windows XP?) trying old way.\n"));

	if(((DVBFilterGraph*)pFilterGraph)->m_IsGenpix)
		return SendCustomGenpixCommand(ksTunerPropSet, pFilterGraph, pTunerDemodDevice, commandLength, command);

	if(((DVBFilterGraph*)pFilterGraph)->m_IsProf7500)
		return SendCustomProf7500Command(ksTunerPropSet, pFilterGraph, pTunerDemodDevice, commandLength, command);

	if(((DVBFilterGraph*)pFilterGraph)->m_IsTTBDG2 || ((DVBFilterGraph*)pFilterGraph)->m_IsTTUSB2)
		return SendCustomTechnotrendCommand(ksTunerPropSet, pFilterGraph, pTunerDemodDevice, commandLength, command);

	log(3, true, 0, TEXT("Could not send DiSEqC command old way either.\n"));
	return false;
}

bool DiSEqC::SendCustomGenpixCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG commandLength, BYTE *command)
{
	log(3, true, 0, TEXT("Genpix device.  Attempting custom DiSEqC command.\n"));

	HRESULT hr;
	TUNER_COMMAND diseqcCommand;
	DWORD type_support = 0;

	ZeroMemory(&diseqcCommand,  sizeof(TUNER_COMMAND));
	CopyMemory(diseqcCommand.DiSEqC_Command, command, commandLength);
	diseqcCommand.DiSEqC_Length = commandLength;
	diseqcCommand.ForceHighVoltage = TRUE;
	diseqcCommand.DiSEqC_Repeats = m_diseqcRepeats;

	// See if a property set is supported
	hr = ksTunerPropSet->QuerySupported(KSPROPERTYSET_TunerControl,
										KSPROPERTY_SET_DISEQC, 
										&type_support);
	if (FAILED(hr))
	{
		log(3, true, 0, TEXT("Genpix QuerySupported failed. Error 0x%.08X"), hr);
		return false;
	}

	// Make call into driver
	hr = ksTunerPropSet->Set(KSPROPERTYSET_TunerControl, 
								KSPROPERTY_SET_DISEQC, 
								NULL, NULL,
								&diseqcCommand, sizeof(TUNER_COMMAND));

	if (FAILED(hr))
	{
		log(3, true, 0, TEXT("Genpix DiSEqC Get failed. Error 0x%.08X\n"), hr);
		return FALSE;
	}

	return true;
}

bool DiSEqC::SendCustomProf7500Command(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG commandLength, BYTE *command)
{
	log(3, true, 0, TEXT("Prof 7500 device.  Attempting custom DiSEqC command.\n"));

	HRESULT hr;
	DISEQC_MESSAGE_PARAMS DiseqcMessageParams;
	DWORD type_support = 0;

	ZeroMemory(&DiseqcMessageParams,  sizeof(DiseqcMessageParams));
	CopyMemory(DiseqcMessageParams.uc_diseqc_send_message, command, commandLength);
	DiseqcMessageParams.uc_diseqc_send_message_length = (UCHAR)commandLength;

	DiseqcMessageParams.burst_tone = PHANTOM_LNB_BURST_MODULATED;
	DiseqcMessageParams.tbscmd_mode = TBSDVBSCMD_MOTOR;

	// See if a property set is supported
	hr = ksTunerPropSet->QuerySupported(KSPROPSETID_BdaTunerExtensionProperties,
					   KSPROPERTY_BDA_DISEQC_MESSAGE, 
					   &type_support);
	if (FAILED(hr))
	{
		log(3, true, 0, TEXT("Prof 7500 QuerySupported failed. Error 0x%.08X"), hr);
		return false;
	}

	// Make call into driver
	hr = ksTunerPropSet->Set(KSPROPSETID_BdaTunerExtensionProperties, 
							 KSPROPERTY_BDA_DISEQC_MESSAGE, 
							 NULL, NULL,
							 &DiseqcMessageParams, sizeof(DISEQC_MESSAGE_PARAMS));

	if (FAILED(hr))
	{
		log(3, true, 0, TEXT("Prof 7500 DiSEqC Get failed. Error 0x%.08X"), hr);
		return false;
	}

	return true;
}

bool DiSEqC::SendCustomTechnotrendCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG commandLength, BYTE *command)
{
	log(3, true, 0, TEXT("%s device.  Attempting custom DiSEqC command.\n"), ((DVBFilterGraph*)pFilterGraph)->m_IsTTBDG2 ? TEXT("TechnoTrend Budget") : TEXT("TechnoTrend USB 2.0"));

	// Determine whether it's budget or USB 2.0
	DEVICE_CAT deviceCategory = ((DVBFilterGraph*)pFilterGraph)->m_IsTTBDG2 ? BUDGET_2 : USB_2;

	// Get device handle from TT BDA API
	HANDLE hTT = bdaapiOpen(deviceCategory, ((DVBFilterGraph*)pFilterGraph)->m_TTDevID);

	// If the handle is not bogus
	if(hTT != INVALID_HANDLE_VALUE)
	{
		int retval = bdaapiSetDiSEqCMsg(hTT,
										command,
										(BYTE)commandLength,
										(BYTE)m_diseqcRepeats,
										0,
										BDA_POLARISATION_LINEAR_H);

		// Close the handle
		bdaapiClose(hTT);

		// Test for failure
		if(retval)
		{
			log(3, true, 0, TEXT("Technotrend bdaapiSetDiSEqCMsg failed."));
			return false;
		}

		return true;
	}
	else
	{
		log(3, true, 0, TEXT("Technotrend bdaapiOpen failed."));
		return false;
	}
}

struct diseqcRecord DiSEqC::GetDiseqcRecord(int onid)
{
	struct diseqcRecord ret;

	ret.ONID = -1;

	bool found = false;
	for (vector<diseqcRecord>::iterator iter = m_diseqcRecords.begin(); iter != m_diseqcRecords.end() && !found; iter++)
	{
		if((*iter).ONID == onid)
		{
			found = true;
			ret.ONID = (*iter).ONID;
			ret.name = (*iter).name;
			ret.diseqc1 = (*iter).diseqc1;
			ret.diseqc2 = (*iter).diseqc2;
			ret.satpos = (*iter).satpos;
			ret.gotox = (*iter).gotox;
			ret.is22kHz = (*iter).is22kHz;
			ret.sw = (*iter).sw;
			ret.lof1 = (*iter).lof1;
			ret.lof2 = (*iter).lof2;
			ret.raw = (*iter).raw;
		}
	}

	return ret;	
}

bool DiSEqC::SendRawDiseqcCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG requestID, int onid)
{
	bool found = false;
	BYTE command[7];
	int commandLength = 0;
	LPTSTR context;
	TCHAR buffer[1024];

	struct diseqcRecord dRec = GetDiseqcRecord(onid);

	if(dRec.ONID == -1 || dRec.raw.empty())
		return false;

	for (vector<rawDiseqcCommand>::iterator iter = m_rawDiseqcCommands.begin(); iter != m_rawDiseqcCommands.end() && !found; iter++)
	{
		if((*iter).name == dRec.raw)
		{
			found = true;

			// Create the byte aray from the command string	
			std::copy((*iter).command.begin(),(*iter).command.end(), buffer);
			for(LPCTSTR token = _tcstok_s(buffer, TEXT(" "), &context); token != NULL; token = _tcstok_s(NULL, TEXT(" "), &context))
			{
				USHORT cmd;
				_stscanf_s(token, TEXT("%hx"), &cmd);
				command[commandLength++] = (BYTE)cmd;
			}
		}
	}

	if(!found)
		return false;
	
	return SendRawDiseqcCommandToDriver(ksTunerPropSet, pFilterGraph, pTunerDemodDevice, requestID, commandLength, command);
}

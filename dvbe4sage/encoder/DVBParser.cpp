#include "StdAfx.h"

#include "DVBParser.h"
#include "crc32.h"
#include "Logger.h"
#include "Recorder.h"
#include "configuration.h"
#include "extern.h"
#include "GenericSource.h"
#include "SatelliteInfo.h"

// Define constants
#define	CRC_LENGTH			4
#define TABLE_PREFIX_LEN	3

// Our message for communications
#define WM_ALL_COMMUNICATIONS	WM_USER + 1

#define SKYNZ_ONID(onid)		(onid == 169)
#define FOXTEL_ONID(onid)		(onid == 4096)
#define SKYITALIA_ONID(onid)	(onid == 64511)
#define DISH_ONID(onid)			(onid >= 4097 && onid <= 4106)

// Start dumping the full transponder
void DVBParser::startTransponderDump()
{
	lock();

	// Do this only if we're not dumping already
	if(m_FullTransponderFile == NULL)
	{
		// Get current time in local
		SYSTEMTIME currentTime;
		GetLocalTime(&currentTime);

		// Create filename
		TCHAR fullTransponderFileName[20];
		_stprintf_s(fullTransponderFileName, TEXT("%hu%02hu%02hu%02hu%02hu%02hu.ts"), 
											 currentTime.wYear,
											 currentTime.wMonth,
											 currentTime.wDay,
											 currentTime.wHour,
											 currentTime.wMinute,
											 currentTime.wSecond);

		// Open the full transponder dump file
		m_FullTransponderFile = _tfsopen(fullTransponderFileName, TEXT("wb"),  _SH_DENYWR);
	}

	unlock();
}

// Stop dumping the full transponder
void DVBParser::stopTransponderDump()
{
	lock();

	// Close the dump file if needed
	if(m_FullTransponderFile != NULL)
	{
		fclose(m_FullTransponderFile);
		m_FullTransponderFile = NULL;
	}

	unlock();
}

// Lock function with logging
void DVBParser::lock()
{
	log(5, true, m_TunerOrdinal, TEXT("Locking the parser\n"));
	m_cs.Lock();
}

void DVBParser::unlock()
{
	log(5, true, m_TunerOrdinal, TEXT("Unlocking the parser\n"));
	m_cs.Unlock();
}


void csvline_populate(vector<string> &record, const string& line, char delimiter)
{
    int linepos=0;
    int inquotes=false;
    char c;
    int linemax=line.length();
    string curstring;
    record.clear();
       
    while(line[linepos]!=0 && linepos < linemax)
    {    
        c = line[linepos];
       
        if (!inquotes && curstring.length()==0 && c=='"')
        {
            //beginquotechar
            inquotes=true;
        }
        else if (inquotes && c=='"')
        {
            //quotechar
            if ( (linepos+1 <linemax) && (line[linepos+1]=='"') )
            {
                //encountered 2 double quotes in a row (resolves to 1 double quote)
                curstring.push_back(c);
                linepos++;
            }
            else
            {
                //endquotechar
                inquotes=false;
            }
        }
        else if (!inquotes && c==delimiter)
        {
            //end of field
            record.push_back( curstring );
            curstring="";
        }
        else if (!inquotes && (c=='\r' || c=='\n') )
        {
            record.push_back( curstring );
            return;
        }
        else
        {
            curstring.push_back(c);
        }
        linepos++;
    }
    record.push_back( curstring );
    return;
}


// Reads the services.cache / transponders.cache
bool DVBParser::readTransponderServicesFromFile() 
{
	bool returnValue=true;
			
	vector<string> row;
	string line;
	ifstream in2("services.cache");
	if(in2.good()) 
		log(1, true, m_TunerOrdinal,TEXT("File services.cache opened successfully\n"));
	else
	{ 
		log(1, true, m_TunerOrdinal,TEXT("Error while trying to open services.cache\n"));
		returnValue = false;
	}

	int counter = 0;
	while(getline(in2,line)) 
	{
		csvline_populate(row, line, ',');
		Service newService;
		USHORT val;
		SHORT val2;
		byte val3;
		istringstream  iss0 (row[0],istringstream::in);
		istringstream  iss1 (row[1],istringstream::in);
		istringstream  iss2 (row[2],istringstream::in);
		istringstream  iss4 (row[4],istringstream::in);
		istringstream  iss5 (row[5],istringstream::in);
		iss0 >> val;
		newService.sid = val;
		iss2 >> val;		
		newService.tid = val;
		iss1 >> val;
		newService.onid = val;
		iss0 >> val2;
		newService.channelNumber = val2;

		newService.serviceNames[string("eng")] = row[3];

		iss4 >> val3;
		newService.runningStatus = val3;
		iss5 >> val3;
		newService.serviceType = val3;	
	  	
		m_PSIParser.addService(newService);
		counter++;
		log(3, true, 0,TEXT("Added Service SID:%hu TID:%hu name:%s from cache\n"),newService.sid,newService.tid,row[3].c_str());
	}

	in2.close();
	log(1, true, 0,TEXT("Added %d services from cache\n"),counter);

	ifstream in3("transponders.cache");
	if(in3.good()) 
		log(1, true, m_TunerOrdinal,TEXT("File transponders.cache opened successfully\n"));
	else 
	{ 
		log(1, true, m_TunerOrdinal,TEXT("Error while trying to open transponders.cache\n"));
		returnValue = false; 
	}
	
	counter=0;
	while(getline(in3,line)) 
	{
		csvline_populate(row, line, ',');
		Transponder newTransponder;
		istringstream  iss0 (row[0],istringstream::in);
		istringstream  iss1 (row[1],istringstream::in);
		istringstream  iss2 (row[2],istringstream::in);
		istringstream  iss3 (row[3],istringstream::in);

		USHORT val1;
		ULONG val2;
	
		iss0 >> val1;
		newTransponder.onid = val1;
		
		iss1 >> val1;
		newTransponder.tid = val1;

		iss2 >> val2;
		newTransponder.frequency = val2;

		iss3 >> val2;
		newTransponder.symbolRate = val2;

		newTransponder.polarization = getPolarizationFromString(row[4].c_str());

		newTransponder.modulation =  getModulationFromString(row[5].c_str());
								
		newTransponder.fec =  getFECFromString(row[6].c_str());

		log(3, true, 0,TEXT("Added Transponder ONID:%hu TID:%hu from cache\n"),newTransponder.onid, newTransponder.tid);

		m_PSIParser.addTransponder(newTransponder);
		counter++;

	}

	in3.close();
	log(1, true, 0,TEXT("Added %d transponders from cache\n"),counter);

	ifstream in4("satelliteInfo.cache");
	if(in4.good()) 
		log(1, true, m_TunerOrdinal,TEXT("File satelliteInfo.cache opened successfully\n"));
	else
	{ 
		log(1, true, m_TunerOrdinal,TEXT("Error while trying to open satelliteInfo.cache\n"));
		returnValue = false;
	}

	counter = 0;
	while(getline(in4,line)) 
	{
		csvline_populate(row, line, ',');
		string satelliteName = row[0];
		UINT orbitalLocation;
		UINT val;
		USHORT onid;
		istringstream  iss1 (row[1],istringstream::in);
		istringstream  iss2 (row[2],istringstream::in);
		istringstream  iss3 (row[3],istringstream::in);
		iss1 >> onid;
		iss2 >> orbitalLocation;
		iss3 >> val;
		bool east;
		if(val == 0)
			east = false;
		else
			east = true;

		g_pSatelliteInfo->addOrUpdateSatellite(0, onid, satelliteName, orbitalLocation, east);
	  	
		counter++;
	}

	in4.close();
	log(1, true, 0,TEXT("Added %d satellites info from cache\n"),counter);

	return returnValue;
}




BinaryConvolutionCodeRate DVBParser::getFECFromString(string str2)
{
	LPCTSTR  str = str2.c_str();
	if(_tcsicmp(str, TEXT("Not set")) == 0)
		return BDA_BCC_RATE_NOT_SET;	
	if(_tcsicmp(str, TEXT("Not defined")) == 0)
		return BDA_BCC_RATE_NOT_DEFINED;
	if(_tcsicmp(str, TEXT("1/2")) == 0)
		return BDA_BCC_RATE_1_2;
	if(_tcsicmp(str, TEXT("2/3")) == 0)
		return BDA_BCC_RATE_2_3;
	if(_tcsicmp(str, TEXT("3/4")) == 0)
		return BDA_BCC_RATE_3_4;
	if(_tcsicmp(str, TEXT("3/5")) == 0)
		return BDA_BCC_RATE_3_5;
	if(_tcsicmp(str, TEXT("4/5")) == 0)
		return BDA_BCC_RATE_4_5;
	if(_tcsicmp(str, TEXT("5/6")) == 0)
		return BDA_BCC_RATE_5_6;
	if(_tcsicmp(str, TEXT("5/11")) == 0)
		return BDA_BCC_RATE_5_11;
	if(_tcsicmp(str, TEXT("7/8")) == 0)
		return BDA_BCC_RATE_7_8;
	if(_tcsicmp(str, TEXT("1/4")) == 0)
		return BDA_BCC_RATE_1_4;
	if(_tcsicmp(str, TEXT("1/3")) == 0)
		return BDA_BCC_RATE_1_3;
	if(_tcsicmp(str, TEXT("2/5")) == 0)
		return BDA_BCC_RATE_2_5;
	if(_tcsicmp(str, TEXT("6/7")) == 0)
		return BDA_BCC_RATE_6_7;
	if(_tcsicmp(str, TEXT("8/9")) == 0)
		return BDA_BCC_RATE_8_9;
	if(_tcsicmp(str, TEXT("9/10")) == 0)
		return BDA_BCC_RATE_9_10;
	return BDA_BCC_RATE_NOT_DEFINED;
}


Polarisation DVBParser::getPolarizationFromString(string str2)
{
	LPCTSTR  str = str2.c_str();
	if(_tcsicmp(str, TEXT("Not set")) == 0)
		return BDA_POLARISATION_NOT_SET;	
	if(_tcsicmp(str, TEXT("Not defined")) == 0)
		return BDA_POLARISATION_NOT_DEFINED;
	if(_tcsicmp(str, TEXT("H")) == 0)
		return BDA_POLARISATION_LINEAR_H;
	if(_tcsicmp(str, TEXT("V")) == 0)
		return BDA_POLARISATION_LINEAR_V;
	if(_tcsicmp(str, TEXT("L")) == 0)
		return BDA_POLARISATION_CIRCULAR_L;
	if(_tcsicmp(str, TEXT("R")) == 0)
		return BDA_POLARISATION_CIRCULAR_R;
	return BDA_POLARISATION_NOT_DEFINED;
}


ModulationType DVBParser::getModulationFromString(string str2)
{
	LPCTSTR  str = str2.c_str();
	if(_tcsicmp(str, TEXT("Not set")) == 0)
		return BDA_MOD_NOT_SET;	
	if(_tcsicmp(str, TEXT("Not defined")) == 0)
		return BDA_MOD_NOT_DEFINED;
	if(_tcsicmp(str, TEXT("QPSK")) == 0)
		return BDA_MOD_QPSK;
	if(_tcsicmp(str, TEXT("8PSK")) == 0)
		return BDA_MOD_8PSK;
	if(_tcsicmp(str, TEXT("NBC-QPSK")) == 0)
		return BDA_MOD_NBC_QPSK;
	if(_tcsicmp(str, TEXT("16QAM")) == 0)
		return BDA_MOD_16QAM;
	if(_tcsicmp(str, TEXT("32QAM")) == 0)
		return BDA_MOD_32QAM;
	if(_tcsicmp(str, TEXT("64QAM")) == 0)
		return BDA_MOD_64QAM;
	if(_tcsicmp(str, TEXT("80QAM")) == 0)
		return BDA_MOD_80QAM;
	if(_tcsicmp(str, TEXT("96QAM")) == 0)
		return BDA_MOD_96QAM;
	if(_tcsicmp(str, TEXT("112QAM")) == 0)
		return BDA_MOD_112QAM;
	if(_tcsicmp(str, TEXT("128QAM")) == 0)
		return BDA_MOD_128QAM;
	if(_tcsicmp(str, TEXT("160QAM")) == 0)
		return BDA_MOD_160QAM;
	if(_tcsicmp(str, TEXT("192QAM")) == 0)
		return BDA_MOD_192QAM;
	if(_tcsicmp(str, TEXT("224QAM")) == 0)
		return BDA_MOD_224QAM;
	if(_tcsicmp(str, TEXT("256QAM")) == 0)
		return BDA_MOD_256QAM;
	if(_tcsicmp(str, TEXT("320QAM")) == 0)
		return BDA_MOD_320QAM;
	if(_tcsicmp(str, TEXT("384QAM")) == 0)
		return BDA_MOD_384QAM;
	if(_tcsicmp(str, TEXT("448QAM")) == 0)
		return BDA_MOD_448QAM;
	if(_tcsicmp(str, TEXT("512QAM")) == 0)
		return BDA_MOD_512QAM;
	if(_tcsicmp(str, TEXT("640QAM")) == 0)
		return BDA_MOD_640QAM;
	if(_tcsicmp(str, TEXT("768QAM")) == 0)
		return BDA_MOD_768QAM;
	if(_tcsicmp(str, TEXT("896QAM")) == 0)
		return BDA_MOD_896QAM;
	if(_tcsicmp(str, TEXT("1024QAM")) == 0)
		return BDA_MOD_1024QAM;
	return BDA_MOD_NOT_DEFINED;
}


// This is the top level DVB TS stream parsing routine, to be called on a generic TS buffer
void DVBParser::parseTSStream(const BYTE* inputBuffer,
							  int inputBufferLength)
{
	// Lock the parser
	lock();

	// Dump the buffer into the file if needed
	if(m_FullTransponderFile != NULL)
		if(fwrite(inputBuffer, sizeof(BYTE), inputBufferLength / sizeof(BYTE), m_FullTransponderFile) != inputBufferLength / sizeof(BYTE))
			log(2, true, m_TunerOrdinal, TEXT("Error while dumping full transponder, some data lost!\n"));

	// Get the TS header
	while(inputBufferLength > 0)
	{
		// First, take care of leftovers
		if(inputBufferLength < TS_PACKET_LEN)
		{
			// Copy it to a special buffer
			memcpy_s(m_LeftoverBuffer, sizeof(m_LeftoverBuffer), inputBuffer, inputBufferLength);
			// And remember its length
			m_LeftoverLength = inputBufferLength;
			// Boil out, nothing left to do here!
			break;
		}

		// This is the packet we're about to analyze
		BYTE currentPacket[TS_PACKET_LEN];
		// We assume we need to copy the input buffer from the beginning, but this might be wrong:-)
		int copyIndex = 0;
		// Same for the length of the data to be copied
		int copyLength = TS_PACKET_LEN;

		// Now, let's see of we have some leftovers pending
		if(*inputBuffer != '\x47' && m_LeftoverLength > 0)
		{
			// Copy it's content to the current buffer
			memcpy_s(currentPacket, sizeof(currentPacket), m_LeftoverBuffer, m_LeftoverLength);
			// Adjust destination index
			copyIndex = m_LeftoverLength;
			// Adjust length to be copied
			copyLength = TS_PACKET_LEN - m_LeftoverLength;
			// Zero the leftover length
			m_LeftoverLength = 0;
		}

		// Copy the contents of the current input buffer
		if(copyIndex > sizeof(currentPacket))
		{
			log(2, true, m_TunerOrdinal, TEXT("Critical error - leftover length is greater than the packet length, dropping the buffer...\n"));
			break;
		}
		else
			memcpy_s(currentPacket + copyIndex, sizeof(currentPacket) - copyIndex, inputBuffer, copyLength);

		// Get its header
		const ts_t* const header = (const ts_t* const)currentPacket;
		
		// Sanity check - the sync byte!
		if(header->sync_byte != '\x47')
		{	
			log(2, true, m_TunerOrdinal, TEXT("Catastrophic error - TS packet has wrong sync_byte, searching for start of next packet...\n"));
			
			// Things are very out of sync.  Find the start of the next packet		
			m_LeftoverLength = 0;
			ts_t* tempHeader = (ts_t*)header;
			while(inputBufferLength > 0 && tempHeader->sync_byte != '\x47')
			{
				// Adjust the input buffer to the next byte
				inputBuffer++;

				// And the remaining length
				inputBufferLength--;

				tempHeader = (ts_t*)inputBuffer;
			}

			continue;
		}

		// Handle the packet only if it doesn't have an error
		if(header->transport_error_indicator)
			log(2, true, m_TunerOrdinal, TEXT("Got an erroneous packet, skipping!\n"));
		else
		{
			// Get the pid of the current packet
			USHORT pid = HILO(header->PID);
				
			// Let's see if we need to handle this PID
			hash_map<USHORT, hash_set<TSPacketParser*>>::iterator parsers = m_ParserForPid.find(pid);
			if(parsers != m_ParserForPid.end())
				// If yes, pass it to its parsers
				for(hash_set<TSPacketParser*>::iterator parser = parsers->second.begin(); parser != parsers->second.end(); parser++)
				{
					// We assume the packet should not be abandoned
					bool abandonPacket = false;
					// Parse it
					(*parser)->parseTSPacket(header, pid, abandonPacket);
					// If the parser says we need to abandon the packet, do it
					if(abandonPacket)
						break;
				}
		}
		
		// Adjust the input buffer for the next iteration
		inputBuffer += copyLength;

		// And the remaining length
		inputBufferLength -= copyLength;
	}
	unlock();
}

// This function resets the PID to parser map and initializes a few constant entries
void DVBParser::resetParser(bool clearPSIParser)
{
	log(3,true,0,TEXT("Reset DVBParser invoked"));
	// Clear the PSI parser if requested
	if(clearPSIParser)
		m_PSIParser.clear();
	// Clear the map
	m_ParserForPid.clear();
	// Pass the internal PSI parser to PID 0 (PAT)
	assignParserToPid(0, &m_PSIParser);
	// Pass the internal PSI parser to PID 0x01 (CAT)
	assignParserToPid(0x01, &m_PSIParser);
	// Pass the internal PSI parser to PID 0x10 (NIT)
	assignParserToPid(0x10, &m_PSIParser);
	// Pass the internal PSI parser to PID 0x11 (SDT and BAT)
	assignParserToPid(0x11, &m_PSIParser);
	// Reset the connected clients flag
	m_HasConnectedClients = false;
}

void DVBParser::assignParserToPid(USHORT pid,
								  TSPacketParser* pParser)
{
	// If there was no parser set for this PID, create one
	if(m_ParserForPid.find(pid) == m_ParserForPid.end())
	{
		hash_set<TSPacketParser*> parsers;
		m_ParserForPid[pid] = parsers;
	}

	// Add the parser to the set for this PID
	m_ParserForPid[pid].insert(pParser);
}

// Stop listening on a specific parser
void DVBParser::dropParser(TSPacketParser* parser)
{
	// Go through all PIDs
	for(hash_map<USHORT, hash_set<TSPacketParser*>>::iterator parsers =	m_ParserForPid.begin(); parsers != m_ParserForPid.end();)
	{
		// Try to erase the parser from the set (it may be not there) and if the set becomes empty, delete the entry in the PID map
		if(parsers->second.erase(parser) == 1 && parsers->second.empty())
			parsers = m_ParserForPid.erase(parsers);
		else
			parsers++;
	}
}

// This routine is called before the beginning of running the PSI parser
void PSIParser::clear()
{
	// Clear all the maps
	m_Provider.clear();
	m_PMTPids.clear();
	m_ESPidsForSid.clear();
	m_CAPidsForSid.clear();
	m_EMMPids.clear();
	m_BufferForPid.clear();
	m_PidStreamInfo.clear();
	m_CurrentTID = 0;
	m_CurrentONID = 0;
	m_AllowParsing = true;
	m_PMTCounter = 0;
	m_AustarDigitalDone = false;
}

// This routine converts PSI TS packets to readable PSI tables
void PSIParser::parseTSPacket(const ts_t* const packet,
							  USHORT pid,
							  bool& abandonPacket)
{
	// If parsing is disallowed, boil out
	if(!m_AllowParsing)
		return;

	// If the packet contains only the adaptaion field, just skip it
	if(packet->adaptation_field_control == 2)
		return;

	// Offset of the data, 0 unless the packet has adaptation field
	USHORT offset = 0;

	// If the packet contains adaptation field and data bytes, skip the adaptation field
	if(packet->adaptation_field_control == 3)
		// This is the adaptation field length
		offset = *((BYTE*)packet + TS_LEN);
		

	// If this is the first time we encountered this PID, create a new entry for it
	if(m_BufferForPid.find(pid) == m_BufferForPid.end())
	{
		SectionBuffer sectionBuffer;
		m_BufferForPid[pid] = sectionBuffer;
	}

	// Get the relevant table
	SectionBuffer& sectionBuffer = m_BufferForPid[pid];

	// Let's check the continuity counter status	
	BYTE expectedContinuityCounter = sectionBuffer.lastContinuityCounter == 15 ? 0 : sectionBuffer.lastContinuityCounter + 1;
	if(expectedContinuityCounter != packet->continuity_counter)
		log(2, true, m_pParent->getTunerOrdinal(), TEXT("Some packets were lost (expected CC=%hu, actual CC=%hu)\n"), (USHORT)expectedContinuityCounter, (USHORT)packet->continuity_counter);

	// Update continuity counter
	sectionBuffer.lastContinuityCounter = (BYTE)packet->continuity_counter;

	// Set the starting address
	const BYTE* inputBuffer = (BYTE*)packet + TS_LEN + offset;
	// And the remaining length
	short remainingLength = TS_PACKET_LEN - TS_LEN - offset;

	// Boil out on invalid remaining length
	if(remainingLength <= 0)
	{
		log(2, true, m_pParent->getTunerOrdinal(), TEXT("Invalid offset on a packet with adaptation field, skipping...\n"));
		return;
	}

	// Let's see if the packet contains start of a new section
	if(packet->payload_unit_start_indicator)
	{
		// Find the pointer byte - should be the first one after the TS header
		const BYTE pointer = *(inputBuffer);

		// Adjust the input buffer
		inputBuffer++;
		// And the remaining length
		remainingLength--;

		// Let's see if we have to do something with data before the pointer
		if(sectionBuffer.offset != 0 && sectionBuffer.expectedLength != 0)
		{
			// Make sure expected length doesn't go beyond the pointer
			if(sectionBuffer.expectedLength > pointer)
			{
				// If no, discard the previously saved buffer
				log(2, true, m_pParent->getTunerOrdinal(), TEXT("PSI table messed up, fixing...\n"));
				sectionBuffer.offset = 0;
				sectionBuffer.expectedLength = 0;
			}
			else
			{
				if(sectionBuffer.offset > sizeof(sectionBuffer.buffer))
				{
					log(2, true, m_pParent->getTunerOrdinal(), TEXT("PSI table parsing went bananas, this really shouldn't be happening...\n"));
					sectionBuffer.offset = 0;
					sectionBuffer.expectedLength = 0;
				}
				else
				{
					// Append the rest of the section
					memcpy_s(&sectionBuffer.buffer[sectionBuffer.offset], sizeof(sectionBuffer.buffer) - sectionBuffer.offset, inputBuffer, sectionBuffer.expectedLength);
					// Adjust the offset
					sectionBuffer.offset += sectionBuffer.expectedLength;
					// Adjust the expected length
					sectionBuffer.expectedLength = 0;
					// Pass it to further parsing
					parseTable((const pat_t* const)sectionBuffer.buffer, (short)sectionBuffer.offset, abandonPacket);
					// Adjust the offset once again
					sectionBuffer.offset = 0;
				}
			}
		}
		
		// Adjust the input buffer
		inputBuffer += pointer;
		// And the remaining length
		remainingLength -= pointer;

		// Now loop through the rest of the packet
		while(remainingLength > 0 && (*inputBuffer) != (BYTE)'\xFF')
		{
			// Find the beginning of the PSI table
			const pat_t* const pat = (const pat_t* const)(inputBuffer);
			// Get the section length
			const USHORT sectionLength = HILO(pat->section_length) + TABLE_PREFIX_LEN;

			// See if we can pass anything to immediate processing
			if(remainingLength >= sectionLength)
				parseTable(pat, sectionLength, abandonPacket);
			else
			{
				// Otherwise we need to keep it in the buffer
				memcpy_s(sectionBuffer.buffer, sizeof(sectionBuffer.buffer), (BYTE*)pat, remainingLength);
				// Set the offset
				sectionBuffer.offset = remainingLength;
				// Set the expected length
				sectionBuffer.expectedLength = sectionLength - remainingLength;
				// Boil out
				break;
			}

			// Adjust the input buffer
			inputBuffer += sectionLength;
			// And the remaining length
			remainingLength -= sectionLength;
		}
		if(remainingLength < 0)
			log(2, true, m_pParent->getTunerOrdinal(), TEXT("Possible corruption - attempt to jump beyond packet boundaries...\n"));
	}
	else
		// Take care of a packet without new section
		if(sectionBuffer.offset != 0 && sectionBuffer.expectedLength != 0)
		{
			// Calculate the length of the data we need to copy
			const USHORT lengthToCopy = min(remainingLength, sectionBuffer.expectedLength);
			if(sectionBuffer.offset > sizeof(sectionBuffer.buffer))
			{
				log(2, true, m_pParent->getTunerOrdinal(), TEXT("PSI table parsing went bananas, this really shouldn't be happening...\n"));
				sectionBuffer.offset = 0;
				sectionBuffer.expectedLength = 0;
			}
			else
			{
				// Copy the data to the buffer
				memcpy_s(&sectionBuffer.buffer[sectionBuffer.offset], sizeof(sectionBuffer.buffer) - sectionBuffer.offset, inputBuffer, lengthToCopy);
				// Adjust the offset
				sectionBuffer.offset += lengthToCopy;
				// And the expected length
				sectionBuffer.expectedLength -= lengthToCopy;
				// Let's see if we can pass the buffer to further processing
				if(sectionBuffer.expectedLength == 0)
				{
					// Parse the section
					parseTable((const pat_t* const)sectionBuffer.buffer, (short)sectionBuffer.offset, abandonPacket);
					// Reset the offset
					sectionBuffer.offset = 0;
					// Make sure the remainder is just stuffing '\xFF' bytes
					for(int i = lengthToCopy; i < remainingLength; i++)
						if(inputBuffer[i] != (BYTE)'\xFF' && inputBuffer[i] != (BYTE)'\0')
							log(4, true, m_pParent->getTunerOrdinal(), TEXT("Invalid byte 0x%.02X at offset %d\n"), (UINT)inputBuffer[i], 188 - remainingLength + i);
				}
			}
		}
}

// This is the top level DVB SI stream parsing routine, to be called on a generic SI buffer
void PSIParser::parseTable(const pat_t* const table,
						   short remainingLength,
						   bool& abandonPacket)
{
	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table;

	// Do until the stream fully parsed
	while(remainingLength != 0)
	{
		// Cast to a generic table type
		const pat_t* const table = (const pat_t* const)inputBuffer;

		// Get table length
		const USHORT tableLength = HILO(table->section_length) + TABLE_PREFIX_LEN;

		// Get the CRC
		const unsigned __int32 crc = ntohl(*(const UINT*)(inputBuffer + tableLength - CRC_LENGTH));

		// Do all the stuff below only if this is not a stuffing table
		if(table->table_id != 0x72)
			// Make sure the CRC matches
			if(crc == _dvb_crc32(inputBuffer, tableLength - CRC_LENGTH))
			{

				// If this is a PAT table
				if(table->table_id == (BYTE)'\0')
					parsePATTable(table, tableLength, abandonPacket);		// Parse PAT table
				//Or if this is a SDT table
				else if(table->table_id == (BYTE)'\x42' || table->table_id == (BYTE)'\x46')
					parseSDTTable((const sdt_t* const)table, (short)tableLength);	// Parse the SDT table
				// Or if this is a BAT table
				else if(table->table_id == (BYTE)'\x4A')
					parseBATTable((const nit_t* const)table, (short)tableLength);	// Parse the BAT table
				// Or if this is a PMT table
				else if(table->table_id == (BYTE)'\x02')
					parsePMTTable((const pmt_t* const)table, (short)tableLength);	// Parse the PMT table
				// Or if this is a NIT table
				else if(table->table_id == (BYTE)'\x40' || table->table_id == (BYTE)'\x41')
					parseNITTable((const nit_t* const)table, (short)tableLength);	// Parse the NIT table
				// Or if this is a CAT table
				else if(table->table_id == (BYTE)'\x01')
					parseCATTable((const cat_t* const)table, (short)tableLength);	// Parse the CAT table
				// Of it is everything else
				else
					parseUnknownTable(table, (short)tableLength);					// Parse the unknown table
			}
			else
				// Print a log message on unmatching CRC and drop the table
				log(2, true, m_pParent->getTunerOrdinal(), TEXT("PSI table CRC error, dropping the table...\n"));

		// Amend input buffer pointer
		inputBuffer += tableLength;

		// Amend input buffer length
		remainingLength -= tableLength;
	}
}

void PSIParser::parseCATTable(const cat_t* const table,
							  short remainingLength)
{
	// Boil out if EMM PID already known
	if(!m_EMMPids.empty())
		return;

	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + CAT_LEN;

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + CAT_LEN;

	// Flag to indicate if we had any CA descriptors
	bool hasCADescriptors = false;

	// For each CA descriptor do
	while(remainingLength != 0)
	{
		// Get the generic descriptor
		const descr_gen_t* const descriptor = CastGenericDescriptor(inputBuffer);
		if(descriptor->descriptor_tag == '\x09')
		{
			// Set the flag
			hasCADescriptors = true;
			// Get CA descriptor
			const descr_ca_t* const caDescriptor = CastCaDescriptor(inputBuffer);
			// Fill the CAScheme structure with data
			CAScheme caScheme;
			// Figure out the EMM PID
			caScheme.pid = HILO(caDescriptor->CA_PID);
			// Figure out the CAID
			caScheme.caId = HILO(caDescriptor->CA_type);
			// And the provider ID, if present
			if(caDescriptor->descriptor_length > DESCR_CA_LEN - 2 /*-2 desc+len*/)
				caScheme.provId = caDescriptor->CA_provid;
			else
				caScheme.provId = 0;
			// Let's see if these CAID/PROVID are served
			if(g_pConfiguration->isCAIDServed(caScheme.caId) && g_pConfiguration->isPROVIDServed(caScheme.provId))
			{
				// Make the log entry
				log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found CA descriptor EMM PID=0x%hX(%hu), CAID=0x%hX(%hu), PROVID=0x%X(%u), these CAID/PROVID are served, so packets with this PID will be passed to plugins\n"), 
					caScheme.pid, caScheme.pid, caScheme.caId, caScheme.caId, caScheme.provId, caScheme.provId);
				// Add this CA scheme to the map
				m_EMMPids.insert(caScheme);
			}
			else
				// Make the log entry
				log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found CA descriptor EMM PID=0x%hX(%hu), CAID=0x%hX(%hu), PROVID=0x%X(%u), these CAID/PROVID are NOT served, so packets with this PID will NOT be passed to plugins\n"), 
						caScheme.pid, caScheme.pid, caScheme.caId, caScheme.caId, caScheme.provId, caScheme.provId);
		}
		// Adjust length and pointer
		remainingLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
		inputBuffer += descriptor->descriptor_length + DESCR_GEN_LEN;
	}
	// Let's see if we had CA descriptors but none was actually used
	if(m_EMMPids.empty() && hasCADescriptors)
		log(0, true, m_pParent->getTunerOrdinal(), TEXT("None of the existing CA descriptors are used, have you forgotten to specify \"ServedCAIDs\"?\n"));
}

void PSIParser::parsePMTTable(const pmt_t* const table,
							  short remainingLength)
{
	// Get SID the for this PMT
	USHORT programNumber = HILO(table->program_number);
	if(programNumber == (USHORT)-1)
		return;

	// Let's see if we already discovered EMM PID or passed PMT packets counter threshold
	// If no, boil out
	if(m_EMMPids.empty() && m_PMTCounter < g_pConfiguration->getPMTThreshold())
	{
		m_PMTCounter++;
		return;
	}

	// Boil out if there is no version change for this program
	if(m_PMTVersions.find(programNumber) != m_PMTVersions.end() &&
			(m_PMTVersions[programNumber] == table->version_number && table->current_next_indicator ||
			!table->current_next_indicator))
		return;

	// Save the PMT version 
	m_PMTVersions[programNumber] = table->version_number;

	// Notify the parent parsers there was a change in PMT
	m_pParent->setPMTChanged(programNumber);

	// Log warning message if EMMPid is still 0
	if(m_EMMPids.empty())
		log(2, true, m_pParent->getTunerOrdinal(), TEXT("Warning: no EMM PIDs have been discovered after %hu PMT packets, EMM data will not be passed to plugins!!!\n"), g_pConfiguration->getPMTThreshold());

	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + PMT_LEN;

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + PMT_LEN;

	// Here we keep the program info
	ProgramInfo programInfo;

	// Copy the PCR PID
	programInfo.m_PCRPid = HILO(table->PCR_PID);

	// Go through the program info, if present
	USHORT programInfoLength = HILO(table->program_info_length);

	// Initialize PID vectors
	hash_set<USHORT> pids;
	m_ESPidsForSid[programNumber] = pids;
	m_CAPidsForSid[programNumber] = pids;

	// Create a map from ECM PID to CA Scheme structure
	hash_set<CAScheme> caMap;

	// Flag to indicate if we had any CA descriptors
	bool hasCADescriptors = false;

	// Loop through the program info descriptors (we look for the CA descriptors)
	while(programInfoLength != 0)
	{
		const descr_gen_t* const descriptor = CastGenericDescriptor(inputBuffer);
		if(descriptor->descriptor_tag == (BYTE)0x09)
		{
			// Set the flag
			hasCADescriptors = true;
			// Get the descriptor pointer
			const descr_ca_t* const caDescriptor = CastCaDescriptor(inputBuffer);
			// Fill the CAScheme structure with data
			CAScheme caScheme;
			// Figure out the ECM PID
			caScheme.pid = HILO(caDescriptor->CA_PID);
			// Get the CAID
			caScheme.caId = HILO(caDescriptor->CA_type);
			// And the PROVID, if present
			if(caDescriptor->descriptor_length > DESCR_CA_LEN - 2)
				caScheme.provId = caDescriptor->CA_provid;
			else
				caScheme.provId = 0;
			// Let's see if these CAID/PROVID are served
			if(g_pConfiguration->isCAIDServed(caScheme.caId) && g_pConfiguration->isPROVIDServed(caScheme.provId))
			{
				// Add the CA Scheme to the set
				caMap.insert(caScheme);
				// Add the ECM pid to the map of PIDs we need to listen to
				m_CAPidsForSid[programNumber].insert(caScheme.pid);
				// Log the descriptor data
				log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found CA descriptor for the entire SID=%hu, ECM PID=0x%hX(%hu), CAID=0x%hX(%hu), PROVID=0x%X(%u), these CAID/PROVID are served, so ECM packets with this PID will be passed to plugins\n"),
							programNumber, caScheme.pid, caScheme.pid, caScheme.caId, caScheme.caId, caScheme.provId, caScheme.provId);
			}
			else
				// Log the descriptor data
				log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found CA descriptor for the entire SID=%hu, ECM PID=0x%hX(%hu), CAID=0x%hX(%hu), PROVID=0x%X(%u), these CAID/PROVID are NOT served, so ECM packets with this PID will NOT be passed to plugins\n"),
							programNumber, caScheme.pid, caScheme.pid, caScheme.caId, caScheme.caId, caScheme.provId, caScheme.provId);
		}
		else
		{
			// Just save descriptors of any type other than CA
			BYTE* tempBuffer = new BYTE[descriptor->descriptor_length + DESCR_GEN_LEN];
			memcpy_s(tempBuffer, descriptor->descriptor_length + DESCR_GEN_LEN, inputBuffer, descriptor->descriptor_length + DESCR_GEN_LEN);
			programInfo.m_Descriptors.push_back(pair<BYTE, BYTE*>(descriptor->descriptor_length + DESCR_GEN_LEN, tempBuffer));
			programInfo.m_DescriptorsLength += descriptor->descriptor_length + DESCR_GEN_LEN;
		}

		// Go to the next record
		programInfoLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
		remainingLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
		inputBuffer += descriptor->descriptor_length + DESCR_GEN_LEN;
	}
	
	// Copy the program info to the global structure
	m_ProgramInfoForSid[programNumber] = programInfo;
	programInfo.m_Descriptors.clear();

	// Let's see if we had CA descriptors but none was actually used for this ES PID
	if(caMap.empty() && hasCADescriptors)
		log(0, true, m_pParent->getTunerOrdinal(), TEXT("None of the existing CA descriptors are used for the entire SID=%hu, have you forgotten to specify \"ServedCAIDs\"/\"ServedPROVIDs\"?\n"), programNumber);

	// Add the CA type into the map only if there were CA descriptors
	if(hasCADescriptors)
		m_CATypesForSid[programNumber] = caMap;

	// For each stream descriptor do
	while(remainingLength != 0)
	{
		// Get stream info
		const pmt_info_t* const streamInfo = (const pmt_info_t* const)inputBuffer;

		// Get the stream PID
		USHORT ESPid = HILO(streamInfo->elementary_PID);

		// Create the new StreamInfo structure
		StreamInfo info;

		// Save the PID type
		info.m_StreamType = streamInfo->stream_type;

		// Log the ES type
		log(2, true, m_pParent->getTunerOrdinal(), TEXT("PID=%hu(0x%hX) of SID=%hu(0x%hX) has Type=0x%.02hX\n"), ESPid, ESPid, programNumber, programNumber, (USHORT)streamInfo->stream_type);

		// Get ES info length
		USHORT ESInfoLength = HILO(streamInfo->ES_info_length);

		// Update the pointer and the length
		inputBuffer += PMT_INFO_LEN;
		remainingLength -= PMT_INFO_LEN;

		// Create a map from ECM PID to CA Scheme structure
		hash_set<CAScheme> caMap;

		// Flag to indicate if we had any CA descriptors
		bool hasCADescriptors = false;

		// Loop through the ES info records (we look for the CA descriptors)
		while(ESInfoLength != 0)
		{
			const descr_gen_t* const descriptor = CastGenericDescriptor(inputBuffer);
			if(descriptor->descriptor_tag == (BYTE)0x09)
			{
				// Set the flag
				hasCADescriptors = true;
				// Get the descriptor pointer
				const descr_ca_t* const caDescriptor = CastCaDescriptor(inputBuffer);
				// Fill the CAScheme structure with data
				CAScheme caScheme;
				// Figure out the ECM PID
				caScheme.pid = HILO(caDescriptor->CA_PID);
				// Get the CAID
				caScheme.caId = HILO(caDescriptor->CA_type);
				// And the PROVID, if present
				if(caDescriptor->descriptor_length > DESCR_CA_LEN - 2)
					caScheme.provId = caDescriptor->CA_provid;
				else
					caScheme.provId = 0;
				// Let's see if these CAID/PROVID are served
				if(g_pConfiguration->isCAIDServed(caScheme.caId) && g_pConfiguration->isPROVIDServed(caScheme.provId))
				{
					// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
					// Temporary hack - use only one ECM pid per CAID per program
					// TODO : fixme!
					// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
					if(m_CAPidsForSid[programNumber].empty() || *m_CAPidsForSid[programNumber].begin() == caScheme.pid)
					{
						// Add the CA Scheme to the set
						caMap.insert(caScheme);
						// Add the ECM pid to the map of PIDs we need to listen to
						m_CAPidsForSid[programNumber].insert(caScheme.pid);
						// Log the descriptor data
						log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found CA descriptor for PID=%hu, SID=%hu, ECM PID=0x%hX(%hu), CAID=0x%hX(%hu), PROVID=0x%X(%u), these CAID/PROVID are served, so ECM packets with this PID will be passed to plugins\n"),
									ESPid, programNumber, caScheme.pid, caScheme.pid, caScheme.caId, caScheme.caId, caScheme.provId, caScheme.provId);
					}
					else
						// Log the descriptor data
						log(0, true, m_pParent->getTunerOrdinal(), TEXT("Found CA descriptor for PID=%hu, SID=%hu, ECM PID=0x%hX(%hu), CAID=0x%hX(%hu), PROVID=0x%X(%u), these CAID/PROVID are served, but another ECM PID=0x%hX(%hu) was already found for the same SID, so packets with this PID will NOT be passed to plugins\n"),
									ESPid, programNumber, caScheme.pid, caScheme.pid, caScheme.caId, caScheme.caId, caScheme.provId, caScheme.provId, *m_CAPidsForSid[programNumber].begin(), *m_CAPidsForSid[programNumber].begin());
				}
				else
					// Log the descriptor data
					log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found CA descriptor for PID=%hu, SID=%hu, ECM PID=0x%hX(%hu), CAID=0x%hX(%hu), PROVID=0x%X(%u), these CAID/PROVID are NOT served, so ECM packets with this PID will NOT be passed to plugins\n"),
								ESPid, programNumber, caScheme.pid, caScheme.pid, caScheme.caId, caScheme.caId, caScheme.provId, caScheme.provId);
			}
			else
			{
				// Just save descriptors of any type other than CA
				BYTE* tempBuffer = new BYTE[descriptor->descriptor_length + DESCR_GEN_LEN];
				memcpy_s(tempBuffer, descriptor->descriptor_length + DESCR_GEN_LEN, inputBuffer, descriptor->descriptor_length + DESCR_GEN_LEN);
				info.m_Descriptors.push_back(pair<BYTE, BYTE*>(descriptor->descriptor_length + DESCR_GEN_LEN, tempBuffer));
				info.m_DescriptorsLength += descriptor->descriptor_length + DESCR_GEN_LEN;
			}

			// Go to the next record
			ESInfoLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
			remainingLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
			inputBuffer += descriptor->descriptor_length + DESCR_GEN_LEN;
		}
		// Let's see if we had CA descriptors but none was actually used for this ES PID
		if(caMap.empty() && hasCADescriptors)
			log(0, true, m_pParent->getTunerOrdinal(), TEXT("None of the existing CA descriptors are used for PID=%hu, SID=%hu, either you have forgotten to specify \"ServedCAIDs\"/\"ServedPROVIDs\" or this PID is not going to be decoded because it uses a non-default ECM PID, so this PID will not be present in the output!\n"), ESPid, programNumber);
		else
			// Add this elementary stream information to the set
			m_ESPidsForSid[programNumber].insert(ESPid);

		// Add the CA type into the map only if there were CA descriptors
		if(hasCADescriptors && !caMap.empty() && m_CATypesForSid.find(programNumber) == m_CATypesForSid.end())
			m_CATypesForSid[programNumber] = caMap;

		// Save all the descriptors for PID
		m_PidStreamInfo[ESPid] = info;
		// Purge descriptors from info, to prevent buffer deletion
		info.m_Descriptors.clear();
	}

	// Add the EMM PIDs discovered earlier
	for(EMMInfo::const_iterator it = m_EMMPids.begin(); it != m_EMMPids.end(); it++)
		m_CAPidsForSid[programNumber].insert(it->pid);
}

// PAT table parsing routine
void PSIParser::parsePATTable(const pat_t* const table,
							  short remainingLength,
							  bool& abandonPacket)
{
	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + PAT_LEN;

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + PAT_LEN;

	// Get this transponder TSID
	USHORT tid = HILO(table->transport_stream_id);

	// Update stuff only if transponder changed
	if(tid == m_CurrentTID)
		return;

	// Indicate no further processing for the packet is necessary
	abandonPacket = true;

	// Tell the parent to clean its PID  map
	m_pParent->resetParser(false);

	// Update current TID
	m_CurrentTID = tid;
	
	// Remove all entries from the PMT and derived hashes
	m_PMTPids.clear();
	m_ESPidsForSid.clear();
	m_CAPidsForSid.clear();

	// For each program descriptor do
	while(remainingLength != 0)
	{
		// These are program descriptors
		const pat_prog_t* const programDescriptor = (const pat_prog_t* const)inputBuffer;

		// Get the program PMT pid
		USHORT sid = HILO(programDescriptor->program_number);

		// Get the program NID
		USHORT pmtPid = HILO(programDescriptor->network_pid);

		// Update PMT hash
		m_PMTPids[sid] = pmtPid;

		// Tell the parent to start listening to the new PID
		m_pParent->assignParserToPid(pmtPid, this);

		// Adjust input buffer pointer
		inputBuffer += PAT_PROG_LEN;

		// Adjust remaining length
		remainingLength -= PAT_PROG_LEN;
	}
}

void PSIParser::addService(Service serv) {
	m_Provider.m_Services[NetworkProvider::getUniqueSID(serv.onid, serv.sid)] = serv;
	//log(0, true, 0,TEXT("Added Service2 %hu thru cache\n",serv.sid));
}


void PSIParser::addTransponder(Transponder trans) {
	m_Provider.m_Transponders[NetworkProvider::getUniqueSID(trans.onid, trans.tid)] = trans;
	//log(0, true, 0,TEXT("Added Transponder %hu thru cache\n"),trans.tid);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine parses SDT tables which are usually transmitted on PID 0x11 of the transport stream.
// From these tables we can get Service IDs (must be unique) as well as names of services (YES has them in 
// 4 languages) and other useful information, like running status and service type.
// Note: here we just detect services and record their information, we don't decide yet which services need to be
// included in the output file, this is done when corresponding bouquet tables are analysed
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PSIParser::parseSDTTable(const sdt_t* const table,
							  short remainingLength)
{
	// If the provider info has already been copied, boil out
	if(m_ProviderInfoHasBeenCopied)
		return;

	// Get the ONID of the service network
	const USHORT onid = HILO(table->original_network_id);

	// Get TID of the transponder
	const USHORT tid = HILO(table->transport_stream_id);

	// Boil out if the transponder is not found in NIT(s)
	if(m_Provider.getTransponders().find(NetworkProvider::getUniqueSID(onid, tid)) == m_Provider.getTransponders().end())
		return;

	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + SDT_LEN;

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + SDT_LEN;

	// For each service descriptor do
	while(remainingLength != 0)
	{
		// These are service descriptors
		const sdt_descr_t* const serviceDescriptor = (const sdt_descr_t* const)inputBuffer;

		// Get service number
		const USHORT sid = HILO(serviceDescriptor->service_id);

		// Get the combined ONID&SID
		const UINT32 usid = NetworkProvider::getUniqueSID(onid, sid);

		// Get the length of descriptors loop
		const USHORT descriptorsLoopLength = HILO(serviceDescriptor->descriptors_loop_length);

		// Only if encountered a new service
		if(m_Provider.m_Services.find(usid) == m_Provider.m_Services.end())
		{
			// Update timestamp
			time(&m_TimeStamp);

			// We have a new service, lets initiate its fields as needed
			Service newService;
			newService.sid = sid;
			newService.tid = tid;
			newService.onid = onid;
			newService.runningStatus = serviceDescriptor->running_status;

			// Starting placement
			const BYTE* currentDescriptor = (const BYTE*)(inputBuffer + SDT_DESCR_LEN);
			int descriptorLoopRemainingLength = descriptorsLoopLength;

			// For each descriptor in the loop
			while(descriptorLoopRemainingLength != 0)
			{
				// We don't know the exact descriptor yet
				const descr_gen_t* genericDescriptor = CastGenericDescriptor(currentDescriptor);

				// Let's see which descriptor did we get
				switch(genericDescriptor->descriptor_tag)
				{
					case 0x48:
					{
						// OK, now we know we have a service descriptor
						const descr_service_t* const serviceInfoDescriptor = CastServiceDescriptor(genericDescriptor);

						// Let's copy the service type from here
						newService.serviceType = serviceInfoDescriptor->service_type;

						// Let's see if this service already has a name in English
						hash_map<string, string>::const_iterator it = newService.serviceNames.find(string("eng"));
						if(it == newService.serviceNames.end())
						{
							// Get the service name
							int serviceNameLength = *(currentDescriptor + DESCR_SERVICE_LEN + serviceInfoDescriptor->provider_name_length);
							const char* beginName = (const char*)currentDescriptor + DESCR_SERVICE_LEN + serviceInfoDescriptor->provider_name_length + 1;
							UINT skipCounter = (*((const BYTE*)beginName) >= (BYTE)32 || serviceNameLength == 0) ? 0 : 1;
							string serviceName(beginName + skipCounter, serviceNameLength - skipCounter);

							// Put the name of service in English into the map
							newService.serviceNames[string("eng")] = serviceName;
						}

						break;
					}
					case 0x5D:
					{

						// Here we have a multilingual service descriptor
						const BYTE* currentServiceName = currentDescriptor + DESCR_GEN_LEN;
						int remainingLen = genericDescriptor->descriptor_length;

						// Till the end
						while(remainingLen != 0)
						{
							// There get the language and the service name
							string language((const char*)currentServiceName, 3);
							int providerNameLength = *(currentServiceName + 3);
							int serviceNameLength = *(currentServiceName + 4 + providerNameLength);
							string serviceName((const char*)currentServiceName + 5 + providerNameLength, serviceNameLength);

							// Put the service name into the map
							newService.serviceNames[language] = serviceName;

							// Adjust pointers and counters
							currentServiceName += providerNameLength + serviceNameLength + 5;
							remainingLen -= providerNameLength + serviceNameLength + 5;
						}
						break;
					}

					// Ones we don't handle quite yet (and may never)
					case 0x4A:		// Linkage Descriptor
					case 0x86:		// Tier info for subscriptions
					case 0x93:		// Channel rights
					case 0x84:		// Unknown
					case 0x85:		// Unknown
					case 0x82:		// Dish and Bev EMM
					case 0x83:		// Dish and Bev EMM
					case 0x80:		// Dish ECM
					case 0x8C:		// Unknown
					case 0x8E:		// Bev ECM
					case 0x9B:		// Unknown
					case 0x9E:		// Unknown
					case 0x99:		// Subchannel information for local channels ...
									// If a channel has a digital subchannel, it and all its subchannels gets this descriptor	
					case 0xA3:		// Unknown
						break;

					default:
						log(4, true, m_pParent->getTunerOrdinal(), TEXT("!!! Unknown Service Descriptor, type=%02X\n"),
										(UINT)genericDescriptor->descriptor_tag); 
						break;
				}

				// Adjust current descriptor pointer
				currentDescriptor += genericDescriptor->descriptor_length + DESCR_GEN_LEN;

				// Adjust the remaining loop length
				descriptorLoopRemainingLength -= genericDescriptor->descriptor_length + DESCR_GEN_LEN;
			}
			
			// Insert the new service into the map - filter out certain ones only for non north americans 
			if(g_pConfiguration->isNorthAmerica())
			{
				if(!g_pConfiguration->includeONIDs() || g_pConfiguration->includeONID(newService.onid))
				{
					if(!g_pConfiguration->excludeONID(newService.onid))
					{
						m_Provider.m_Services[usid] = newService;

						log(3, true, m_pParent->getTunerOrdinal(), TEXT("Service Discovered: SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
							newService.sid, newService.onid, newService.tid, (USHORT)newService.channelNumber, newService.serviceNames["eng"].c_str(), (USHORT)newService.serviceType, (USHORT)newService.runningStatus);
					}
					else
					{
						log(3, true, m_pParent->getTunerOrdinal(), TEXT("Service Discovered but excluded (1): SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
							newService.sid, newService.onid, newService.tid, (USHORT)newService.channelNumber, newService.serviceNames["eng"].c_str(), (USHORT)newService.serviceType, (USHORT)newService.runningStatus);
					}
				}
				else
				{
					log(3, true, m_pParent->getTunerOrdinal(), TEXT("Service Discovered but excluded (2): SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
						newService.sid, newService.onid, newService.tid, (USHORT)newService.channelNumber, newService.serviceNames["eng"].c_str(), (USHORT)newService.serviceType, (USHORT)newService.runningStatus);
				}
			}
			else
			{
				if((newService.runningStatus != 0 || DISH_ONID(onid)) && (!FOXTEL_ONID(onid) && !SKYNZ_ONID(onid) && !SKYITALIA_ONID(onid) || newService.serviceType < 0x80))
				{
					if(!g_pConfiguration->includeONIDs() || g_pConfiguration->includeONID(newService.onid))
					{
						if(!g_pConfiguration->excludeONID(newService.onid))
						{
							m_Provider.m_Services[usid] = newService;

							// Print log message
							log(3, true, m_pParent->getTunerOrdinal(), TEXT("Discovered SID=%hu, ONID=%hu, TSID=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
								sid, onid, tid, newService.serviceNames["eng"].c_str(), (USHORT)newService.serviceType, (USHORT)newService.runningStatus);
						}
						else
						{
							log(3, true, m_pParent->getTunerOrdinal(), TEXT("Service Discovered but excluded (1): SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
								newService.sid, newService.onid, newService.tid, (USHORT)newService.channelNumber, newService.serviceNames["eng"].c_str(), (USHORT)newService.serviceType, (USHORT)newService.runningStatus);
						}
					}
					else
					{
						log(3, true, m_pParent->getTunerOrdinal(), TEXT("Service Discovered but excluded (2): SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
							newService.sid, newService.onid, newService.tid, (USHORT)newService.channelNumber, newService.serviceNames["eng"].c_str(), (USHORT)newService.serviceType, (USHORT)newService.runningStatus);
					}
				}
				else
				{
					log(3, true, m_pParent->getTunerOrdinal(), TEXT("Service Discovered but disallowed: SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
							newService.sid, newService.onid, newService.tid, (USHORT)newService.channelNumber, newService.serviceNames["eng"].c_str(), (USHORT)newService.serviceType, (USHORT)newService.runningStatus);
				}
			}
		}

		// Adjust input buffer pointer
		inputBuffer += descriptorsLoopLength + SDT_DESCR_LEN;

		// Adjust remaining length
		remainingLength -= descriptorsLoopLength + SDT_DESCR_LEN;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine parses BATs (bouquet association tables) which are usually transmitted on PID 0x11 of the
// transport stream. Here the channel number allocation takes place.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PSIParser::parseBATTable(const nit_t* const table,
							  short remainingLength)
{
	// If the provider info has already been copied, boil out
	if(m_ProviderInfoHasBeenCopied)
		return;

	// Get the bouquet ID
	USHORT bouquetID = HILO(table->network_id);

	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + NIT_LEN;

	// Get bouquet descriptors length
	USHORT bouquetDescriptorsLength = HILO(table->network_descriptor_length);

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + NIT_LEN + bouquetDescriptorsLength;

	// In the beginning we don't know which bouquet this is
	string bouquetName;

	// For each bouquet descriptor do
	while(bouquetDescriptorsLength != 0)
	{
		// Get bouquet descriptor
		const descr_bouquet_name_t* const bouquetDescriptor = CastBouquetNameDescriptor(inputBuffer);

		switch(bouquetDescriptor->descriptor_tag)
		{
			case 0x47:
			{
				const char* beginName = (const char*)inputBuffer + DESCR_BOUQUET_NAME_LEN;
				UINT skipCounter = (*((const BYTE*)beginName) >= (BYTE)32 || bouquetDescriptor->descriptor_length == 0) ? 0 : 1;
				bouquetName = string(beginName + skipCounter, bouquetDescriptor->descriptor_length - skipCounter);
				log(4, true, m_pParent->getTunerOrdinal(), TEXT("### Found bouquet with name %s\n"), bouquetName.c_str());
				break;
			}
			default:
				log(4, true, m_pParent->getTunerOrdinal(), TEXT("!!! Unknown bouquet descriptor, type=%02X\n"), (UINT)bouquetDescriptor->descriptor_tag); 
				break;
		}
		// Adjust input buffer pointer
		inputBuffer += bouquetDescriptor->descriptor_length + DESCR_BOUQUET_NAME_LEN;

		// Adjust remaining length
		bouquetDescriptorsLength -= bouquetDescriptor->descriptor_length + DESCR_BOUQUET_NAME_LEN;
	}

	// Flags for supported "remote channel number tuning" providers
	bool isYES = (bouquetName == "Yes Bouquet 1");
	bool isFoxtel = bouquetID == 25191 || bouquetID == 25184;
	bool isSkyUK = (bouquetName == g_pConfiguration->getBouquetName());
	bool isSkyItalia = (bouquetName == "Live bouquet ID");
	bool isSkyNZ = (bouquetName.find("CA2 ") == 0 && !g_pConfiguration->getPreferSDOverHD() || bouquetName == g_pConfiguration->getBouquetName());

	// Do it only for supported providers
	if(isYES || isFoxtel || isSkyUK || isSkyItalia || isSkyNZ)
	{
		// First time flag
		bool firstTime = true;

		// Now we're at the beginning of the transport streams list
		const nit_mid_t* const transportStreams = (const nit_mid_t*)inputBuffer;

		// Update input buffer
		inputBuffer += SIZE_NIT_MID;

		// Get transport loop length
		USHORT transportLoopLength = HILO(transportStreams->transport_stream_loop_length);

		// Update remaining length, should be 0 here!
		remainingLength -= transportLoopLength + SIZE_NIT_MID;

		// Loop through transport streams
		while(transportLoopLength != 0)
		{
			// Get transport stream
			const nit_ts_t* const transportStream = (const nit_ts_t*)inputBuffer;

			// Get length of descriptors
			const USHORT transportDescriptorsLength = HILO(transportStream->transport_descriptors_length);

			// Get the ONID
			const USHORT onid = HILO(transportStream->original_network_id);

			// Get the TID
			const USHORT tid = HILO(transportStream->transport_stream_id);

			// Update inputBuffer
			inputBuffer += NIT_TS_LEN;
			
			// Length of remaining descriptors
			int remainingDescriptorsLength = transportDescriptorsLength;

			// Loop through descriptors
			while(remainingDescriptorsLength != 0)
			{
				// Get generic descriptor
				const descr_gen_t* const genericDescriptor = CastGenericDescriptor(inputBuffer);
				const USHORT descriptorLength = genericDescriptor->descriptor_length;

				if((isYES || isSkyNZ && g_pConfiguration->getPreferSDOverHD()) && genericDescriptor->descriptor_tag == (BYTE)0xE2/* ||
				   isFoxtel && genericDescriptor->descriptor_tag == (BYTE)0x93 && (!bouquetName.empty() || m_AustarDigitalDone)*/)
				{
					// This is how YES, Foxtel and SkyNZ (non-HD) keep their channel mapping
					const int len = descriptorLength / 4;
					for(int i = 0; i < len; i++)
					{
						// Get plain pointer
						const BYTE* pointer = inputBuffer + DESCR_GEN_LEN + i * 4;

						// Now get service ID
						const USHORT sid = ntohs(*(USHORT*)pointer);

						// And build the combined ONID&SID
						const UINT32 usid = NetworkProvider::getUniqueSID(onid, sid);

						// And then get channel number
						const USHORT channelNumber = ntohs(*(USHORT*)(pointer + 2));

						// See if service ID already initialized
						hash_map<UINT32, Service>::iterator it = m_Provider.m_Services.find(usid);
						if(channelNumber < 1000 && it != m_Provider.m_Services.end() &&	it->second.channelNumber == -1 &&
							m_Provider.m_Channels.find(channelNumber) == m_Provider.m_Channels.end())							// YES, Foxtel and Sky NZ disallow duplicate channel numbers
						{
							// Update timestamp
							time(&m_TimeStamp);

							// Prime channel number here
							Service& myService = it->second;
							myService.channelNumber = (short)channelNumber;

							// And make the opposite mapping too
							m_Provider.m_Channels[channelNumber] = usid;

							// Print log message
							if(firstTime)
							{
								log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found in Bouquet=\"%s\"\n"), bouquetName.c_str());
								firstTime = false;
							}

							// Print log message
							log(2, true, m_pParent->getTunerOrdinal(), TEXT("Mapped SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
									sid, onid, myService.tid, (USHORT)channelNumber, myService.serviceNames["eng"].c_str(), (USHORT)myService.serviceType, (USHORT)myService.runningStatus);
						}
					}
				}
				else if((isSkyUK || isSkyItalia || isSkyNZ || isFoxtel) && genericDescriptor->descriptor_tag == (BYTE)0xB1)
				{
					// This is how Sky UK, Sky Italia and Sky NZ (HD) keep their channel mapping
					const int len = (descriptorLength - 2) / 9;
					for(int i = 0; i < len; i++)
					{
						// Get plain pointer
						const BYTE* pointer = inputBuffer + DESCR_GEN_LEN + i * 9 + 2;

						// Boil out on bogus entries
						if(ntohs(*(USHORT*)(pointer + 7)) == (USHORT)0xFFFF)
							continue;

						// Now get service ID
						const USHORT sid = ntohs(*(USHORT*)pointer);

						// And build the combined ONID&SID
						const UINT32 usid = NetworkProvider::getUniqueSID(onid, sid);

						// Get the transponder data for this USID
						Transponder transponder;

						// And then get channel number
						const USHORT channelNumber = ntohs(*(USHORT*)(pointer + 5));

						// Bail out if weird data has been detected, including non-matching TIDs between the SDT and the BAT information, which could suggest the BAT
						// information is misinterpreted, as it's not formally well-defined
						if(sid == (USHORT)0xFFFF || sid == 0 || channelNumber == (USHORT)0xFFFF || channelNumber == 0 || !m_Provider.getTransponderForSid(usid, transponder) || transponder.tid != tid)
							continue;

						// See if service ID already initialized
						hash_map<UINT32, Service>::iterator it = m_Provider.m_Services.find(usid);
						if(it != m_Provider.m_Services.end() &&																		// If the service USID has already been found in the STD tables
							it->second.channelNumber == -1 &&																		// AND it hasn't been assigned the channel number yet
							(m_Provider.m_Channels.find(channelNumber) == m_Provider.m_Channels.end()								// AND this channel number hasn't been assigned yet
									|| it->second.tid == tid && g_pConfiguration->isPreferredTID(tid)) &&							// OR the current transponder is the preferred one
							!it->second.serviceNames["eng"].empty())																// AND the service has a non-empty English name
						{
							// Get the service
							Service& myService = it->second;

							// For Sky Italia and Sky NZ, allow override of duplicate channels only for HD channels on top of SD ones, if not requested otherwise
							if((isSkyItalia || isSkyNZ) && m_Provider.m_Channels.find(channelNumber) != m_Provider.m_Channels.end())
							{
								const UINT32 otherServiceUsid = m_Provider.m_Channels[channelNumber];
								if(otherServiceUsid != usid)
								{
									Service& otherService = m_Provider.m_Services[otherServiceUsid];
									if(myService.serviceType != otherService.serviceType)
										if(g_pConfiguration->getPreferSDOverHD() ^ (myService.serviceType != 25 && myService.serviceType != 17))			// 25 or 17 means the service has HD video
											continue;
										else
										{
											// Unmap the other service from this channel number
											otherService.channelNumber = -1;
											// Print log message
											log(2, true, m_pParent->getTunerOrdinal(), TEXT("Unmapped SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
												otherService.sid, otherService.onid, otherService.tid, (USHORT)channelNumber, otherService.serviceNames["eng"].c_str(), (USHORT)otherService.serviceType, (USHORT)otherService.runningStatus);
										}
								}
							}
							
							// Update timestamp
							time(&m_TimeStamp);

							// Prime channel number here
							myService.channelNumber = (short)channelNumber;

							// And make the opposite mapping too
							m_Provider.m_Channels[channelNumber] = usid;

							// Print log message
							if(firstTime)
							{
								log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found in Bouquet=\"%s\", BouquetID=%hu\n"), bouquetName.c_str(), bouquetID);
								firstTime = false;
							}

							// Print log message
							log(2, true, m_pParent->getTunerOrdinal(), TEXT("Mapped SID=%hu, ONID=%hu, TSID=%hu, Channel=%hu, Name=\"%s\", Type=%hu, Running Status=%hu\n"),
								sid, onid, myService.tid, (USHORT)channelNumber, myService.serviceNames["eng"].c_str(), (USHORT)myService.serviceType, (USHORT)myService.runningStatus);
						}
					}
				}

				// Adjust input Buffer
				inputBuffer += descriptorLength + DESCR_GEN_LEN;

				// Adjust remaining length
				remainingDescriptorsLength -= descriptorLength + DESCR_GEN_LEN;
			}

			// Adjust the length
			transportLoopLength -= transportDescriptorsLength + NIT_TS_LEN;
		}

		// Now we can indicate that Austar digital bouquet has been scanned
		m_AustarDigitalDone = m_AustarDigitalDone || (bouquetName == "Austar digital");
	}
}

void PSIParser::parseNITTable(const nit_t* const table,
							  short remainingLength)
{
	// If the provider info has already been copied, boil out
	if(m_ProviderInfoHasBeenCopied)
		return;

	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + NIT_LEN;

	// Get bouquet descriptors length
	USHORT networkDescriptorsLength = HILO(table->network_descriptor_length);

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + NIT_LEN + networkDescriptorsLength;

	// Get the current network NID
	if(m_CurrentONID == 0)
	{
		m_CurrentONID = HILO(table->network_id);
		log(2, true, m_pParent->getTunerOrdinal(), TEXT("Current network NID is %hu\n"), m_CurrentONID);
		m_Provider.m_DefaultONID = m_CurrentONID;
	}

	// In the beginning we don't know which bouquet this is
	string networkName;

	// For each network descriptor do
	while(networkDescriptorsLength != 0)
	{
		// Get network descriptor
		const descr_network_name_t* const networkDescriptor = CastNetworkNameDescriptor(inputBuffer);

		switch(networkDescriptor->descriptor_tag)
		{
			case 0x40:
			{
				networkName = string((const char*)inputBuffer + DESCR_NETWORK_NAME_LEN, networkDescriptor->descriptor_length);
				log(3, true, m_pParent->getTunerOrdinal(), TEXT("### Found network with name %s\n"), networkName.c_str());
				break;
			}
			default:
				log(4, true, m_pParent->getTunerOrdinal(), TEXT("!!! Unknown network descriptor, type=%02X\n"), (UINT)networkDescriptor->descriptor_tag);
				break;
		}
		
		// Adjust input buffer pointer
		inputBuffer += networkDescriptor->descriptor_length + DESCR_NETWORK_NAME_LEN;

		// Adjust remaining length
		networkDescriptorsLength -= networkDescriptor->descriptor_length + DESCR_NETWORK_NAME_LEN;
	}

	// Now we're at the beginning of the transport streams list
	const nit_mid_t* const transportStreams = (const nit_mid_t*)inputBuffer;

	// Update input buffer
	inputBuffer += SIZE_NIT_MID;

	// Get transport loop length
	USHORT transportLoopLength = HILO(transportStreams->transport_stream_loop_length);

	// Update remaining length, should be 0 here!
	remainingLength -= transportLoopLength + SIZE_NIT_MID;

	// Loop through transport streams
	while(transportLoopLength != 0)
	{
		// Get transport stream
		const nit_ts_t* const transportStream = (const nit_ts_t*)inputBuffer;

		// Get the ONID
		const USHORT onid = HILO(transportStream->original_network_id);

		// Get length of descriptors
		const USHORT transportDescriptorsLength = HILO(transportStream->transport_descriptors_length);

		// Get the TID
		USHORT tid = HILO(transportStream->transport_stream_id);

		// Update inputBuffer
		inputBuffer += NIT_TS_LEN;
		
		// Length of remaining descriptors
		short remainingDescriptorsLength = transportDescriptorsLength;

		// Skip if the transponder is excluded
		if(!g_pConfiguration->isExcludedTID(tid))
		{
			// Loop through descriptors
			while(remainingDescriptorsLength != 0)
			{
				// Get generic descriptor
				const descr_gen_t* const generalDescriptor = CastGenericDescriptor(inputBuffer);
				const USHORT descriptorLength = generalDescriptor->descriptor_length;

				switch(generalDescriptor->descriptor_tag)
				{
					case 0x43:
					{
						// This is a satellite transponder descriptor
						// Construct new transponder
						Transponder transponder;
						// Set its TID
						transponder.tid = tid;
						// And ONID
						transponder.onid = onid;
						// Get the combined ONID&TID
						const UINT32 utid = NetworkProvider::getUniqueSID(onid, tid);
						// Here we have satellite descriptor
						const descr_satellite_delivery_system_t* const satDescriptor = CastSatelliteDeliverySystemDescriptor(inputBuffer);
						// Get the frequency
						transponder.frequency = 10000000 * BcdCharToInt(satDescriptor->frequency1) + 100000 * BcdCharToInt(satDescriptor->frequency2) +
												1000 * BcdCharToInt(satDescriptor->frequency3) + 10 * BcdCharToInt(satDescriptor->frequency4);
						// Get the symbol rate
						transponder.symbolRate = 10000 * BcdCharToInt(satDescriptor->symbol_rate1) + 100 * BcdCharToInt(satDescriptor->symbol_rate2) +
												 BcdCharToInt(satDescriptor->symbol_rate3);
						// Get the modulation type
						transponder.modulation = getDVBSModulationFromDescriptor(satDescriptor->modulationsystem, satDescriptor->modulationtype);
						// Get the FEC rate
						transponder.fec = getFECFromDescriptor(satDescriptor->fec_inner);
						// Get the polarization
						transponder.polarization = getPolarizationFromDescriptor(satDescriptor->polarization);

						// Set the satellite information (if it hasn't already been set)
						UINT orbitalLocation = 1000 * BcdCharToInt(satDescriptor->orbital_position2) + 100 * BcdCharToInt(satDescriptor->orbital_position1) + 10 * BcdCharToInt(satDescriptor->orbital_position4) + BcdCharToInt(satDescriptor->orbital_position3);
						g_pSatelliteInfo->addOrUpdateSatellite(m_pParent->getTunerOrdinal(), onid, networkName, orbitalLocation, (satDescriptor->west_east_flag == 1) ? true : false);
						
						// Set the transponder info if not set yet
						hash_map<UINT32, Transponder>::iterator it = m_Provider.m_Transponders.find(utid);
						if(it == m_Provider.m_Transponders.end())
						{
							// Update timestamp
							time(&m_TimeStamp);

							// Prime transponder data
							m_Provider.m_Transponders[utid] = transponder;
							log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found transponder for ONID=%hu with TID=%d, Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s\n"),
								onid, tid, transponder.frequency, transponder.symbolRate, printablePolarization(transponder.polarization),
								printableModulation(transponder.modulation), printableFEC(transponder.fec));
						}
						break;
					}
					case 0x44:
					{
						// This is a cable transponder descriptor
						// Construct new transponder
						Transponder transponder;
						// Set its TID
						transponder.tid = tid;
						// And ONID
						transponder.onid = onid;
						// Get the combined ONID&TID
						const UINT32 utid = NetworkProvider::getUniqueSID(onid, tid);
						// Here we have satellite descriptor
						const descr_cable_delivery_system_t* const cabDescriptor = CastCableDeliverySystemDescriptor(inputBuffer);
						// Get the frequency
						transponder.frequency = 100000 * BcdCharToInt(cabDescriptor->frequency1) + 1000 * BcdCharToInt(cabDescriptor->frequency2) +
												10 * BcdCharToInt(cabDescriptor->frequency3) + 1 * BcdCharToInt(cabDescriptor->frequency4);
						// Get the symbol rate
						transponder.symbolRate = 10000 * BcdCharToInt(cabDescriptor->symbol_rate1) + 100 * BcdCharToInt(cabDescriptor->symbol_rate2) +
												 BcdCharToInt(cabDescriptor->symbol_rate3);
						// Get the modulation type
						transponder.modulation = getDVBCModulationFromDescriptor(cabDescriptor->modulation);
						// Get the FEC rate
						//transponder.fec = getFECFromDescriptor(cabDescriptor->fec_inner);
						
						// Set the transponder info if not set yet
						hash_map<UINT32, Transponder>::iterator it = m_Provider.m_Transponders.find(utid);
						if(it == m_Provider.m_Transponders.end())
						{
							// Update timestamp
							time(&m_TimeStamp);

							// Prime transponder data
							m_Provider.m_Transponders[utid] = transponder;
							log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found transponder for ONID=%hu with TID=%d, Frequency=%lu, Symbol Rate=%lu, Modulation=%s\n"),
								onid, tid, transponder.frequency, transponder.symbolRate, printableModulation(transponder.modulation));
						}
						break;
					}
					case 0x41:
						// Do nothing
						break;
					default:
						log(4, true, m_pParent->getTunerOrdinal(), TEXT("### Unknown transport stream descriptor with TAG=%02X and lenght=%d\n"),
										(UINT)generalDescriptor->descriptor_tag, descriptorLength);
						break;
				}

				// Adjust input Buffer
				inputBuffer += descriptorLength + DESCR_GEN_LEN;

				// Adjust remaining length
				remainingDescriptorsLength -= descriptorLength + DESCR_GEN_LEN;
			}
		}

		// Adjust the length
		transportLoopLength -= transportDescriptorsLength + NIT_TS_LEN;

		// Adjust the input buffer (should not change if TID not excluded)
		inputBuffer += remainingDescriptorsLength;
	}
}

void PSIParser::parseUnknownTable(const pat_t* const table,
								  const short remainingLength) const
{
	// Print diagnostics message
	log(4, true, m_pParent->getTunerOrdinal(), TEXT("$$$ Unknown table detected with TID=%02X, length=%u\n"), (UINT)table->table_id, remainingLength);
}

// Query methods
bool PSIParser::getPMTPidForSid(USHORT sid,
								USHORT& pmtPid) const
{
	hash_map<USHORT, USHORT>::const_iterator it = m_PMTPids.find(sid);
	if(it != m_PMTPids.end())
	{
		pmtPid = it->second;
		return true;
	}
	else
		return false;
}

bool PSIParser::getESPidsForSid(USHORT sid,
								hash_set<USHORT>& esPids) const
{
	hash_map<USHORT, hash_set<USHORT>>::const_iterator it = m_ESPidsForSid.find(sid);
	if(it != m_ESPidsForSid.end())
	{
		esPids = it->second;
		return true;
	}
	else
		return false;
}

bool PSIParser::getCAPidsForSid(USHORT sid,
								hash_set<USHORT>& caPids) const
{
	hash_map<USHORT, hash_set<USHORT>>::const_iterator it = m_CAPidsForSid.find(sid);
	if(it != m_CAPidsForSid.end())
	{
		caPids = it->second;
		return true;
	}
	else
		return false;
}

bool PSIParser::getECMCATypesForSid(USHORT sid,
									hash_set<CAScheme>& ecmCATypes) const
{
	hash_map<USHORT, hash_set<CAScheme>>::const_iterator it = m_CATypesForSid.find(sid);
	if(it != m_CATypesForSid.end())
	{
		ecmCATypes = it->second;
		return true;
	}
	else
		return false;
}

BYTE PSIParser::getTypeForPid(USHORT pid) const
{
	hash_map<USHORT, StreamInfo>::const_iterator it = m_PidStreamInfo.find(pid);
	return it != m_PidStreamInfo.end() ? it->second.m_StreamType : 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This is the worker thread main function waits for events
DWORD WINAPI parserWorkerThreadRoutine(LPVOID param)
{
	// Get the pointer to the parser from the parameter
	ESCAParser* const parser = (ESCAParser* const)param;
	while(!parser->m_ExitWorkerThread)
	{
		// Sleep for a while
		Sleep(100);

		// Decrypt and write any pending data
		parser->decryptAndWritePending(parser->m_ExitWorkerThread);
	}

	//Exit the thread
	return 0;
}


void ESCAParser::addPSIData(const BYTE* const data,
							USHORT length,
							bool topLevel,
							USHORT pid,
							BYTE& PSIContinuityCounter)
{
	// Let's see if we need to put some initial stuff into the TS packet
	if(m_PSIOutputBufferOffset == 0)
	{
		// Fill the remaining entries of the TS header
		ts_t* const tsHeader = (ts_t*)m_PSIOutputBuffer;
		// The continuity counter
		tsHeader->continuity_counter = PSIContinuityCounter;
		// Increment it for the next time
		PSIContinuityCounter = PSIContinuityCounter == 15 ? 0 : PSIContinuityCounter + 1;
		// Payload unit start indicator is set to 1 for the first time and then to 0
		tsHeader->payload_unit_start_indicator = m_PSIFirstTimeFlag;
		// Fill the PID
		SETHILO(tsHeader->PID, pid);
		// Set the output offset
		m_PSIOutputBufferOffset = TS_LEN;
		// See if this is the first TS packet for a PSI table
		if(m_PSIFirstTimeFlag)
		{
			// Put empty pointer if this is the first time
			m_PSIOutputBuffer[m_PSIOutputBufferOffset++] = 0;
			// First time PSI flag should be FALSE now
			m_PSIFirstTimeFlag = false;
		}
	}

	// Calculate the copy length
	USHORT copyLength = min(TS_PACKET_LEN - m_PSIOutputBufferOffset, length);

	// Copy the data
	memcpy(m_PSIOutputBuffer + m_PSIOutputBufferOffset, data, copyLength);

	// Increase the offset in the buffer
	m_PSIOutputBufferOffset += copyLength;

	// Check if the buffer is full
	if(m_PSIOutputBufferOffset == TS_PACKET_LEN)
	{
		// Send it to the output
		putToOutputBuffer(m_PSIOutputBuffer);
		// Reset the offset
		m_PSIOutputBufferOffset = 0;
		// Increase output packets counter
		m_PSIOutputPacketsCounter++;
	}

	// If there are any remainders, process them again
	if(copyLength < length)
		addPSIData(data + copyLength, length - copyLength, false, pid, PSIContinuityCounter);

	// See if we need to add the data to the section for future CRC calculation
	if(topLevel)
	{
		// Copy the data to the section
		memcpy(m_PSISection + m_PSISectionOffset, data, length);
		// Increase the offset
		m_PSISectionOffset += length;
	}
}

USHORT ESCAParser::finishPSI(USHORT pid,
							 BYTE& PSIContinuityCounter)
{
	// Get the CRC first
	UINT32 crc = (UINT32)htonl(_dvb_crc32(m_PSISection, m_PSISectionOffset));

	// Add it to the output, without adding to the section
	addPSIData((const BYTE* const)&crc, 4, false, pid, PSIContinuityCounter);

	// If there are any remainders
	if(m_PSIOutputBufferOffset != 0)
	{
		// Fill them to the full length
		memset(m_PSIOutputBuffer + m_PSIOutputBufferOffset, (BYTE)0xFF, TS_PACKET_LEN - m_PSIOutputBufferOffset);
		// Send to output
		putToOutputBuffer(m_PSIOutputBuffer);
		// Reset the offset
		m_PSIOutputBufferOffset = 0;
	}
	// Reset the PSI section offset
	m_PSISectionOffset = 0;
	// Reset the first time flag
	m_PSIFirstTimeFlag = true;
	// Save the previous value of the output reset counter
	USHORT result = m_PSIOutputPacketsCounter;
	// And reset the PSI output packets counter
	m_PSIOutputPacketsCounter = 0;
	// And return the total number of output packets
	return result;
}

// This is the top level DVB TS stream parsing routine, to be called on a generic TS buffer
void ESCAParser::parseTSPacket(const ts_t* const packet,
							   USHORT pid,
							   bool& abandonPacket)
{
	// First, see if the packet is encrypted and set the flag
	//m_IsEncrypted = (packet->transport_scrambling_control != 0);

	// This is the packet we're about to analyze
	BYTE currentPacket[TS_PACKET_LEN];
	
	// Copy the contents of the current input buffer
	memcpy_s(currentPacket, sizeof(currentPacket), (BYTE*)packet, TS_PACKET_LEN);

	// Write or skip PAT
	if(pid == 0)
	{
		// First, deal with dilution
		if(m_PATDilutionCounter++ != 0)
		{
			// If the dilution counter reached the maximum, make it 0
			if(m_PATDilutionCounter >= m_PATDilutionFactor)
				m_PATDilutionCounter = 0;
		}
		else
		{
			// Fill the PAT header
			pat_t patHeader;
			// Nullify it
			ZeroMemory((void*)&patHeader, PAT_LEN);
			// Syntax indicator is always 1
			patHeader.section_syntax_indicator = 1;
			// Current next indicator is 1 (this is the actual PAT)
			patHeader.current_next_indicator = 1;
			// The version number
			patHeader.version_number = m_PATVersion;
			// Increment it for the next time
			m_PATVersion = m_PATVersion == 31 ? 0 : m_PATVersion + 1;
			// Set the TSID
			SETHILO(patHeader.transport_stream_id, m_pParent->getCurrentTID());
			// Set the new length - should be 13 (5 the remainder of the header + 4 for the single PMT + 4 for CRC
			SETHILO(patHeader.section_length, 13);
			
			// Output the PAT header
			addPSIData((const BYTE* const)&patHeader, PAT_LEN, true, pid, m_PATContinuityCounter);

			// Get the SID in network order
			USHORT nsid = htons(m_Sid);
			// And output it
			addPSIData((const BYTE* const)&nsid, 2, true, pid, m_PATContinuityCounter);

			// Get the PMT PID in network order
			USHORT nPMTPid = htons(m_PmtPid);
			// And output it
			addPSIData((const BYTE* const)&nPMTPid, 2, true, pid, m_PATContinuityCounter);

			// Adjust the PAT dilution factor
			m_PATDilutionFactor = max(finishPSI(pid, m_PATContinuityCounter), m_PATDilutionFactor);
		}
	}

	// Write or skip PMT depending on dilution
	if(pid == m_PmtPid)
	{
		// First, deal with dilution
		if(m_PMTDilutionCounter++ != 0 && !m_pParent->isPMTChanged(m_Sid))
		{
			// If the dilution counter reached the maximum, make it 0
			if(m_PMTDilutionCounter >= m_PMTDilutionFactor)
				m_PMTDilutionCounter = 0;
		}
		else
		{
			// Get the list of all ES PIDs for this SID
			hash_set<USHORT> esPids;
			m_pParent->getESPidsForSid(m_Sid, esPids);
			// Remove the PAT PID
			esPids.erase(0);
			// Remove the PMT PID
			esPids.erase(m_PmtPid);

			// Build the right PID order for this SID
			vector<USHORT> esPidsOrdered;
			int lastVideoIndex = -1;
			int lastPreferredAudioIndex = -1;
			int lastAudioIndex = -1;
			int lastSubtitleIndex = -1;
			for(hash_set<USHORT>::const_iterator it = esPids.begin(); it != esPids.end(); it++)
			{
				// Get the StreamInfo for this PID
				const StreamInfo& info = m_pParent->getStreamInfoForPid(*it);
				// Find all language descriptors for this stream
				hash_set<string> languages;
				// Various flags
				bool hasAC3AudioDescriptor = false;
				bool hasTeletextDescriptor = false;
				bool hasSubtitlingDescriptor = false;
				for(list<pair<BYTE, BYTE*>>::const_iterator it1 = info.m_Descriptors.begin(); it1 != info.m_Descriptors.end(); it1++)
				{
					// Get the descriptor header
					const descr_gen_t* const genericDescriptor = CastGenericDescriptor(it1->second);
					// Take care of different descriptors
					switch(genericDescriptor->descriptor_tag)
					{
						case (BYTE)0x0A :
						{
							// Get the language descriptor
							const descr_iso_639_language_t* const languageDescriptor = CastIso639LanguageDescriptor(it1->second);
							// Get the language
							string language;
							language += (char)languageDescriptor->lang_code1;
							language += (char)languageDescriptor->lang_code2;
							language += (char)languageDescriptor->lang_code3;
							// Add it to the list of languages for this stream
							languages.insert(language);
							break;
						}
						case (BYTE)0x6A:
							hasAC3AudioDescriptor = true;
							break;
						case (BYTE)0x56:
						{
							hasTeletextDescriptor = true;
							// Get the language descriptor
							const descr_iso_639_language_t* const languageDescriptor = CastIso639LanguageDescriptor(it1->second);
							// Get the language
							string language;
							language += (char)languageDescriptor->lang_code1;
							language += (char)languageDescriptor->lang_code2;
							language += (char)languageDescriptor->lang_code3;
							// Add it to the list of languages for this stream
							languages.insert(language);
							break;
						}
						case (BYTE)0x59:
						{
							hasSubtitlingDescriptor = true;
							// Get the language descriptor
							const descr_iso_639_language_t* const languageDescriptor = CastIso639LanguageDescriptor(it1->second);
							// Get the language
							string language;
							language += (char)languageDescriptor->lang_code1;
							language += (char)languageDescriptor->lang_code2;
							language += (char)languageDescriptor->lang_code3;
							// Add it to the list of languages for this stream
							languages.insert(language);
							break;
						}
						default:
							break;
					}
				}
				// Take care of the MPEG2 video streams
				if(info.m_StreamType == 1 || info.m_StreamType == 2)
				{
					// Set the last video index
					lastVideoIndex++;
					// Increment the last preferred audio, audio and subtitle indexes if needed
					if(lastPreferredAudioIndex != -1)
						lastPreferredAudioIndex++;
					if(lastAudioIndex != -1)
						lastAudioIndex++;
					if(lastSubtitleIndex != -1)
						lastSubtitleIndex++;
					// Put it in front of the list
					esPidsOrdered.insert(esPidsOrdered.begin(), *it);
				}
				// Take care of the audio streams
				else if(info.m_StreamType == 3 || info.m_StreamType == 4 || (info.m_StreamType == 6 && hasAC3AudioDescriptor))
				{
					// See if the audio language matches the preferred one
					if(languages.count(g_pConfiguration->getPreferredAudioLanguage()) > 0)
					{
						// Set the last preferred audio index
						lastPreferredAudioIndex = (lastPreferredAudioIndex == -1 ? lastVideoIndex : lastPreferredAudioIndex) + 1;
						// Increment the last audio and subtitle indexes if needed
						if(lastAudioIndex != -1)
							lastAudioIndex++;
						if(lastSubtitleIndex != -1)
							lastSubtitleIndex++;
						// Now check if the audio is AC3 and it is also preferred
						if(hasAC3AudioDescriptor && g_pConfiguration->getPreferredAudioFormat() == "ac3")
							// If yes, put it in front of the list
							esPidsOrdered.insert(esPidsOrdered.begin() + (lastVideoIndex + 1), *it);
						// Now check if the audio is not AC3 and it is preferred this way
						else if(!hasAC3AudioDescriptor && g_pConfiguration->getPreferredAudioFormat() != "ac3")
							// If yes, put it in front of the list
							esPidsOrdered.insert(esPidsOrdered.begin() + (lastVideoIndex + 1), *it);
						else
							// All other preferred audio goes here
							esPidsOrdered.insert(esPidsOrdered.begin() + lastPreferredAudioIndex, *it);
					}
					else
					{
						// Set the last audio index
						lastAudioIndex = (lastAudioIndex == -1 ? (lastPreferredAudioIndex == -1 ? lastVideoIndex : lastPreferredAudioIndex) : lastAudioIndex) + 1;
						// Increment the last subtitle index if needed
						if(lastSubtitleIndex != -1)
							lastSubtitleIndex++;

						// See if the audio is of the preferred format
						if((hasAC3AudioDescriptor && g_pConfiguration->getPreferredAudioFormat() == "ac3") || 
							(!hasAC3AudioDescriptor && g_pConfiguration->getPreferredAudioFormat() != "ac3"))
							// If yes, insert it right after the preferred language
							esPidsOrdered.insert(esPidsOrdered.begin() + ((lastPreferredAudioIndex == -1 ? lastVideoIndex : lastPreferredAudioIndex ) + 1), *it);
						else
							// All the rest of the audio streams go to the new audio index
							esPidsOrdered.insert(esPidsOrdered.begin() + lastAudioIndex, *it);
					}
				}
				// Take care of the subtitles streams
				else if(info.m_StreamType == 6 && (hasTeletextDescriptor || hasSubtitlingDescriptor))
				{
					// Set the last subtitles index
					lastSubtitleIndex = (lastSubtitleIndex == -1 ? (lastAudioIndex == -1 ? (lastPreferredAudioIndex == -1 ? lastVideoIndex : lastPreferredAudioIndex) : lastAudioIndex) : lastSubtitleIndex) + 1;
						
					// If the subtitle language matches the preferred one, put it at the front of the list
					if(languages.count(g_pConfiguration->getPreferredSubtitlesLanguage()) > 0)
						esPidsOrdered.insert(esPidsOrdered.begin() + ((lastAudioIndex == -1 ? (lastPreferredAudioIndex == -1 ? lastVideoIndex : lastPreferredAudioIndex) : lastAudioIndex) + 1), *it);
					else
						// All the rest of the subtitle streams go to the new subtitle index
						esPidsOrdered.insert(esPidsOrdered.begin() + lastSubtitleIndex, *it);
				}
				else
					// All the rest of the streams go to the back of the list
					esPidsOrdered.push_back(*it);
			}

			// Get the program info from the parent
			const ProgramInfo& programInfo = m_pParent->getProgramInfoForSid(m_Sid);

			// Now, we start filling in the PMT table
			// Fill the PMT header
			pmt_t pmtHeader;
			// Nullify it
			ZeroMemory((void*)&pmtHeader, PMT_LEN);
			// Syntax indicator is always 1
			pmtHeader.section_syntax_indicator = 1;
			// Fill the program number (== SID)
			SETHILO(pmtHeader.program_number, m_Sid);
			// Table ID for PMT is 0x02
			pmtHeader.table_id = 2;
			// Current next indicator is 1 (this is the actual PMT)
			pmtHeader.current_next_indicator = 1;
			// The version number
			pmtHeader.version_number = m_PMTVersion;
			// Increment it for the next time
			m_PMTVersion = m_PMTVersion == 31 ? 0 : m_PMTVersion + 1;
			// Fill the program info length
			SETHILO(pmtHeader.program_info_length, programInfo.m_DescriptorsLength);
			// Fill the PCR PID
			SETHILO(pmtHeader.PCR_PID, programInfo.m_PCRPid);

			// Compute the total section length
			USHORT sectionLength = 13 + programInfo.m_DescriptorsLength;
			// Add descriptors length for every PID of this program
			for(hash_set<USHORT>::const_iterator it = esPids.begin(); it != esPids.end(); it++)
				sectionLength += PMT_INFO_LEN + m_pParent->getStreamInfoForPid(*it).m_DescriptorsLength;
			SETHILO(pmtHeader.section_length, sectionLength);

			// Output the PMT header
			addPSIData((const BYTE* const)&pmtHeader, PMT_LEN, true, pid, m_PMTContinuityCounter);

			// Iterate through program descriptors
			for(list<pair<BYTE, BYTE*>>::const_iterator it = programInfo.m_Descriptors.begin(); it != programInfo.m_Descriptors.end(); it++)
				addPSIData(it->second, (USHORT)it->first, true, pid, m_PMTContinuityCounter);

			// Iterate through the streams in the right order
			for(UINT i = 0; i < esPidsOrdered.size(); i++)
			{
				// Get stream info for the current stream
				const StreamInfo& streamInfo = m_pParent->getStreamInfoForPid(esPidsOrdered[i]);

				// Fill stream info structure
				pmt_info_t pmtInfo;
				// Nullify it
				ZeroMemory((void*)&pmtInfo, PMT_INFO_LEN);
				// Copy the stream type
				pmtInfo.stream_type = streamInfo.m_StreamType;
				// Set the PID
				SETHILO(pmtInfo.elementary_PID, esPidsOrdered[i]);
				// Set the descriptors length
				SETHILO(pmtInfo.ES_info_length, streamInfo.m_DescriptorsLength);

				// Output the stream info header
				addPSIData((const BYTE* const)&pmtInfo, PMT_INFO_LEN, true, pid, m_PMTContinuityCounter);

				// Now, iterate through the descriptors of this stream
				for(list<pair<BYTE, BYTE*>>::const_iterator it = streamInfo.m_Descriptors.begin(); it != streamInfo.m_Descriptors.end(); it++)
					addPSIData(it->second, (USHORT)it->first, true, pid, m_PMTContinuityCounter);
			}

			// Mark the end of PMT section and adjust the PMT dilution factor
			m_PMTDilutionFactor = max(finishPSI(pid, m_PMTContinuityCounter), m_PMTDilutionFactor);

			// Mark the PMT for this SID as unchanged
			m_pParent->clearPMTChanged(m_Sid);
		}
	}

	// If this is an ES PID
	if(m_IsESPid[pid])
	{
		// See if we already had a valid packet
		if(!m_ValidPacketFound[pid])
			// If no and the packet is not a beginning of a new packet, boil out
			if(!packet->payload_unit_start_indicator)
				return;
			else
				// Otherwise set the flag
				m_ValidPacketFound[pid] = true;
		// Put the packet to the output queue
		putToOutputBuffer(currentPacket);
	}
	else if(pid != 0)
		// Otherwise send it to CAM, if it's not PAT
		sendToCam(currentPacket, pid);
}

void ESCAParser::sendToCam(const BYTE* const currentPacket,
						   USHORT caPid)
{
	// Do this only if have plugins handler
	if(m_pPluginsHandler != NULL)
	{
		// EMM/PMT and CAT packets go directly to the plugins
		if(m_EMMCATypes.hasPid(caPid))
			// Handle EMM packets
			m_pPluginsHandler->putCAPacket(this, TYPE_EMM, m_ECMCATypes, m_EMMCATypes, m_Sid, m_ChannelName, caPid, m_PmtPid, currentPacket + 4);
		else if(caPid == m_PmtPid)
			// Handle PMT packets
			m_pPluginsHandler->putCAPacket(this, TYPE_PMT, m_ECMCATypes, m_EMMCATypes, m_Sid, m_ChannelName, caPid, m_PmtPid, currentPacket + 4);
		else if(caPid == 1)
			// Handle CAT packets
			m_pPluginsHandler->putCAPacket(this, TYPE_CAT, m_ECMCATypes, m_EMMCATypes, m_Sid, m_ChannelName, caPid, m_PmtPid, currentPacket + 4);
		// Now, we have only ECM packets left
		// For ECM packets, see if the content is new
		else if(memcmp(m_LastECMPacket, currentPacket + 4, min((size_t)currentPacket[7] + 4, sizeof(m_LastECMPacket))) != 0)
		{
			// If yes, save it
			memcpy_s(m_LastECMPacket, sizeof(m_LastECMPacket), currentPacket + 4, (size_t)currentPacket[7] + 4);

			// Lock the output buffers queue
			m_csOutputBuffer.Lock();

			// Get the last packet in the queue
			OutputBuffer* const lastBuffer = m_OutputBuffers.back();

			// If the last buffer is empty, we don't need a new one
			if(lastBuffer->numberOfPackets != 0)
			{
				// Log how many packets were before the new buffer has been added
				log(3, true, getTunerOrdinal(), TEXT("ECM packet received when %lu packets resided in the output buffer\n"), lastBuffer->numberOfPackets);

				// Otherwise, create a new output buffer
				OutputBuffer* const newBuffer = new OutputBuffer;

				// Copy both keys from the previously last buffer
				memcpy_s(newBuffer->evenKey, sizeof(newBuffer->evenKey), lastBuffer->evenKey, sizeof(newBuffer->evenKey));
				memcpy_s(newBuffer->oddKey, sizeof(newBuffer->oddKey), lastBuffer->oddKey, sizeof(newBuffer->oddKey));
				
				// Put the new buffer at the end of the queue
				m_OutputBuffers.push_back(newBuffer);
			}
			else
			{
				// Log the fact no packets were in the output buffer when an ECM packet has been received
				log(3, true, getTunerOrdinal(), TEXT("ECM packet received when the output buffer was epmty\n"));

				// Also, make sure the keys are reset
				lastBuffer->hasKey = false;
				lastBuffer->keyVerified = false;
			}

			// Unlock the output buffers queue
			m_csOutputBuffer.Unlock();


			// And send the packet to the plugins
			m_pPluginsHandler->putCAPacket(this, TYPE_ECM, m_ECMCATypes, m_EMMCATypes, m_Sid, m_ChannelName, caPid, m_PmtPid, currentPacket + 4);
		}
	}
}

bool ESCAParser::setKey(bool isOddKey,
						const BYTE* const key,
						bool setWithNoCheck)
{
	// Output buffer manipulations in the critical section
	CAutoLock lock(&m_csOutputBuffer);

	// The key matched at least one of the buffers
	bool foundMatchingBuffer = false;

	// Let's find where we need to set the key
	for(deque<OutputBuffer* const>::iterator it = m_OutputBuffers.begin(); it != m_OutputBuffers.end();)
	{
		// Get the buffer from the queue
		OutputBuffer* const currentBuffer = *it;

		// Try to use the key only if the buffer doesn't have one
		// If the buffer has a key, even an unverified one, leave it alone, it might verify itself later
		// But we do want to do this for the last empty buffer
		if(!currentBuffer->hasKey || it + 1 == m_OutputBuffers.end())
		{
			// Let's see if the key can decrypt the current buffer
			KeyCorrectness keyCorrectness = setWithNoCheck ? KEY_OK : isCorrectKey(currentBuffer->buffer, currentBuffer->numberOfPackets, key, isOddKey, key, !isOddKey);

			// First, deal with keys known as wrong ones
			if(keyCorrectness == KEY_WRONG)
				// If this is not the last buffer, delete it
				if(it + 1 != m_OutputBuffers.end())
				{
					// Delete the buffer from the queue
					it = m_OutputBuffers.erase(it);

					// Log the fact we dropped some packets
					log(2, true, getTunerOrdinal(), TEXT("Key doesn't match, dropped %lu packets\n"), currentBuffer->numberOfPackets);

					// And delete the buffer itself
					delete currentBuffer;
				}
				else
				{
					// We get here if the key was wrong, but it was the last buffer
					// Log the fact the key didn't match the last available buffer
					log(2, true, getTunerOrdinal(), TEXT("Key doesn't match, maybe the next one will...\n"));

					// The buffer doesn't have a key and it was not verified
					currentBuffer->keyVerified = false;
					currentBuffer->hasKey  = false;

					// Move the iterator one step forward
					it++;
				}
			else
			{
				// We get here if the key COULD be correct
				// Copy the key
				memcpy_s(isOddKey ? currentBuffer->oddKey : currentBuffer->evenKey, sizeof(currentBuffer->oddKey), key, sizeof(currentBuffer->oddKey));

				// Now this buffer has its key
				currentBuffer->hasKey = true;

				// Whether it's verified or not depends on the certainty of the key check
				currentBuffer->keyVerified = (keyCorrectness == KEY_OK);
				if(currentBuffer->keyVerified)
				{
					// Buffer key has been verified
					log(3, true, getTunerOrdinal(), TEXT("Buffer with length %lu, key verified immediately...\n"), currentBuffer->numberOfPackets);
					// Also, verify all non-verified buffers prior that one
					for(deque<OutputBuffer* const>::iterator it1 = m_OutputBuffers.begin(); *it1 != currentBuffer; it1++)
						if((*it1)->hasKey)
							(*it1)->keyVerified = true;
				}
				else
					// Buffer key will be verified later
					log(3, true, getTunerOrdinal(), TEXT("Buffer with length %lu, key will be verified with delay...\n"), currentBuffer->numberOfPackets);

				// Let's see if we need to update a key to its successor
				if(it + 1 != m_OutputBuffers.end())
				{
					// Get the next buffer
					OutputBuffer* const nextBuffer = *(it + 1);

					// Copy the key
					memcpy_s(isOddKey ? nextBuffer->oddKey : nextBuffer->evenKey, sizeof(nextBuffer->oddKey), key, sizeof(nextBuffer->oddKey));
				}

				// Indicate we found and least one matching buffer
				foundMatchingBuffer = true;

				// Move the iterator one step forward
				it++;
			}
		}
		else
			// If the buffer already has a key, just move the iterator one step forward
			it++;
	}

	return foundMatchingBuffer;
}

void ESCAParser::reset()
{
	// We have to do this within critical section
	CAutoLock lock(&m_csOutputBuffer);

	// Do reset only if passed the threshold
	if(++m_ResetCounter >= g_pConfiguration->getMaxNumberOfResets())
	{
		// Delete all remaining output buffers
		while(!m_OutputBuffers.empty())
		{
			delete m_OutputBuffers.front();
			m_OutputBuffers.pop_front();
		}
	}

	// Add a single buffer to the end of the output queue
	m_OutputBuffers.push_back(new OutputBuffer);
	
	// Fix the PSI auxiliary variables
	m_PMTContinuityCounter = 0;
	m_PMTVersion = 0;
	m_PMTDilutionCounter = 0;
	m_PATContinuityCounter = 0;
	m_PATVersion = 0;
	m_PATDilutionCounter = 0;
	m_PSIOutputBufferOffset = 0;
	m_PSIFirstTimeFlag = true;
	m_PSISectionOffset = 0;
	m_PSIOutputPacketsCounter = 0;
}

// This is the only public constructor
ESCAParser::ESCAParser(DVBParser* const pParent,
					   Recorder* const pRecorder,
					   FILE* const pFile,
					   PluginsHandler* const pPluginsHandler,
					   LPCTSTR channelName,
					   USHORT sid,
					   USHORT pmtPid,
					   const hash_set<CAScheme>& ecmCATypes,
					   const EMMInfo& emmCATypes,
					   __int64 maxFileLength) :
	TSPacketParser(pParent),
	m_WorkerThread(NULL),
	m_pOutFile(pFile),
	m_pPluginsHandler(pPluginsHandler),
	m_ChannelName(channelName),
	m_Sid(sid),
	m_PmtPid(pmtPid),
	m_ECMCATypes(ecmCATypes),
	m_EMMCATypes(emmCATypes),
	m_ExitWorkerThread(false),
	m_pRecorder(pRecorder),
	m_IsEncrypted(!ecmCATypes.empty()),
	m_FileLength((__int64)0),
	m_MaxFileLength(maxFileLength),
	m_CurrentPosition(0),
	m_ResetCounter(0),
	m_PMTContinuityCounter(0),
	m_PMTVersion(0),
	m_PMTDilutionCounter(0),
	m_PMTDilutionFactor(g_pConfiguration->getPMTDilutionFactor()),
	m_PATContinuityCounter(0),
	m_PATVersion(0),
	m_PATDilutionCounter(0),
	m_PATDilutionFactor(g_pConfiguration->getPATDilutionFactor()),
	m_PSIOutputBufferOffset(0),
	m_PSIFirstTimeFlag(true),
	m_PSISectionOffset(0),
	m_PSIOutputPacketsCounter(0)
{
	// Make sure last ECM packet is zeroed out
	ZeroMemory(m_LastECMPacket, sizeof(m_LastECMPacket));

	// Add a single buffer to the end of the output queue
	m_OutputBuffers.push_back(new OutputBuffer);

	// Initialize the PMT output buffer
	// Nullify the TS header
	ZeroMemory(m_PSIOutputBuffer, TS_LEN);
	// Fill the common TS header fields
	ts_t* tsHeader = (ts_t*)m_PSIOutputBuffer;
	// Sync byte is always 0x47
	tsHeader->sync_byte = (BYTE)0x47;
	// There is no adaptation field
	tsHeader->adaptation_field_control = 1;

	// Create worker thread
	m_WorkerThread = CreateThread(NULL, 0, parserWorkerThreadRoutine, this, 0, NULL);
}

// Destructor of the parser
ESCAParser::~ESCAParser()
{
	// Shutdown the worker thread 
	// Set the flag
	m_ExitWorkerThread = true;

	// Wait for the thread to finish
	WaitForSingleObject(m_WorkerThread, INFINITE);

	// Delete all remaining output buffers
	while(!m_OutputBuffers.empty())
	{
		delete m_OutputBuffers.front();
		m_OutputBuffers.pop_front();
	}

	// Finally, close the worker thread handle
	CloseHandle(m_WorkerThread);
}

// This function is called from the main filter thread!
void ESCAParser::putToOutputBuffer(const BYTE* const packet)
{
	// Do it in the critical section
	CAutoLock lock(&m_csOutputBuffer);

	// Get the last buffer from the queue
	OutputBuffer* const currentBuffer = m_OutputBuffers.back();

	// This really shouldn't happen!
	if(currentBuffer->numberOfPackets >= g_pConfiguration->getTSPacketsPerOutputBuffer())
		log(0, true, getTunerOrdinal(), TEXT("Too many packets for decryption!\n"));
	else
	{
		// Copy the packet to the output buffer
		memcpy(currentBuffer->buffer + currentBuffer->numberOfPackets * TS_PACKET_LEN, packet, TS_PACKET_LEN);

		// Increment the packet counter
		currentBuffer->numberOfPackets++;

		// If the current buffer has a key but it hasn't been verified yet, try to verify it
		if(currentBuffer->hasKey && !currentBuffer->keyVerified)
		{
			switch(isCorrectKey(packet, 1, currentBuffer->oddKey, true, currentBuffer->evenKey, true))
			{
				case KEY_OK:
					// Key verified
					currentBuffer->keyVerified = true;
					// Buffer key has been verified
					log(3, true, getTunerOrdinal(), TEXT("Buffer key verified with delay...\n"));
					// Verify all the previous ones post factum
					for(deque<OutputBuffer* const>::iterator it = m_OutputBuffers.begin(); *it != currentBuffer;it++)
						(*it)->keyVerified = true;
					break;
				case KEY_WRONG:
					// Key wrong, invalidate it!
					currentBuffer->hasKey = false;
					currentBuffer->keyVerified = false;
					log(2, true, getTunerOrdinal(), TEXT("Wrong key has been assigned to the buffer, invalidating...\n"));
					break;
				default:
					break;
			}
		}
	}
}

// This function is called from the worker thread!
void ESCAParser::decryptAndWritePending(bool immediately)
{
	// Do it within critical section
	CAutoLock lock(&m_csOutputBuffer);

	// For all packets in the output queue
	while(!m_OutputBuffers.empty())
	{
		// Get the current output buffer
		OutputBuffer* const currentBuffer = m_OutputBuffers.front();

		// Let's see if we can write its contents
		if((!m_IsEncrypted || currentBuffer->keyVerified) && (m_OutputBuffers.size() > 1 || currentBuffer->numberOfPackets >= g_pConfiguration->getTSPacketsOutputThreshold() || immediately))
		{
			// We can zero the reset counter
			m_ResetCounter = 0;

			// Set the decrypter keys
			m_Decrypter.setKeys(currentBuffer->oddKey, currentBuffer->evenKey);

			// Log how many packets are about to be written
			log(3, true, getTunerOrdinal(), TEXT("Writing %d pending packets..."), currentBuffer->numberOfPackets);
			
			// Decrypt multiple packets
			m_Decrypter.decrypt(currentBuffer->buffer, currentBuffer->numberOfPackets);
		
			// And write them to the file
			int writtenBytes = write(currentBuffer->buffer, currentBuffer->numberOfPackets * TS_PACKET_LEN);
			if(writtenBytes > 0)
				m_FileLength = m_FileLength + writtenBytes;
			if(writtenBytes != (int)(currentBuffer->numberOfPackets * TS_PACKET_LEN))
				// Notify recorder about broken pipe if necessary
				m_pRecorder->setBrokenPipe();

			// Now all the packets are written
			log(3, false, 0, TEXT("Done!\n"));

			// Let's see if we already have newer buffer
			if(m_OutputBuffers.size() > 1)
			{
				// Remove the output buffer from the head of the queue
				m_OutputBuffers.pop_front();

				// And delete the buffer object
				delete currentBuffer;
			}
			else
			{
				// Make the number of packets back to zero
				currentBuffer->numberOfPackets = 0;

				// And break out
				break;
			}
		}
		else
			// Do nothing and boil out
			break;
	}
}

int ESCAParser::write(const BYTE* const buffer,
					  int bytesToWrite)
{
	int writtenBytes = 0;
	if(m_MaxFileLength == (__int64)-1)
		writtenBytes = fwrite(buffer, 1, bytesToWrite, m_pOutFile);
	else
	{
		while(m_CurrentPosition + bytesToWrite > m_MaxFileLength)
		{
			int writeNow = (int)(m_MaxFileLength - m_CurrentPosition);
			int writtenNow = fwrite(buffer + writtenBytes, 1, writeNow, m_pOutFile);
			writtenBytes += writtenNow;
			bytesToWrite -= writtenNow;
			m_CurrentPosition = 0;
			fseek(m_pOutFile, 0, SEEK_SET);
			if(writtenNow != writeNow)
				return writtenBytes;
		}
		int writtenNow = fwrite(buffer + writtenBytes, 1, bytesToWrite, m_pOutFile);
		m_CurrentPosition += writtenNow;
		writtenBytes += writtenNow;
	}
	return writtenBytes;
}

void ESCAParser::setESPid(USHORT pid,
						  bool isESPid)
{
	// Set the flag for the PID
	m_IsESPid[pid] = isESPid;
	// Right now no packets have been written for this PID
	m_ValidPacketFound[pid] = false;
}

bool ESCAParser::matchAudioLanguage(const BYTE* const buffer,
									const int bufferLength,
									const char* language)
{
	int langLen = strlen(language);
	for(int i = 0; i <= bufferLength - langLen; i++)
	{
		bool stop = false;
		for(int j = 0; !stop && j < langLen; j++)
			stop = (buffer[i + j] != language[j]);
		if(!stop)
			return true;
	}
	return false;
}

KeyCorrectness ESCAParser::isCorrectKey(const BYTE* const buffer,
										ULONG numberOfPackets,
										const BYTE* const oddKey,
										bool checkAgainstOddKey,
										const BYTE* const evenKey,
										bool checkAgainstEvenKey)
{
	// If configured to always trust keys from the plugin just accept that it is OK
	if(g_pConfiguration->trustKeys() == true)
		return KEY_OK;

	log(4, true, getTunerOrdinal(), TEXT("Trying to match the key against %lu packets\n"), numberOfPackets);

	// See if the keys are initialized
	static const BYTE nullKey[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	bool oddKeyInitialized = (memcmp(oddKey, nullKey, sizeof(nullKey)) != 0);
	bool evenKeyInitialized = (memcmp(evenKey, nullKey, sizeof(nullKey)) != 0);

	// Flag indicating any PES packets have been found
	bool anyPESPacketsFound = false;

	// Set the decrypter keys
	m_Decrypter.setKeys(oddKey, evenKey);

	// Go through all the packets
	for(ULONG i = 0; i < numberOfPackets; i++)
	{
		// Get the packet header
		const ts_t* const packet = (const ts_t* const)(buffer + i * TS_PACKET_LEN);
		// Let's see if we need to try to decrypt it, meaning:
		// 1. The packet is the beginning of the payload unit
		// 2. It is encrypted (this already implies the packet is not PMT or PAT)
		// 3. It contains not only adaptation field
		// 4. It doesn't have an error
		if(m_ShouldBeValidated[HILO(packet->PID)] &&
		   packet->payload_unit_start_indicator &&
		   packet->adaptation_field_control != 2 &&
		   !packet->transport_error_indicator &&
		   (evenKeyInitialized && checkAgainstEvenKey && packet->transport_scrambling_control == 2 ||
		    oddKeyInitialized &&  checkAgainstOddKey &&  packet->transport_scrambling_control == 3 ))
		{
			// There is at least one PES packet in the buffer
			anyPESPacketsFound = true;

			// Copy the packet in question aside
			BYTE copyPacket[TS_PACKET_LEN];
			memcpy_s(copyPacket, sizeof(copyPacket), (void*)packet, sizeof(copyPacket));

			// Decrypt the packet
			m_Decrypter.decrypt(copyPacket, 1);

			// Calculate the offset of the PES packet header
			const ULONG offset = TS_LEN + ((packet->adaptation_field_control == 3) ? (ULONG)(copyPacket[TS_LEN] + 1) : 0);

			// Log the offset value
			log(4, true, getTunerOrdinal(), TEXT("Offset = %lu\n"), offset);

			USHORT pid = HILO(packet->PID);

			// Now let's see if we have a valid PES packet header
			if(copyPacket[offset] == 0 && copyPacket[offset + 1] == 0 && copyPacket[offset + 2] == 1)
			{
				log(3, true, getTunerOrdinal(), TEXT("Discovered a packet with PID=0x%hX matching the key, expected %s key\n"),
					pid, (packet->transport_scrambling_control & 1) ? TEXT("ODD") : TEXT("EVEN"));
				// If yes, we know for sure we have the right key
				return KEY_OK;
			}

			// If no, it doesn't mean anything, we might just have a wrong parity key
			// (the packet is odd and the key is even or vise versa)
			
			// Log message
			log(3, true, getTunerOrdinal(), TEXT("Discovered a packet with PID=0x%hX mismatching the key, expected %s key\n"),
				pid, (packet->transport_scrambling_control & 1) ? TEXT("ODD") : TEXT("EVEN"));
		}
	}

	// If no PES packets have been in the buffer, the key still might be OK, otherwise the key is wrong
	return anyPESPacketsFound ? KEY_WRONG : KEY_MAYBE_OK;
}

int ESCAParser::getTunerOrdinal() const
{
	return m_pRecorder->getSource()->getSourceOrdinal();
}

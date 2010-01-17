#include "StdAfx.h"

#include "DVBParser.h"
#include "crc32.h"
#include "Logger.h"
#include "Recorder.h"
#include "configuration.h"
#include "extern.h"
#include "GenericSource.h"

// Define constants
#define	CRC_LENGTH			4
#define TABLE_PREFIX_LEN	3

// Our message for communications
#define WM_ALL_COMMUNICATIONS	WM_USER + 1

#define SKYNZ_ONID			169
#define FOXTEL_ONID			4096
#define SKYITALIA_ONID		64511

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

// This is the top level DVB TS stream parsing routine, to be called on a generic TS buffer
void DVBParser::parseTSStream(const BYTE* inputBuffer,
							  int inputBufferLength)
{
	// Lock the parser
	lock();

	// Dump the buffur into the file if needed
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
			log(2, true, m_TunerOrdinal, TEXT("Catastrophic error - TS packet has wrong sync_byte, dropping the buffer...\n"));
			break;
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
	m_PidType.clear();
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

		// Do all the stuff below only if CRC matches
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
			else if(table->table_id == (BYTE)'\x40')
				parseNITTable((const nit_t* const)table, (short)tableLength);	// Parse the NIT table
			// Or if this is a CAT table
			else if(table->table_id == (BYTE)'\x01')
				parseCATTable((const cat_t* const)table, (short)tableLength);	// Parse the CAT table
			// Of it is everything else
			else
				parseUnknownTable(table, (short)tableLength);					// Parse the unknown table
		}
		else
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

	// Also boil out if PMT for this SID has already been parsed
	if(m_ESPidsForSid.find(programNumber) != m_ESPidsForSid.end())
		return;

	// Log warning message if EMMPid is still 0
	if(m_EMMPids.empty())
		log(2, true, m_pParent->getTunerOrdinal(), TEXT("Warning: no EMM PIDs have been discovered after %hu PMT packets, EMM data will not be passed to plugins!!!\n"), g_pConfiguration->getPMTThreshold());

	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + PMT_LEN;

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + PMT_LEN;

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
		if(descriptor->descriptor_tag == '\x09')
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
			if(caDescriptor->descriptor_length > DESCR_CA_LEN - 2 /*-2 desc+len*/)
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

		// Go to the next record
		programInfoLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
		remainingLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
		inputBuffer += descriptor->descriptor_length + DESCR_GEN_LEN;
	}
	
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

		// Save the PID type
		m_PidType[ESPid] = streamInfo->stream_type;

		// Log the ES type
		log(2, true, m_pParent->getTunerOrdinal(), TEXT("PID=%hu(0x%hX) has Type=0x%.02hX\n"), ESPid, ESPid, (USHORT)streamInfo->stream_type);

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
			if(descriptor->descriptor_tag == '\x09')
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
				if(caDescriptor->descriptor_length > DESCR_CA_LEN - 2 /*-2 desc+len*/)
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
			
			// Insert the new service into the map
			if(newService.runningStatus != 0 && (onid != FOXTEL_ONID && onid != SKYNZ_ONID && onid != SKYITALIA_ONID || newService.serviceType < 0x80))
				m_Provider.m_Services[usid] = newService;
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
	bool isFoxtel = (bouquetName.empty() || bouquetName == "Austar digital");
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

				if((isYES || isSkyNZ && g_pConfiguration->getPreferSDOverHD()) && genericDescriptor->descriptor_tag == (BYTE)0xE2 ||
				   isFoxtel && genericDescriptor->descriptor_tag == (BYTE)0x93 && (!bouquetName.empty() || m_AustarDigitalDone))
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
				else if((isSkyUK || isSkyItalia || isSkyNZ) && genericDescriptor->descriptor_tag == (BYTE)0xB1)
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

						// And then get channel number
						const USHORT channelNumber = ntohs(*(USHORT*)(pointer + 5));

						if(sid == (USHORT)0xFFFF || sid == 0 || channelNumber == (USHORT)0xFFFF || channelNumber == 0)
							continue;

						// See if service ID already initialized
						hash_map<UINT32, Service>::iterator it = m_Provider.m_Services.find(usid);
						if(it != m_Provider.m_Services.end() && it->second.channelNumber == -1 && !it->second.serviceNames["eng"].empty())
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
										if(g_pConfiguration->getPreferSDOverHD() ^  (myService.serviceType != 25))
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
								log(2, true, m_pParent->getTunerOrdinal(), TEXT("Found in Bouquet=\"%s\"\n"), bouquetName.c_str());
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
				log(4, true, m_pParent->getTunerOrdinal(), TEXT("### Found network with name %s\n"), networkName.c_str());
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
					transponder.modulation = getModulationFromDescriptor(satDescriptor->modulationsystem, satDescriptor->modulationtype);
					// Get the FEC rate
					transponder.fec = getFECFromDescriptor(satDescriptor->fec_inner);
					// Get the polarization
					transponder.polarization = getPolarizationFromDescriptor(satDescriptor->polarization);
					
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

		// Adjust the length
		transportLoopLength -= transportDescriptorsLength + NIT_TS_LEN;
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
	hash_map<USHORT, BYTE>::const_iterator it = m_PidType.find(pid);
	return it != m_PidType.end() ? it->second : 0;
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

	// Skip or fix PAT
	if(pid == 0)
	{
		// Find the pointer byte - should be the first one after the TS header
		BYTE pointer = currentPacket[TS_LEN];
		// Now, find the beginning of the PAT table
		pat_t* const pat = (pat_t* const)(currentPacket + TS_LEN + 1 + pointer);
		// Now get the previous length - we need this to stuff the PAT with FF
		USHORT prevPatLength = HILO(pat->section_length);
		// Set the new length - should be 13 (5 the remainder of the header + 4 for the single PMT + 4 for CRC
		pat->section_length_hi = 0;
		pat->section_length_lo = 13;
		// Stuff the contents of the PAT with FF
		memset((BYTE*)pat + PAT_LEN, '\xFF', min(prevPatLength - 5, TS_PACKET_LEN - PAT_LEN - pointer - 1 - TS_LEN));
		// Get the pointer to the new SID
		USHORT* pSid = (USHORT*)((BYTE*)pat + PAT_LEN);
		// Get the pointer to the new PMT PID
		USHORT* ppmtPid = (USHORT*)((BYTE*)pat + PAT_LEN + 2);
		// Put them there after host-to-network translation
		*pSid = htons(m_Sid);
		*ppmtPid = htons(m_PmtPid);
		// Calculate the new CRC and put it there
		*(UINT32*)((BYTE*)pat + 12) = (UINT32)htonl(_dvb_crc32((const BYTE*)pat, 12));
		// Pat is fixed!
	}

	// Skip or fix PMT (always skip long PMTs)
	if(pid == m_PmtPid && !g_pConfiguration->getDontFixPMT() && packet->payload_unit_start_indicator)
	{
		// Copy the packet
		BYTE copyPacket[TS_PACKET_LEN];
		memcpy_s(copyPacket, sizeof(copyPacket), currentPacket, TS_PACKET_LEN);

		// Find the pointer byte - should be the first one after the TS header
		BYTE pointer = currentPacket[TS_LEN];

		// Let's check the pointer isn't bogus, if yes - escape!
		if(TS_LEN + 1 + pointer + PMT_LEN >= TS_PACKET_LEN)
		{
			log(3, true, getTunerOrdinal(), TEXT("Bogus PMT encountered, skipping...\n"));
			return;
		}

		// Now, find the beginning of the PMT table
		pmt_t* const pmt = (pmt_t* const)(copyPacket + TS_LEN + 1 + pointer);

		// And get the remaining length of the table
		int remainingLength = HILO(pmt->section_length);

		// Do the internal part only if the rest of the section is within the same packet and if the CRC matches
		if(remainingLength > 0 && remainingLength + TS_LEN + 1 + pointer - 1 < TS_PACKET_LEN)
			if((UINT32)htonl(_dvb_crc32(currentPacket + TS_LEN + 1 + pointer, HILO(pmt->section_length) - 1)) == *(UINT32*)((const BYTE*)pmt + remainingLength - 1))
			{
				// Adjust input buffer pointer
				const BYTE* inputBuffer = (const BYTE*)pmt + PMT_LEN;

				// Remove CRC and prefix
				remainingLength -= CRC_LENGTH + PMT_LEN - 3;

				// Skip program info, if present
				const USHORT programInfoLength = HILO(pmt->program_info_length);
				inputBuffer += programInfoLength;
				remainingLength -= programInfoLength;

				// Save the inputBuffer and the remaining length
				const BYTE* const saveInputBuffer = inputBuffer;
				const int saveRemainingLength = remainingLength;

				// Adjust the output buffer pointer
				BYTE* outputBuffer = currentPacket + (saveInputBuffer - copyPacket);

				// Loop through the stream 4 times
				for(int i = 0; i < 4; i++)
				{
					// Restore the input buffer and the remaining length
					inputBuffer = saveInputBuffer;
					remainingLength = saveRemainingLength;

					// For each stream descriptor do
					while(remainingLength != 0)
					{
						// Get stream info
						const pmt_info_t* const streamInfo = (const pmt_info_t* const)inputBuffer;

						// Get ES info length
						USHORT ESInfoLength = HILO(streamInfo->ES_info_length);

						// Copy flag is initially false
						bool copy = false;

						// Now, depending on the pass
						switch(i)
						{
							// We take care of the video first
							case 0:
								copy = (streamInfo->stream_type == 2 || streamInfo->stream_type == 1 || streamInfo->stream_type == 0x1B);
								break;
							// Here we take care of the audio in the right language
							case 1:
								copy = ((streamInfo->stream_type == 4 || streamInfo->stream_type == 3) && 
												matchAudioLanguage(inputBuffer + PMT_INFO_LEN,
												ESInfoLength,
												g_pConfiguration->getPrefferedAudioLanguage()));
								break;
							// Here we take care of all other audio streams
							case 2:
								copy = ((streamInfo->stream_type == 4 || streamInfo->stream_type == 3) && 
												!matchAudioLanguage(inputBuffer + PMT_INFO_LEN,
												ESInfoLength,
												g_pConfiguration->getPrefferedAudioLanguage()));
								break;
							// Here we take care of all other streams
							default:
								copy = (streamInfo->stream_type != 1 && streamInfo->stream_type != 2 &&
										streamInfo->stream_type != 3 && streamInfo->stream_type != 4);
								break;
						}

						// If needed, copy the info
						if(copy)
						{
							memcpy(outputBuffer, inputBuffer, PMT_INFO_LEN + ESInfoLength);
							outputBuffer += PMT_INFO_LEN + ESInfoLength;
						}

						// Go to next ES info
						inputBuffer += PMT_INFO_LEN + ESInfoLength;
						remainingLength -= PMT_INFO_LEN + ESInfoLength;
					}
				}
				// Calculate the new CRC and put it there
				*(UINT32*)outputBuffer = (UINT32)htonl(_dvb_crc32(currentPacket + TS_LEN + 1 + pointer, HILO(pmt->section_length) - 1));
			}
			else
			{
				log(3, true, getTunerOrdinal(), TEXT("Bogus PMT CRC, ignoring...\n"));
				return;
			}
		else if(m_firstTimePMTFixMessage)
		{
			log(2, true, getTunerOrdinal(), TEXT("Long PMT table encountered while trying to fix it, ignoring...\n"));
			m_firstTimePMTFixMessage = false;
		}

	}

	// Send PMT to CAM as well
	if(pid == m_PmtPid)
		sendToCam(currentPacket, pid);

	// For all other ES PIDs
	if(pid != 0 && pid != m_PmtPid && m_IsESPid[pid] && !m_ValidPacketFound[pid])
		if(!packet->payload_unit_start_indicator)
			return;
		else
			m_ValidPacketFound[pid] = true;
	
	// If this is an ES PID
	if(m_IsESPid[pid])
		// Put the packet to the output queue
		putToOutputBuffer(currentPacket);
	else
		// Otherwise send it to CAM
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
		else if(memcmp(m_LastECMPacket, currentPacket + 4, (size_t)currentPacket[7] + 4) != 0)
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
	m_firstTimePMTFixMessage = true;
}

// This is the only public constructor
ESCAParser::ESCAParser(Recorder* const pRecorder,
					   FILE* const pFile,
					   PluginsHandler* const pPluginsHandler,
					   LPCTSTR channelName,
					   USHORT sid,
					   USHORT pmtPid,
					   const hash_set<CAScheme>& ecmCATypes,
					   const EMMInfo& emmCATypes,
					   __int64 maxFileLength) :
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
	m_firstTimePMTFixMessage(true)
{
	// Make sure last ECM packet is zeroed out
	ZeroMemory(m_LastECMPacket, sizeof(m_LastECMPacket));

	// Add a single buffer to the end of the output queue
	m_OutputBuffers.push_back(new OutputBuffer);

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

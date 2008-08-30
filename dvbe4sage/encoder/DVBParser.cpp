#include "StdAfx.h"

#include "DVBParser.h"
#include "crc32.h"
#include "Logger.h"
#include "Recorder.h"
#include "configuration.h"

// Define constants
#define	CRC_LENGTH			4
#define TABLE_PREFIX_LEN	3

// Our message for communications
#define WM_ALL_COMMUNICATIONS	WM_USER + 1

// Lock function with logging
void DVBParser::lock()
{
	g_Logger.log(3, true, TEXT("Locking the parser\n"));
	m_cs.Lock();
}

void DVBParser::unlock()
{
	g_Logger.log(3, true, TEXT("Unlocking the parser\n"));
	m_cs.Unlock();
}

// This is the top level DVB TS stream parsing routine, to be called on a generic TS buffer
void DVBParser::parseTSStream(const BYTE* inputBuffer,
							  int inputBufferLength)
{
	// Lock the parser
	lock();

	// Get the TS header
	while(inputBufferLength > 0)
	{
		// First, take care of leftovers
		if(inputBufferLength < TS_PACKET_LEN)
		{
			// Copy it to a special buffer
			memcpy(m_LeftoverBuffer, inputBuffer, inputBufferLength);
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
			memcpy(currentPacket, m_LeftoverBuffer, m_LeftoverLength);
			// Adjust destination index
			copyIndex = m_LeftoverLength;
			// Adjust length to be copied
			copyLength = TS_PACKET_LEN - m_LeftoverLength;
			// Zero the leftover length
			m_LeftoverLength = 0;
		}

		// Copy the contents of the current input buffer
		memcpy(currentPacket + copyIndex, inputBuffer, copyLength);

		// Get its header
		const ts_t* const header = (const ts_t* const)currentPacket;
		
		// Sanity check - the sync byte!
		if(header->sync_byte != '\x47')
		{	
			g_Logger.log(0, true, TEXT("Catastrophic error - TS packet has wrong sync_byte!\n"));
			break;
		}

		// Handle the packet only if it doesn't have an error
		if(header->transport_error_indicator)
			g_Logger.log(2, true, TEXT("Got an erroneous packet, skipping!\n"));
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
	// Pass the internal PSI parser to PID 0x10 (NIT)
	//assignParserToPid(0x10, &m_PSIParser);
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
	m_Transponders.clear();
	m_Services.clear();
	m_Channels.clear();
	m_PMTPids.clear();
	m_ESPidsForSid.clear();
	m_CAPidsForSid.clear();
	m_BufferForPid.clear();
	m_CurrentTid = 0;
	m_AllowParsing = true;
}

// This routine converts PSI TS packets to readable PSI tables
void PSIParser::parseTSPacket(const ts_t* const packet,
							  USHORT pid,
							  bool& abandonPacket)
{
	// If parsing is disallowed, boil out
	if(!m_AllowParsing)
		return;

	// Make sure we don't miss the adaptation field!
	if(packet->adaptation_field_control != 1)
		g_Logger.log(2, true, TEXT("Strange adaptation field control encountered\n"));

	// If this is the first time we encountered this PID, create a new entry for it
	if(m_BufferForPid.find(pid) == m_BufferForPid.end())
	{
		SectionBuffer sectionBuffer;
		m_BufferForPid[pid] = sectionBuffer;
	}

	// Get the relevant table
	SectionBuffer& sectionBuffer = m_BufferForPid[pid];

	// If duplicate buffer, skip it
	if(sectionBuffer.lastContinuityCounter == packet->continuity_counter)
	{
		g_Logger.log(2, true, TEXT("Got duplicate packet...\n"));
		return;
	}

	// Update countinuity counter
	sectionBuffer.lastContinuityCounter = (BYTE)packet->continuity_counter;

	// Set the starting address
	const BYTE* inputBuffer = (BYTE*)packet + TS_LEN;
	// And the remaining length
	short remainingLength = TS_PACKET_LEN - TS_LEN;

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
				g_Logger.log(2, true, TEXT("TS packet messed up, fixing...\n"));
				sectionBuffer.offset = 0;
				sectionBuffer.expectedLength = 0;
			}
			else
			{
				// Append the rest of the section
				memcpy(&sectionBuffer.buffer[sectionBuffer.offset], inputBuffer, sectionBuffer.expectedLength);
				// Adjust the offset
				sectionBuffer.offset += sectionBuffer.expectedLength;
				// Adjust the expected length
				sectionBuffer.expectedLength = 0;
				// Pass it to further parsing
				parseTable((const pat_t* const)sectionBuffer.buffer, sectionBuffer.offset, abandonPacket);
				// Adjust the offset once again
				sectionBuffer.offset = 0;
			}
		}
		
		// Adjust the input buffer
		inputBuffer += pointer;
		// And the remaining length
		remainingLength -= pointer;

		// Now loop through the rest of the packet
		while(remainingLength != 0 && (*inputBuffer) != (BYTE)'\xFF')
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
				memcpy(sectionBuffer.buffer, (BYTE*)pat, remainingLength);
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
	}
	else
		// Take care of a packet without new section
		if(sectionBuffer.offset != 0 && sectionBuffer.expectedLength != 0)
		{
			// Calculate the length of the data we need to copy
			const USHORT lengthToCopy = min(remainingLength, sectionBuffer.expectedLength);
			// Copy the data to the buffer
			memcpy(&sectionBuffer.buffer[sectionBuffer.offset], inputBuffer, lengthToCopy);
			// Adjust the offset
			sectionBuffer.offset += lengthToCopy;
			// And the expected length
			sectionBuffer.expectedLength -= lengthToCopy;
			// Let's see if we can pass the buffer to further processing
			if(sectionBuffer.expectedLength == 0)
			{
				// Parse the section
				parseTable((const pat_t* const)sectionBuffer.buffer, sectionBuffer.offset, abandonPacket);
				// Reset the offset
				sectionBuffer.offset = 0;
				// Make sure the remainder is just stuffing '\xFF' bytes
				for(int i = lengthToCopy; i < remainingLength; i++)
					if(inputBuffer[i] != (BYTE)'\xFF' && inputBuffer[i] != (BYTE)'\0')
						g_Logger.log(2, true, TEXT("Invalid byte 0x%.02X at offset %d\n"), (UINT)inputBuffer[i], 188 - remainingLength + i);
			}
		}
}

// This is the top level DVB SI stream parsing routine, to be called on a generic SI buffer
void PSIParser::parseTable(const pat_t* const table,
						   int remainingLength,
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
		const UINT tableLength = HILO(table->section_length) + TABLE_PREFIX_LEN;

		// Get the CRC
		const UINT crc = ntohl(*(const UINT*)(inputBuffer + tableLength - CRC_LENGTH));

		// Do all the stuff below only if CRC matches
		if(crc == _dvb_crc32(inputBuffer, tableLength - CRC_LENGTH))
		{
			// If this is a PAT table
			if(table->table_id == (BYTE)'\0')
				parsePATTable(table, tableLength, abandonPacket);		// Parse PAT table
			//Or if this is a SDT table
			else if(table->table_id == (BYTE)'\x42' || table->table_id == (BYTE)'\x46')
				parseSDTTable((const sdt_t* const)table, tableLength);	// Parse the SDT table
			// Or if this is a BAT table
			else if(table->table_id == (BYTE)'\x4A')
				parseBATTable((const nit_t* const)table, tableLength);	// Parse the BAT table
			// Or if this is a PMT table
			else if(table->table_id == (BYTE)'\x02')
				parsePMTTable((const pmt_t* const)table, tableLength);	// Parse the PMT table
			// Or if this is a NIT table
			else if(table->table_id == (BYTE)'\x40')
				parseNITTable((const nit_t* const)table, tableLength);	// Parse the NIT table
			// Of it is everything else
			else
				parseUnknownTable(table, tableLength);					// Parse the unknown table
		}
		else
			g_Logger.log(2, true, TEXT("!!! CRC error!\n"));

		// Amend input buffer pointer
		inputBuffer += tableLength;

		// Amend input buffer length
		remainingLength -= tableLength;
	}
}

void PSIParser::parsePMTTable(const pmt_t* const table,
							  int remainingLength)
{
	// Get the for this PMT SID
	USHORT programNumber = HILO(table->program_number);

	// Boil out if PMT for this SID has already been parsed
	if(m_ESPidsForSid.find(programNumber) != m_ESPidsForSid.end())
		return;

	// Make sure both ES and CA maps don't have an entry for this SID
	ASSERT(m_CAPidsForSid.find(programNumber) == m_CAPidsForSid.end());

	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + PMT_LEN;

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + PMT_LEN;

	// Skip program info, if present
	USHORT programInfoLength = HILO(table->program_info_length);
	inputBuffer += programInfoLength;
	remainingLength -= programInfoLength;

	// Initialize PID vectors
	hash_set<USHORT> pids;
	m_ESPidsForSid[programNumber] = pids;
	m_CAPidsForSid[programNumber] = pids;

	// For each stream descriptor do
	while(remainingLength != 0)
	{
		// Get stream info
		const pmt_info_t* const streamInfo = (const pmt_info_t* const)inputBuffer;

		// Get the stream PID
		USHORT ESPid = HILO(streamInfo->elementary_PID);

		// Add this elementary stream information to the set
		m_ESPidsForSid[programNumber].insert(ESPid);

		// Get ES info length
		USHORT ESInfoLength = HILO(streamInfo->ES_info_length);

		// Update the pointer and the length
		inputBuffer += PMT_INFO_LEN;
		remainingLength -= PMT_INFO_LEN;

		// Loop through the ES info records (we look for the CA descriptors)
		while(ESInfoLength != 0)
		{
			const descr_gen_t* const descriptor = CastGenericDescriptor(inputBuffer);
			if(descriptor->descriptor_tag == '\x09')
			{
				const descr_ca_t* const caDescriptor = CastCaDescriptor(inputBuffer);
				// Get the CA type
				USHORT caType = HILO(caDescriptor->CA_type);
				// +++++++++++++++++ Make sure it's NDS Videoguard
				if(caType != 0x90C && caType != 0x90D)
					g_Logger.log(0, true, TEXT("Wrong CA type %hX!\n"), caType);
				// Add the ECM pid
				m_CAPidsForSid[programNumber].insert(HILO(caDescriptor->CA_PID));
				// Add hardcoded EMM PID for YES
				m_CAPidsForSid[programNumber].insert(g_Configuration.getEMMPid());
			}

			// Go to the next record
			ESInfoLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
			remainingLength -= descriptor->descriptor_length + DESCR_GEN_LEN;
			inputBuffer += descriptor->descriptor_length + DESCR_GEN_LEN;
		}
	}
}

// PAT table parsing routine
void PSIParser::parsePATTable(const pat_t* const table,
							  int remainingLength,
							  bool& abandonPacket)
{
	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + PAT_LEN;

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + PAT_LEN;

	// Get this transponder TSID
	USHORT tid = HILO(table->transport_stream_id);

	// Update stuff only if transponder changed
	if(tid == m_CurrentTid)
		return;

	// Indicate no further processing for the packet is necessary
	abandonPacket = true;

	// Tell the parent to clean its PID  map
	m_pParent->resetParser(false);

	// Update current TID
	m_CurrentTid = tid;
	
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
							  int remainingLength)
{
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
		const USHORT serviceID = HILO(serviceDescriptor->service_id);

		// Get the length of descriptors loop
		const int descriptorsLoopLength = HILO(serviceDescriptor->descriptors_loop_length);

		// Only if encountered a new service
		if(m_Services.find(serviceID) == m_Services.end())
		{
			// We have a new service, lets initiate its fields as needed
			Service newService;
			newService.TSID = HILO(table->transport_stream_id);
			newService.ONID = HILO(table->original_network_id);
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

						// We don't know the channel number yet
						newService.channelNumber = 0;

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
					case 0x5F:
					case 0xCC:
						// I currently don't know what to do with these codes
						break;
					default:
						g_Logger.log(2, true, TEXT("!!! Unknown Service Descriptor, type=%02X\n"),
										(UINT)genericDescriptor->descriptor_tag); 
						break;
				}

				// Adjust current descriptor pointer
				currentDescriptor += genericDescriptor->descriptor_length + DESCR_GEN_LEN;

				// Adjust the remaining loop length
				descriptorLoopRemainingLength -= genericDescriptor->descriptor_length + DESCR_GEN_LEN;
			}
			
			// Insert the new service to the map
			m_Services[serviceID] = newService;
		}

		// Adjust input buffer pointer
		inputBuffer += descriptorsLoopLength + SDT_DESCR_LEN;

		// Adjust remaining length
		remainingLength -= descriptorsLoopLength + SDT_DESCR_LEN;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine parses BATs (bouquet association tables) which are usually transmitted on PID 0x11 of the
// transport stream. YES has several bouquets transmitted of which the interesting one is "Yes Bouquet 1".
// This bouquet includes definition of all services mapped to channels on the remote. Since the service IDs are
// the same as in SDTs, we can record those previously and here we can complete this information with the
// actual channel number and output channels according to the following criteria:
//	1. Their running status is 4 (running).
//	2. Their type is 1 (video), 2 (audio) or 25 (HD video). We filter out interactive services for the time being.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PSIParser::parseBATTable(const nit_t* const table,
							  int remainingLength)
{
	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + NIT_LEN;

	// Get bouquet descriptors length
	int bouquetDescriptorsLength = HILO(table->network_descriptor_length);

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
				bouquetName = string((const char*)inputBuffer + DESCR_BOUQUET_NAME_LEN, bouquetDescriptor->descriptor_length);
				g_Logger.log(3, true, TEXT("### Found bouquet with name %s\n"), bouquetName.c_str());
				break;
			}
			case 0x5C:
				break;
			default:
				g_Logger.log(2, true, TEXT("!!! Unknown bouquet descriptor, type=%02X\n"), (UINT)bouquetDescriptor->descriptor_tag); 
				break;
		}
		// Adjust input buffer pointer
		inputBuffer += bouquetDescriptor->descriptor_length + DESCR_BOUQUET_NAME_LEN;

		// Adjust remaining length
		bouquetDescriptorsLength -= bouquetDescriptor->descriptor_length + DESCR_BOUQUET_NAME_LEN;
	}

	// Do it only for "Yes Bouquet 1" bouquet
	if(bouquetName == "Yes Bouquet 1")
	{
		// Now we're at the beginning of the transport streams list
		const nit_mid_t* const transportStreams = (const nit_mid_t*)inputBuffer;

		// Update input buffer
		inputBuffer += SIZE_NIT_MID;

		// Get transport loop length
		int transportLoopLength = HILO(transportStreams->transport_stream_loop_length);

		// Update remaining length, should be 0 here!
		remainingLength -= transportLoopLength + SIZE_NIT_MID;

		// Loop through transport streams
		while(transportLoopLength != 0)
		{
			// Get transport stream
			const nit_ts_t* const transportStream = (const nit_ts_t*)inputBuffer;

			// Get length of descriptors
			const int transportDescriptorsLength = HILO(transportStream->transport_descriptors_length);

			// Update inputBuffer
			inputBuffer += NIT_TS_LEN;
			
			// Length of remaining descriptors
			int remainingDescriptorsLength = transportDescriptorsLength;

			// Loop through descriptors
			while(remainingDescriptorsLength != 0)
			{
				// Get generic descriptor
				const descr_gen_t* const generalDescriptor = CastGenericDescriptor(inputBuffer);
				const int descriptorLength = generalDescriptor->descriptor_length;

				switch(generalDescriptor->descriptor_tag)
				{
					case 0xE2:
					{
						// This is where YES keeps channel mapping
						const int len = descriptorLength / 4;
						for(int i = 0; i < len; i++)
						{
							// Get plain pointer
							const BYTE* pointer = inputBuffer + DESCR_GEN_LEN + i * 4;

							// Now get service ID
							USHORT serviceID = ntohs(*(USHORT*)pointer);

							// And then get channel number
							USHORT channelNumber = ntohs(*(USHORT*)(pointer + 2));

							// See if service ID already initialized
							hash_map<USHORT, Service>::iterator it = m_Services.find(serviceID);
							if(it != m_Services.end() && it->second.channelNumber == 0)
							{
								// Prime channel number here
								Service& myService = it->second;
								myService.channelNumber = channelNumber;

								// And make the opposite mapping too
								m_Channels[channelNumber] = serviceID;
							}
						}

						break;
					}
					case 0x41:
					case 0xE4:
					case 0x5F:
						// Do nothing
						break;
					default:
						g_Logger.log(2, true, TEXT("### Unknown transport stream descriptor with TAG=%02X and lenght=%d\n"),
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
}

void PSIParser::parseNITTable(const nit_t* const table,
							  int remainingLength)
{
	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + NIT_LEN;

	// Get bouquet descriptors length
	int networkDescriptorsLength = HILO(table->network_descriptor_length);

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + NIT_LEN + networkDescriptorsLength;

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
				g_Logger.log(3, true, TEXT("### Found network with name %s\n"), networkName.c_str());
				break;
			}
			default:
				g_Logger.log(2, true, TEXT("!!! Unknown network descriptor, type=%02X\n"), (UINT)networkDescriptor->descriptor_tag);
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
	int transportLoopLength = HILO(transportStreams->transport_stream_loop_length);

	// Update remaining length, should be 0 here!
	remainingLength -= transportLoopLength + SIZE_NIT_MID;

	// Loop through transport streams
	while(transportLoopLength != 0)
	{
		// Get transport stream
		const nit_ts_t* const transportStream = (const nit_ts_t*)inputBuffer;

		// Get length of descriptors
		const int transportDescriptorsLength = HILO(transportStream->transport_descriptors_length);

		// Get the TID
		USHORT tid = HILO(transportStream->transport_stream_id);

		// Update inputBuffer
		inputBuffer += NIT_TS_LEN;
		
		// Length of remaining descriptors
		int remainingDescriptorsLength = transportDescriptorsLength;

		// Loop through descriptors
		while(remainingDescriptorsLength != 0)
		{
			// Get generic descriptor
			const descr_gen_t* const generalDescriptor = CastGenericDescriptor(inputBuffer);
			const int descriptorLength = generalDescriptor->descriptor_length;

			switch(generalDescriptor->descriptor_tag)
			{
				case 0x43:
				{
					// Construct new transponder
					Transponder transponder;
					// Set its TID
					transponder.tid = tid;
					// Here we have satellite descriptor
					const descr_satellite_delivery_system_t* const satDescriptor = CastSatelliteDeliverySystemDescriptor(inputBuffer);
					// Get the frequency
					transponder.frequency = 10000000 * BcdCharToInt(satDescriptor->frequency1) + 100000 * BcdCharToInt(satDescriptor->frequency2) +
											1000 * BcdCharToInt(satDescriptor->frequency3) + 10 * BcdCharToInt(satDescriptor->frequency4);
					// Get the symbol rate
					transponder.symbolRate = 10000 * BcdCharToInt(satDescriptor->symbol_rate1) + 100 * BcdCharToInt(satDescriptor->symbol_rate2) +
											 BcdCharToInt(satDescriptor->symbol_rate3);
					// Get the modulation type
					transponder.modulation = satDescriptor->modulationtype == 1 ? BDA_MOD_QPSK : BDA_MOD_8VSB;
					// Get the FEC rate
					transponder.fec = satDescriptor->fec_inner == 3 ? BDA_BCC_RATE_3_4 : BDA_BCC_RATE_2_3;
					// Get the polarization
					transponder.polarization = satDescriptor->polarization == 0 ? BDA_POLARISATION_LINEAR_H : BDA_POLARISATION_LINEAR_V;
					
					// Set the transponder info if not set yet
					hash_map<USHORT, Transponder>::iterator it = m_Transponders.find(tid);
					if(it == m_Transponders.end())
					{
						m_Transponders[tid] = transponder;
						g_Logger.log(2, true, TEXT("Found transponder with TID=%d, Frequency=%lu, Symbol Rate=%lu, Polarization=%c, Modulation=%s, FEC=%s\n"),
							tid, transponder.frequency, transponder.symbolRate, transponder.polarization == BDA_POLARISATION_LINEAR_H ? CHAR('H') : CHAR('V'),
							transponder.modulation == BDA_MOD_QPSK ? TEXT("QPSK") : TEXT("8PSK"), transponder.fec == BDA_BCC_RATE_3_4 ? TEXT("3/4") : TEXT("2/3"));
					}
					break;
				}
				case 0x41:
					// Do nothing
					break;
				default:
					g_Logger.log(2, true, TEXT("### Unknown transport stream descriptor with TAG=%02X and lenght=%d\n"),
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
								  const int remainingLength) const
{
	// Print diagnostics message
	g_Logger.log(3, true, TEXT("$$$ Unknown table detected with TID=%02X, length=%u\n"), (UINT)table->table_id, remainingLength);
}

// Query methods
bool PSIParser::getPMTPidForSid(USHORT sid,
								USHORT& pmtPid)
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
								hash_set<USHORT>& esPids)
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
								hash_set<USHORT>& caPids)
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

bool PSIParser::getSidForChannel(USHORT channel,
								 USHORT& sid)
{
	hash_map<USHORT, USHORT>::const_iterator it = m_Channels.find(channel);
	if(it != m_Channels.end())
	{
		sid = it->second;
		return true;
	}
	else
		return false;
}

bool PSIParser::getTransponderForSid(USHORT sid,
									 Transponder& transponder)
{
	hash_map<USHORT, Service>::const_iterator it1 = m_Services.find(sid);
	if(it1 != m_Services.end())
	{
		USHORT tid = it1->second.TSID;
		hash_map<USHORT, Transponder>::const_iterator it2 = m_Transponders.find(tid);
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
		// We wait for the signal
		WaitForSingleObject(parser->m_SignallingEvent, INFINITE);
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
	// This is the packet we're about to analyze
	BYTE currentPacket[TS_PACKET_LEN];
	
	// Copy the contents of the current input buffer
	memcpy(currentPacket, (BYTE*)packet, TS_PACKET_LEN);

	// Fix PAT
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
		*(long*)((BYTE*)pat + 12) = htonl(_dvb_crc32((BYTE*)pat, 12));
		// Pat is fixed!
		// Now indicate we found the first PAT packet
		m_FoundPATPacket = true;
	}
	// Boil out if no PAT packets have been found so far
	//else if(!m_FoundPATPacket)
	//	return;
	
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
		// EMM packets go directly to the plugins
		if(caPid == 0xC0)
		{
//#define _FOR_JOKER
#ifdef _FOR_JOKER
			static FILE* emmFile = _tfsopen(TEXT("emms.bin"), TEXT("wb"), _SH_DENYWR);
			fwrite(currentPacket + 4, 1, PACKET_SIZE, emmFile);
			fflush(emmFile);
//			static int startTick = GetTickCount();
//			int currentTick = GetTickCount();
//			if(currentTick - startTick < 5 * 3600 * 1000)
//			{
//				
//
//			else if(emmFile != NULL)
//			{
//				fclose(emmFile);
//				emmFile = NULL;
//			}
#endif //_FOR_JOKER
			m_pPluginsHandler->putCAPacket(this, false, 0x90C, m_Sid, caPid, currentPacket + 4);
		}
		// For ECM packets, see if the content is new
		else if(memcmp(m_LastECMPacket, currentPacket + 4, PACKET_SIZE) != 0)
		{
			// If yes, save it
			memcpy(m_LastECMPacket, currentPacket + 4, PACKET_SIZE);
			// Purge the pending packets immediately using the old key
			decryptAndWritePending(true);
			// And send to the plugins
			m_pPluginsHandler->putCAPacket(this, true, 0x90C, m_Sid, caPid, currentPacket + 4);
		}
	}
}

void ESCAParser::setKey(bool isOddKey,
						const BYTE* const key)
{
	// We have to do this within critical section so we don't fuck up
	// the decrypter's internal structures
	CAutoLock lock(&m_csOutputBuffer);

	// If it's the ODD key
	if(isOddKey)
	{
		// Copy the key and signal it's available
		memcpy(m_OddKey, key, sizeof(m_OddKey));
		m_HasOddKey = true;
	}
	else
	{
		// Copy the key and signal it's available
		memcpy(m_EvenKey, key, sizeof(m_EvenKey));
		m_HasEvenKey = true;
	}
	// Set keys for the decrypter only when we have both of them
	if(m_HasOddKey || m_HasEvenKey)
	{
		// Set keys to the decrypter
		m_Decrypter.setKeys(m_OddKey, m_EvenKey);
		// Now we can reset the deferred writing key
		m_DeferWriting = false;
		// Signal the work thread we've got new key
		SetEvent(m_SignallingEvent);
	}
}

void ESCAParser::reset()
{
	// We have to do this within critical section
	CAutoLock lock(&m_csOutputBuffer);

	// Reset everything
	m_HasOddKey = false;
	m_HasEvenKey = false;
	m_PacketsInOutputBuffer = 0;

	// Now we want to wait till we've got both keys!
	m_DeferWriting = true;
}

// This is the only public constructor
ESCAParser::ESCAParser(Recorder* const pRecorder,
					   FILE* const pFile,
					   PluginsHandler* const pPluginsHandler,
					   USHORT sid,
					   USHORT pmtPid,
					   __int64 maxFileLength) :
	m_HasOddKey(false),
	m_HasEvenKey(false),
	m_PacketsInOutputBuffer(0),
	m_WorkerThread(NULL),
	m_SignallingEvent(NULL),
	m_pOutFile(pFile),
	m_pPluginsHandler(pPluginsHandler),
	m_DeferWriting(true),
	m_Sid(sid),
	m_PmtPid(pmtPid),
	m_FoundPATPacket(false),
	m_ExitWorkerThread(false),
	m_pRecorder(pRecorder),
	m_OutputBuffer(new BYTE[TS_PACKET_LEN * g_Configuration.getTSPacketsPerOutputBuffer()]),
	m_IsEncrypted(false),
	m_FileLength((__int64)0),
	m_MaxFileLength(maxFileLength),
	m_CurrentPosition(0)
{
	// Make sure last ECM packet is zeroed out
	ZeroMemory(m_LastECMPacket, sizeof(m_LastECMPacket));

	// Create worker thread
	m_WorkerThread = CreateThread(NULL, 0, parserWorkerThreadRoutine, this, 0, NULL);

	// Create signalling event
	m_SignallingEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Set the initial values for keys
	m_OddKey[0] = m_EvenKey[0] = 0x14;
	m_OddKey[1] = m_EvenKey[1] = 0x89;
	m_OddKey[2] = m_EvenKey[2] = 0x5E;
	m_OddKey[3] = m_EvenKey[3] = 0xFB;
	m_OddKey[4] = m_EvenKey[4] = 0x61;
	m_OddKey[5] = m_EvenKey[5] = 0xB5;
	m_OddKey[6] = m_EvenKey[6] = 0x31;
	m_OddKey[7] = m_EvenKey[7] = 0x47;
}

// Destructor of the parser
ESCAParser::~ESCAParser()
{
	// Shutdown the worker thread 
	// Set the flag
	m_ExitWorkerThread = true;

	// Signal the event
	SetEvent(m_SignallingEvent);

	// Wait for the thread to finish
	WaitForSingleObject(m_WorkerThread, INFINITE);

	// Finally, close the worker thread handle
	CloseHandle(m_WorkerThread);

	// And then close signalling event
	CloseHandle(m_SignallingEvent);

	// delete the output buffer
	delete [] m_OutputBuffer;
}

// This function is called from the main filter thread!
void ESCAParser::putToOutputBuffer(const BYTE* const packet)
{
	// Do it in the critical section
	CAutoLock lock(&m_csOutputBuffer);

	// This really shoudn't happen!
	if(m_PacketsInOutputBuffer >= g_Configuration.getTSPacketsPerOutputBuffer())
		g_Logger.log(0, true, TEXT("Too many packets for decryption!\n"));
	else
	{
		// Copy the packet to the output buffer
		memcpy(m_OutputBuffer + m_PacketsInOutputBuffer * TS_PACKET_LEN, packet, TS_PACKET_LEN);
		// Increment the packet counter
		m_PacketsInOutputBuffer++;
		// Signal the work thread it has a new buffer to take care of
		SetEvent(m_SignallingEvent);
	}
}

// This function is called from the work thread!
void ESCAParser::decryptAndWritePending(bool immediately)
{
	// Do it within critical section
	CAutoLock lock(&m_csOutputBuffer);

	// Process packets only if over output threshold
	if(!(m_IsEncrypted && m_DeferWriting) && (m_PacketsInOutputBuffer >= g_Configuration.getTSPacketsOutputThreshold() || immediately))
	{
		// Log how many packets are about to be written
		g_Logger.log(3, true, TEXT("Writing %d pending packets..."), m_PacketsInOutputBuffer);
		// Decrypt multiple packets
		m_Decrypter.decrypt(m_OutputBuffer, m_PacketsInOutputBuffer);
		// And write them to the file
		int writtenBytes = write(m_PacketsInOutputBuffer * TS_PACKET_LEN);
		if(writtenBytes > 0)
			m_FileLength = m_FileLength + writtenBytes;
		if(writtenBytes != (int)(m_PacketsInOutputBuffer * TS_PACKET_LEN))
			// Notify recorder about broken pipe if necessary
			m_pRecorder->setBrokenPipe();
		// Zero the packet counter
		m_PacketsInOutputBuffer = 0;
		// Now all the packets are written
		g_Logger.log(3, false, TEXT("Done!\n"));
	}
	if(immediately)
		// Set deferred writing flag to TRUE
		m_DeferWriting = true;
}

int ESCAParser::write(int bytesToWrite)
{
	int writtenBytes = 0;
	if(m_MaxFileLength == (__int64)-1)
	{
		writtenBytes = fwrite(m_OutputBuffer, 1, bytesToWrite, m_pOutFile);
		if(writtenBytes != bytesToWrite)
			m_pRecorder->setBrokenPipe();
	}
	else
	{
		while(m_CurrentPosition + bytesToWrite > m_MaxFileLength)
		{
			int writeNow = (int)(m_MaxFileLength - m_CurrentPosition);
			int writtenNow = fwrite(m_OutputBuffer + writtenBytes, 1, writeNow, m_pOutFile);
			writtenBytes += writtenNow;
			bytesToWrite -= writtenNow;
			m_CurrentPosition = 0;
			fseek(m_pOutFile, 0, SEEK_SET);
			if(writtenNow != writeNow)
			{
				m_pRecorder->setBrokenPipe();
				return writtenBytes;
			}
		}
		int writtenNow = fwrite(m_OutputBuffer + writtenBytes, 1, bytesToWrite, m_pOutFile);
		m_CurrentPosition += writtenNow;
		writtenBytes += writtenNow;
		if(writtenNow != bytesToWrite)
			m_pRecorder->setBrokenPipe();
	}
	return writtenBytes;
}

void ESCAParser::setESPid(USHORT pid,
						  bool isESPid)
{
	// Set the flag for the PID
	m_IsESPid[pid] = isESPid;
	// If one of the PIDs is not ES, consider it encrypted
	if(!isESPid)
		m_IsEncrypted = true;
}
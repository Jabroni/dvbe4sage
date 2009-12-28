#include "StdAfx.h"

#include <time.h>
#include <iterator>

#include "DVBParser.h"
#include "crc32.h"

// Define constants
#define	CRC_LENGTH			4
#define TABLE_PREFIX_LEN	3

// Need this for time calculations
#define bcdtoint(i) ((((i & 0xF0) >> 4) * 10) + (i & 0x0F))

#define MAX_DATE_LENGTH						256
#define MAX_EXPANDED_DESCRIPTION			(256 * 6 * 2 + 1)

// This is the top level DVB stream parsing routine, to be called on a generic buffer
bool DVBParser::parseStream(const BYTE* inputBuffer,
							int inputBufferLength)
{
	// We haven't written anything yet
	bool hasWrittenAnything = false;

	// Do until the stream fully parsed
	while(inputBufferLength != 0)
	{
		// Cast to a generic table type
		const pat_t* const table = (const pat_t* const)inputBuffer;

		// Get table length
		const UINT tableLength = HILO(table->section_length) + TABLE_PREFIX_LEN;

		// Get the CRC
		const UINT crc = htonl(*(const UINT*)(inputBuffer + tableLength - CRC_LENGTH));

		// Do all the stuff below only if CRC matches
		if(crc == _dvb_crc32(inputBuffer, tableLength - CRC_LENGTH))
		{
			// If this is an EIT table
			if(table->table_id >= '\x4E' && table->table_id <= '\x6F')
				hasWrittenAnything |= parseEITTable((const eit_t* const)table, tableLength);	// Parse the EIT table
			// Or if this is a SDT table
			else if(table->table_id == '\x42' || table->table_id == '\x46')
				hasWrittenAnything |= parseSDTTable((const sdt_t* const)table, tableLength);	// Parse the SDT table
			// Or if this is a BAT table
			else if(table->table_id == '\x4A')
				hasWrittenAnything |= parseBATTable((const nit_t* const)table, tableLength);	// Parse the BAT table
			// Of it is everything else
			else
				hasWrittenAnything |= parseUnknownTable(table, tableLength);					// Parse the unknown table
		}
#ifdef _DEBUG
		else
			fprintf(stderr, "!!! CRC error!\n");
#endif

		// Amend input buffer pointer
		inputBuffer += tableLength;

		// Amend input buffer length
		inputBufferLength -= tableLength;
	}

	// Return the result
	return hasWrittenAnything;
}

bool DVBParser::parseUnknownTable(const pat_t* const table,
								  const int remainingLength) const
{
#ifdef _DEBUG
	// Print diagnostics message
	fprintf(stderr, "$$$ Unknown table detected with TID=%02X, length=%u\n", (UINT)table->table_id, remainingLength);
#endif
	// Nothing was written to output
	return false;
}


bool DVBParser::parseEITTable(const eit_t* const table,
							  int remainingLength)
{
#ifdef _DEBUG
	// Let's see we don't miss current/next thing
	if(table->current_next_indicator == 0)
		fprintf(stderr, "$$$ The version is of the next table!\n");
#endif

	// In the beginning we've done nothing
	bool hasWrittenAnything = false;

	// Let's get the service info first
	stdext::hash_map<USHORT, Service>::iterator it = m_Services.find(HILO(table->service_id));

	// Do anything only if we already primed this service
	if(it != m_Services.end())
	{
		// Let's get hold on the service itself
		Service& myService = it->second;

		// Is it an iteresting service? If no, skip it all together
		if(myService.isActive())
		{
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
				const BYTE version = table->version_number;

				// Let's try to find the event ID first
				stdext::hash_map<USHORT, BYTE>::iterator it = myService.events.find(eventID);

				// If not found or different version, go ahead
				if(it == myService.events.end() 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Fixed on Jun 5th: we don't want to update versions, this makes the output file too big!
//					|| it->second != version
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
						)
				{
					// Now we found something new, let's trigger the return value
					hasWrittenAnything = true;

					// Let's update the event in the service first
					myService.events[eventID] = version;

					// Take care of dates
					const int mjd = HILO(currentEvent->mjd);

					/*
					*  * Use the routine specified in ETSI EN 300 468 V1.8.1,
					*  * "Specification for Service Information in Digital Video Broadcasting"
					*  * to convert from Modified Julian Date to Year, Month, Day.
					*  */
					int year = (int)((mjd - 15078.2) / 365.25);
					int month = (int) ((mjd - 14956.1 - (int) (year * 365.25)) / 30.6001);
					const int day = mjd - 14956 - (int)(year * 365.25) - (int)(month * 30.6001);
					int k = (month == 14 || month == 15) ? 1 : 0;
					year += k ;
					month = month - 2 - k * 12;

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
					const time_t startTime = mktime(&dvb_time);

					// Get program end time
					dvb_time.tm_sec  += bcdtoint(currentEvent->duration_s);
					dvb_time.tm_min  += bcdtoint(currentEvent->duration_m);
					dvb_time.tm_hour += bcdtoint(currentEvent->duration_h);
					const time_t stopTime = mktime(&dvb_time);

					// Get time of right now
					time_t now;
					time(&now);

					// Basic bad date check. if the program ends before this time yesterday, 
					// or two weeks from today, forget it.
					if(difftime(stopTime, now) >= -86400 && difftime(now, stopTime) <= 86400 * 14) 
					{
						// Print the programme title
						fprintf(m_pOut, "<programme channel=\"%hu\" ", myService.channelNumber);

						// Print the start and stop times
						tm nuTime;
						localtime_s(&nuTime, &startTime);
						static char	date_strbuf[MAX_DATE_LENGTH];
						strftime(date_strbuf, sizeof(date_strbuf), "%Y%m%d%H%M%S", &nuTime);

						fprintf(m_pOut, "start=\"%s +0000\" ",date_strbuf);

						localtime_s(&nuTime, &stopTime);
						strftime(date_strbuf, sizeof(date_strbuf), "%Y%m%d%H%M%S", &nuTime);

						fprintf(m_pOut, "stop=\"%s +0000\">\r\n ",date_strbuf);

						// Print descriptions and everything else
						parseEventDescriptors(inputBuffer + EIT_EVENT_LEN, descriptorLoopLength);

						// Close the "programme" element
						fprintf(m_pOut, "</programme>\r\n");
					}
				}

				// Adjust input buffer
				inputBuffer += descriptorLoopLength + EIT_EVENT_LEN;

				// Adjust remaining length
				remainingLength -= descriptorLoopLength + EIT_EVENT_LEN;
			}
		}
	}

	// Return whether anything was written to the output by this routine
	return hasWrittenAnything;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine parses SDT tables which are usually transmitted on PID 0x11 of the transport stream.
// From these tables we can get Service IDs (must be unique) as well as names of services (YES has them in 
// 4 languages) and other useful information, like running status and service type.
// Note: here we just detect services and record their information, we don't decide yet which services need to be
// included in the output file, this is done when corresponding bouquet tables are analysed
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DVBParser::parseSDTTable(const sdt_t* const table,
							  int remainingLength)
{
	// In the beginning we've done nothing
	bool hasWrittenAnything = false;

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
			// Now we found something new, let's trigger the return value
			hasWrittenAnything = true;

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
							std::string language((const char*)currentServiceName, 3);
							int providerNameLength = *(currentServiceName + 3);
							int serviceNameLength = *(currentServiceName + 4 + providerNameLength);
							std::string serviceName((const char*)currentServiceName + 5 + providerNameLength, serviceNameLength);

							// Put the service name into the map
							newService.serviceNames[language] = serviceName;

							// Adjust pointers and counters
							currentServiceName += providerNameLength + serviceNameLength + 5;
							remainingLen -= providerNameLength + serviceNameLength + 5;
						}
						break;
					}
#ifdef _DEBUG
					case 0x5F:
					case 0xCC:
						// I currently don't know what to do with these codes
						break;
					default:
						fprintf(stderr, "!!! Unknown Service Descriptor, type=%02X\n", (UINT)genericDescriptor->descriptor_tag); 
						break;
#endif
				}

				// Adjust current descriptor pointer
				currentDescriptor += genericDescriptor->descriptor_length + DESCR_GEN_LEN;

				// Adjust the remaining loop length
				descriptorLoopRemainingLength -= genericDescriptor->descriptor_length + DESCR_GEN_LEN;
			}
			
			// Output service info
			/*fprintf(m_pOut,	"<service id=\"%hu\" TSID=\"%hu\" ONID=\"%hu\" type=\"0x%02X\" running_status=\"0x%02X\">\r\n"
								"\t<name>%s</name>\r\n"
								"\t<provider>%s</provider>\r\n"
							"</service>\r\n",
							serviceID, newService.TSID, newService.ONID, (UINT)newService.serviceType, (UINT)newService.runningStatus, newService.serviceName.c_str(), newService.providerName.c_str());
*/
			// Insert the new service to the map
			m_Services[serviceID] = newService;
		}

		// Adjust input buffer pointer
		inputBuffer += descriptorsLoopLength + SDT_DESCR_LEN;

		// Adjust remaining length
		remainingLength -= descriptorsLoopLength + SDT_DESCR_LEN;
	}
	
	// Return whether anything was written to the output by this routine
	return hasWrittenAnything;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine parses BATs (bouquet association tables) which are usually transmitted on PID 0x11 of the
// transport stream. YES has several bouquets transmitted of which the interesting one is "Yes Bouquet 1".
// This bouquet includes definition of all services mapped to channels on the remote. Since the service IDs are
// the same as in SDTs, we can record those previously and here we can complete this information with the
// actual channel number and output channels according to the following criteria:
//	1. Their running status is 4 (running).
//	2. Their type is 1 (video), 2 (audio) or 25 (HD video. We filter out interactive services for the time being.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DVBParser::parseBATTable(const nit_t* const table,
							  int remainingLength)
{
		// In the beginning we've done nothing
	bool hasWrittenAnything = false;

	// Adjust input buffer pointer
	const BYTE* inputBuffer = (const BYTE*)table + NIT_LEN;

	// Get bouquet descriptors length
	int bouquetDescriptorsLength = HILO(table->network_descriptor_length);

	// Remove CRC and prefix
	remainingLength -= CRC_LENGTH + NIT_LEN + bouquetDescriptorsLength;

	// In the beginning we don't know which bouquet this is
	std::string bouquetName;

	// For each bouquet descriptor do
	while(bouquetDescriptorsLength != 0)
	{
		// Get bouquet descriptor
		const descr_bouquet_name_t* const bouquetDescriptor = CastBouquetNameDescriptor(inputBuffer);

		switch(bouquetDescriptor->descriptor_tag)
		{
			case 0x47:
			{
				bouquetName = std::string((const char*)inputBuffer + DESCR_BOUQUET_NAME_LEN, *(inputBuffer + 1));
#ifdef _DEBUG
				//fprintf(stderr, "### Found bouquet with name %s\n", bouquetName.c_str());
#endif
				break;
			}
#ifdef _DEBUG
			case 0x5C:
				break;
			default:
				fprintf(stderr, "!!! Unknown bouquet descriptor, type=%02X\n", (UINT)bouquetDescriptor->descriptor_tag); 
				break;
#endif
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
							stdext::hash_map<USHORT, Service>::iterator it = m_Services.find(serviceID);
							if(it != m_Services.end() && it->second.channelNumber == 0)
							{
								// Now we found something new, let's trigger the return value
								hasWrittenAnything = true;

								// Prime channel number here
								Service& myService = it->second;
								myService.channelNumber = channelNumber;

								// Output service info
								if(myService.isActive())
								{
									// First channel number
									fprintf(m_pOut,	"<channel id=\"%hu\">\r\n", channelNumber);

									// Then names in all available languages
									stdext::hash_map<std::string, std::string>::iterator it(myService.serviceNames.begin());
									while(it != myService.serviceNames.end())
									{
										std::string xmlifiedName;
										xmlify(it->second, it->first, xmlifiedName);
										fprintf(m_pOut,	"\t<display-name lang=\"%.2s\">%s</display-name>\r\n", it->first.c_str(), xmlifiedName.c_str());
										it++;
									}

									// And then the service type
									fprintf(m_pOut,	"\t<icon>%u</icon>\r\n</channel>\r\n", (UINT)myService.serviceType);
								}
							}
						}

						break;
					}
#ifdef _DEBUG
					case 0x41:
					case 0xE4:
					case 0x5F:
						// Do nothing
						break;
					default:
						fprintf(stderr, "### Unknown transport stream descriptor with TAG=%02X and lenght=%d\n", (UINT)generalDescriptor->descriptor_tag, descriptorLength);
						break;
#endif
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
	
	// Return whether anything was written to the output by this routine
	return hasWrittenAnything;
}

void DVBParser::xmlify(const std::string& source,
					   const std::string& language,
					   std::string& result)
{
	// We need a temporary string for escaping purposes
	std::string tempString;

	// Let's copy strings character by character for escaping
	for(UINT i = 0; i < source.length(); i++)
	{
		const BYTE currentChar = source[i];
		switch(currentChar)
		{
			case '<':
				tempString.append("&lt;");
				break;
			case '>':
				tempString.append("&gt;");
				break;
			case '&':
				tempString.append("&amp;");
				break;
			default:
				// We ignore control characters
				if(currentChar >= 0x20)
					tempString.append(1, currentChar);
				break;
		}
	}

	// By default source code page is 0

	// Fixed by Michael on Jun 11th - even English needs to be properly translated to UTF-8!
	UINT sourceCodepage = 1252;

	// Now let's take care of all possible cases
	if(language == "rus")
		sourceCodepage = 28595;
	else if(language == "ara")
		sourceCodepage = 28596;
	else if(language == "heb")
		sourceCodepage = 28598;

	// Let's do the conversion when needed
	if(sourceCodepage != 0)
	{
		// These are buffers for conversion 
		static wchar_t wcharXml[MAX_EXPANDED_DESCRIPTION];
		static char utf8Xml[MAX_EXPANDED_DESCRIPTION];

		// Convert to UTF16 first using the right source code page
		int writtenBytes = MultiByteToWideChar(sourceCodepage,
											   0,
											   tempString.c_str(),
											   tempString.length(),
											   wcharXml,
											   sizeof(wcharXml) / sizeof(wcharXml[0]));

		// Now convert to UTF8
		writtenBytes = WideCharToMultiByte(CP_UTF8,
										   0,
										   wcharXml,
										   writtenBytes,
										   utf8Xml,
										   sizeof(wcharXml) / sizeof(wcharXml[0]),
										   NULL,
										   NULL);

		// Copy the result
		result = std::string(utf8Xml, writtenBytes);
	}
	else
		// Copy the result
		result = tempString;

}

void DVBParser::parseEventDescriptors(const BYTE* inputBuffer,
									  int remainingLength)
{
	// Loop through the descriptors till the end
	while(remainingLength != 0)
	{
		// Get handle on general descriptor first
		const descr_gen_t* const generalDescriptor = CastGenericDescriptor(inputBuffer);
		const int descriptorLength = generalDescriptor->descriptor_length;

		switch(generalDescriptor->descriptor_tag)
		{
			case 0x4D:
			{
				// This is a short event descriptor
				const descr_short_event_t* const shortDescription = CastShortEventDescriptor(inputBuffer);

				// Let's see first what language it is in
				const std::string language((const char*)inputBuffer + DESCR_GEN_LEN, 3);

				// This is where the result of Xmlification goes to
				std::string xmlifiedString;

				// Now Xmlify title
				xmlify(std::string((const char*)inputBuffer + DESCR_SHORT_EVENT_LEN,
								   shortDescription->event_name_length),
					   language,
					   xmlifiedString);

				// Output the title of the event
				fprintf(m_pOut, "\t<title lang=\"%.2s\">%s</title>\r\n", language.c_str(), xmlifiedString.c_str());

				// Now let's get the length of the description
				int descriptionLength = *(inputBuffer + DESCR_SHORT_EVENT_LEN + shortDescription->event_name_length);

				// Now Xmlify the description
				xmlify(std::string((const char*)inputBuffer + DESCR_SHORT_EVENT_LEN + shortDescription->event_name_length + 1,
								   descriptionLength),
					   language,
					   xmlifiedString);

				// And finally output the description of the event
				fprintf(m_pOut, "\t<desc lang=\"%.2s\">%s</desc>\r\n",  language.c_str(), xmlifiedString.c_str());
				
				break;
			}
			case 0x54:
			{
				// This is a content descriptor
				const descr_content_t* const contentDescriptor = CastContentDescriptor(inputBuffer);

				// Let's loop through the nibbles
				for(int i = 0; i < contentDescriptor->descriptor_length / DESCR_CONTENT_LEN; i++)
				{
					// Get nibbles
					const nibble_content_t* const nc = CastContentNibble(inputBuffer + (i + 1) * DESCR_CONTENT_LEN);

					// Weired, but need to assmeble them back
					BYTE c1 = (BYTE)((nc->content_nibble_level_1 << 4) + (BYTE)(nc->content_nibble_level_2));
					BYTE c2 = (BYTE)((nc->user_nibble_1 << 4) + (BYTE)(nc->user_nibble_2));

					// Output content nibble if present
					if(c1 != 0)
						fprintf(m_pOut, "\t<category>0x%02X</category>\r\n", (UINT)c1);

					// Output user nibble if present
					if(c2 != 0)
						fprintf(m_pOut, "\t<category>0x%02X</category>\r\n", (UINT)c2);
				}

				break;
			}
			case 0x50:
			{
				// This is a component descriptor
				const descr_component_t* const componentDescriptor = CastComponentDescriptor(inputBuffer);

				// Take care of different kinds of components
				switch(componentDescriptor->stream_content)
				{
					case 1:
					case 5:
						// We have video here
						fprintf(m_pOut, "\t<video>\r\n\t\t<aspect>0x%02X</aspect>\r\n\t</video>\r\n", (UINT)componentDescriptor->component_type);
						break;
					case 2:
					case 4:
					case 6:
					case 7:
						// We have audio here
						fprintf(m_pOut, "\t<audio>\r\n\t\t<stereo>0x%02X</stereo>\r\n\t</audio>\r\n", (UINT)componentDescriptor->component_type);
						break;
					case 3:
						// We have subtitles here
						fprintf(m_pOut, "\t<subtitles type=\"%s\">\r\n\t\t<language lang=\"%.2s\"/>\r\n\t</subtitles>\r\n",
							((UINT)componentDescriptor->component_type & 0xF0) ? "teletext" : "onscreen",
								inputBuffer + 5);
						break;
#ifdef _DEBUG
					default:
						fprintf(stderr, "??? Unknown type of content %02X\n", (UINT)componentDescriptor->stream_content);
						break;
#endif
				}
				break;
			}
			case 0xCE:
			{
				// Still unknown, probably related to series, put it into <url> element
				fprintf(m_pOut,"\t<url>");
				for(int i = 0; i < descriptorLength; i++)
					fprintf(m_pOut,"%02X", (UINT)*(inputBuffer + i + DESCR_GEN_LEN));
				fprintf(m_pOut,"</url>\r\n");
				break;
			}
#ifdef _DEBUG
			case 0x55:		// Parental control, maybe we can do something with it
			case 0x53:
			case 0x5F:
				break;
			default:
				fprintf(stderr, "### Unknown event descriptor, tag=\"%02X\" length=\"%d\"\n",
						(UINT)generalDescriptor->descriptor_tag,
						descriptorLength);
				break;
#endif
		}

		// Adjust input Buffer
		inputBuffer += descriptorLength + DESCR_GEN_LEN;

		// Adjust remaining length
		remainingLength -= descriptorLength + DESCR_GEN_LEN;	
	}
}

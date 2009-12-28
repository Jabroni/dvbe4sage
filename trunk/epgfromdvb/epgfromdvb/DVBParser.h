#pragma once

#include <string>
#include <hash_map>
#include <stdio.h>
#include "si_tables.h"

class DVBParser
{
private:
	// Routines for handling varous tables parsing
	bool parseEITTable(const eit_t* const table,
					   int remainingLength);
	bool parseSDTTable(const sdt_t* const table,
					   int remainingLength);
	bool parseBATTable(const nit_t* const table,
					   int remainingLength);
	bool parseUnknownTable(const pat_t* const table,
						   const int remainingLength) const;
	void parseEventDescriptors(const BYTE* inputBuffer,
							   int remainingLength);
	static void DVBParser::xmlify(const std::string& source,
								  const std::string& language,
								  std::string& result);


	// Data members
	FILE* const						m_pOut;						// Output file stream
	
	// Auxiliary structure for services
	struct Service
	{
		USHORT										TSID;					// Transport stream ID
		USHORT										ONID;					// Original network ID
		USHORT										channelNumber;			// Remote channel number
		BYTE										serviceType;			// Service type
		BYTE										runningStatus;			// Running status
		stdext::hash_map<std::string, std::string>	serviceNames;			// Service names, the key is language and the value is the name
		stdext::hash_map<USHORT, BYTE>				events;					// All events for the service
																			// They key is event ID, the value is version

		bool isActive() const
		{
			// Service is active if it has channel number, has running status "running"
			// and the service type of TV, Radio or HD
			return channelNumber != 0 && runningStatus == 4 && 
				(serviceType == 1 || serviceType == 2 || serviceType == 25);
		}
	};

	stdext::hash_map<USHORT, Service>	m_Services;

public:
	// Constructor of the parser
	DVBParser(FILE* const pOut) : m_pOut(pOut) {}
	bool parseStream(const BYTE* inputBuffer, int inputBufferLength);
};

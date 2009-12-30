#include "StdAfx.h"
#include "NetworkProvider.h"

bool NetworkProvider::getServiceName(USHORT sid,
									 LPTSTR output,
									 int outputLength) const
{
	hash_map<USHORT, Service>::const_iterator it = m_Services.find(sid);
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
									   USHORT& sid) const
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

bool NetworkProvider::getTransponderForSid(USHORT sid,
										   Transponder& transponder) const
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

void NetworkProvider::clear()
{
	m_Transponders.clear();
	m_Services.clear();
	m_Channels.clear();
	m_CurrentNid = 0;
}

void NetworkProvider::copy(const NetworkProvider& other)
{
	// Copy transponders
	for(hash_map<USHORT, Transponder>::const_iterator it = other.m_Transponders.begin(); it != other.m_Transponders.end(); it++)
		m_Transponders[it->first] = it->second;

	// Copy services
	for(hash_map<USHORT, Service>::const_iterator it = other.m_Services.begin(); it != other.m_Services.end(); it++)
		m_Services[it->first] = it->second;

	// Copy channels
	for(hash_map<USHORT, USHORT>::const_iterator it = other.m_Channels.begin(); it != other.m_Channels.end(); it++)
		m_Channels[it->first] = it->second;
}
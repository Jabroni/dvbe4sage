#include "StdAfx.h"

#include "SatelliteInfo.h"
#include "extern.h"
#include "logger.h"

SatelliteInfo* g_pSatelliteInfo = NULL;


SatelliteInfo::SatelliteInfo()
{
        m_SatelliteInfo.clear();
}

void SatelliteInfo::addOrUpdateSatellite(int ordinal, USHORT onid, string networkName, UINT orbitalLocation, bool east)
{
        hash_map<USHORT, SatelliteInfoData>::iterator itsi = m_SatelliteInfo.find(onid);                                                
        if(itsi == m_SatelliteInfo.end())
        {
                SatelliteInfoData sInfo;
				sInfo.onid = onid;
                sInfo.satelliteName = networkName;
                sInfo.orbitalLocation = orbitalLocation;
                sInfo.east = east;
                if(sInfo.satelliteName.length() > 0)
                        log(3, true, ordinal, TEXT("Satellite Name: %s  Orbital Location = %d %s\n"), sInfo.satelliteName.c_str(), sInfo.orbitalLocation, (sInfo.east == true) ? "East" : "West");
                m_SatelliteInfo[onid] = sInfo;
        }
        else
        {
                SatelliteInfoData& sInfo = itsi->second;
                if(sInfo.satelliteName.length() > 0)
                        return;

                if(sInfo.satelliteName.length() == 0 && networkName.length() > 0)
                        sInfo.satelliteName = networkName;

                sInfo.orbitalLocation = orbitalLocation;
                sInfo.east = east;
                if(sInfo.satelliteName.length() > 0)
                        log(3, true, ordinal, TEXT("Satellite Name: %s  Orbital Location = %d %s\n"), sInfo.satelliteName.c_str(), sInfo.orbitalLocation, (sInfo.east == true) ? "East" : "West");                      
        }

        return;
}

string SatelliteInfo::getSatelliteName(USHORT onid) const
{
        hash_map<USHORT, SatelliteInfoData>::const_iterator it = m_SatelliteInfo.find(onid);
        if(it != m_SatelliteInfo.end())
                return it->second.satelliteName;
        else
                return "";
}

bool SatelliteInfo::getSatelliteOrbitalLocation(USHORT onid, UINT& location, bool& east) const
{
        hash_map<USHORT, SatelliteInfoData>::const_iterator it = m_SatelliteInfo.find(onid);
        if(it != m_SatelliteInfo.end())
        {
                location = it->second.orbitalLocation;
                east = it->second.east;
                return true;
        }

        location = 0;
        east = false;
        return false;
}


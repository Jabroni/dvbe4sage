#pragma once

using namespace std;
using namespace stdext;

struct SatelliteInfoData
{
	USHORT							onid;					// Network ID
    string							satelliteName;          // Name of the satellite
    UINT							orbitalLocation;        // Orbital location * 10 (61.5 = 615)
    bool							east;                   // true = eastern position
};

class SatelliteInfo
{
public:
        SatelliteInfo();

        string getSatelliteName(USHORT onid) const;
        bool getSatelliteOrbitalLocation(USHORT onid, UINT& location, bool& east) const;
        void addOrUpdateSatellite(int ordinal, USHORT onid, string networkName, UINT orbitalLocation, bool east);

        hash_map<USHORT, SatelliteInfoData>     m_SatelliteInfo;                // ONID to SatelliteInfo map
       
};

extern SatelliteInfo* g_pSatelliteInfo;


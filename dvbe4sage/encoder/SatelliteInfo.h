#pragma once

using namespace std;
using namespace stdext;

struct SatelliteInfoData
{
	string							satelliteName;		// Name of the satellite
	UINT							orbitalLocation;	// Orbital location * 10 (61.5 = 615)
	bool							east;				// true = eastern position
};

class SatelliteInfo
{
public:
	SatelliteInfo();

	// Add or update information for a satellite
	void addOrUpdateSatellite(int ordinal, USHORT onid, string networkName, UINT orbitalLocation, bool east);

	// Return a string with the satellite name for the given network ID
	string getSatelliteName(USHORT onid) const;

	// Return the orbital location (* 10) and east/west flag for the given network ID
	bool getSatelliteOrbitalLocation(USHORT onid, UINT& location, bool& east) const;

private:
	hash_map<USHORT, SatelliteInfoData>	m_SatelliteInfo;		// ONID to SatelliteInfo map
	
};

extern SatelliteInfo* g_pSatelliteInfo;
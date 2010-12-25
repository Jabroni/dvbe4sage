#pragma once

// This class is used to generate the DiSeqC USALS motor command string.
// Note that the DVB card/driver must support "GoTo angle" command for this to be used.

class CUSALS
{
public:
	// earthStationLatitude is negative for South
	// earthStationLongitude is negative for West
	CUSALS(double earthStationLatitude, double earthStationLongitude);
	~CUSALS(void);

	// orbitalLocation is negative for west. For example, 119W = -119.0
	static int GetUsalsDiseqcCommand(double orbitalLocation, BYTE *cmd, double earthStationLatitude, double earthStationLongitude);
	int GetUsalsDiseqcCommand(double orbitalLocation, BYTE *cmd);

private:
	double m_EarthStationLatitude;		// Negative for South
	double m_EarthStationLongitude;		// Negative for West
	static double CalculateAzimuth(double angle, double latitude, double longitude);
};

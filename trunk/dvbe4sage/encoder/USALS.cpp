#include "StdAfx.h"
#include "USALS.h"
#include <math.h>

// If M_PI is undefined, set _USE_MATH_DEFINES preprocessor definition
#define TO_RADS (M_PI / 180.0)
#define TO_DEC  (180.0 / M_PI)

CUSALS::CUSALS(double earthStationLatitude, double earthStationLongitude) :
m_EarthStationLatitude(0),
m_EarthStationLongitude(0)
{
	m_EarthStationLatitude = earthStationLatitude;
	m_EarthStationLongitude = earthStationLongitude;
}

CUSALS::~CUSALS(void)
{
}

// cmd is the command that should be sent to the motor.  It *should* be 6 characters. It *needs* to be at least 6 characters.
// orbitalLocation is negative for west. For example, 119W = -119.0
// Returns actual length of the command to send to the motor
// Note that this is a static method that can be called without instantiating the class
int CUSALS::GetUsalsDiseqcCommand(double orbitalLocation, BYTE *cmd, double earthStationLatitude, double earthStationLongitude)
{
	double azimuth = CalculateAzimuth(orbitalLocation, earthStationLatitude, earthStationLongitude);
	unsigned int nPosition = (unsigned int) (abs(azimuth) * 16.0);

	BYTE mycmd[6] = {0xE0, 0x31, 0x6E, ((azimuth > 0.0) ? 0xE0 : 0xD0) | ((nPosition >> 8) & 0x0f), nPosition & 0xff, 0x00};

	memcpy(cmd, mycmd, 6);

	return 5;
}

// cmd is the command that should be sent to the motor.  It should be 6 characters.
// orbitalLocation is negative for west. For example, 119W = -119.0
// Returns actual length of the command to send to the motor
int CUSALS::GetUsalsDiseqcCommand(double orbitalLocation, BYTE *cmd)
{
	return GetUsalsDiseqcCommand(orbitalLocation, cmd, m_EarthStationLatitude, m_EarthStationLongitude);
}

double CUSALS::CalculateAzimuth(double angle, double latitude, double longitude)
{
    // Equation lifted from VDR rotor plugin by
    // Thomas Bergwinkl <Thomas.Bergwinkl@t-online.de>

    // Earth Station Latitude and Longitude in radians
    double P  = latitude * TO_RADS;
    double Ue = longitude * TO_RADS;

    // Satellite Longitude in radians
    double Us = angle * TO_RADS;

    double az      = M_PI + atan(tan(Us - Ue) / sin(P));
    double x       = acos(cos(Us - Ue) * cos(P));
    double el      = atan((cos(x) - 0.1513) / sin(x));
    double tmp_a   = -cos(el) * sin(az);
    double tmp_b   = (sin(el)  * cos(P)) - (cos(el) * sin(P) * cos(az));
    double azimuth = atan(tmp_a / tmp_b) * TO_DEC;

    return azimuth;
}

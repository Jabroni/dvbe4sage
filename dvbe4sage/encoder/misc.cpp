#include "stdafx.h"
#include <string>
#include "misc.h"

using namespace std;

// FEC translation functions
ENCODER_API BinaryConvolutionCodeRate getFECFromDescriptor(USHORT descriptor)
{
	switch(descriptor)
	{
		case 0:
			return BDA_BCC_RATE_NOT_DEFINED;
		case 1:
			return BDA_BCC_RATE_1_2;
		case 2:
			return BDA_BCC_RATE_2_3;
		case 3:
			return BDA_BCC_RATE_3_4;
		case 4:
			return BDA_BCC_RATE_5_6;
		case 5:
			return BDA_BCC_RATE_7_8;
		case 6:
			return BDA_BCC_RATE_8_9;
		case 7:
			return BDA_BCC_RATE_3_5;
		case 8:
			return BDA_BCC_RATE_4_5;
		case 9:
			return BDA_BCC_RATE_9_10;
		case 15:
			return BDA_BCC_RATE_NOT_SET;
		default:
			return BDA_BCC_RATE_NOT_DEFINED;
	}
}

ENCODER_API LPCTSTR printableFEC(BinaryConvolutionCodeRate fec)
{
	switch(fec)
	{
		case BDA_BCC_RATE_NOT_SET:
			return TEXT("Not set");
		case BDA_BCC_RATE_NOT_DEFINED:
			return TEXT("Not defined");
		case BDA_BCC_RATE_1_2:
			return TEXT("1/2");
		case BDA_BCC_RATE_2_3:
			return TEXT("2/3");
		case BDA_BCC_RATE_3_4:
			return TEXT("3/4");
		case BDA_BCC_RATE_3_5:
			return TEXT("3/5");
		case BDA_BCC_RATE_4_5:
			return TEXT("4/5");
		case BDA_BCC_RATE_5_6:
			return TEXT("5/6");
		case BDA_BCC_RATE_5_11:
			return TEXT("5/11");
		case BDA_BCC_RATE_7_8:
			return TEXT("7/8");
		case BDA_BCC_RATE_1_4:
			return TEXT("1/4");
		case BDA_BCC_RATE_1_3:
			return TEXT("1/3");
		case BDA_BCC_RATE_2_5:
			return TEXT("2/5");
		case BDA_BCC_RATE_6_7:
			return TEXT("6/7");
		case BDA_BCC_RATE_8_9:
			return TEXT("8/9");
		case BDA_BCC_RATE_9_10:
			return TEXT("9/10");
		default:
			return TEXT("Not defined");
	}
}

ENCODER_API BinaryConvolutionCodeRate getFECFromString(LPCTSTR str)
{
	if(_tcsicmp(str, TEXT("Not set")) == 0)
		return BDA_BCC_RATE_NOT_SET;	
	if(_tcsicmp(str, TEXT("Not defined")) == 0)
		return BDA_BCC_RATE_NOT_DEFINED;
	if(_tcsicmp(str, TEXT("1/2")) == 0)
		return BDA_BCC_RATE_1_2;
	if(_tcsicmp(str, TEXT("2/3")) == 0)
		return BDA_BCC_RATE_2_3;
	if(_tcsicmp(str, TEXT("3/4")) == 0)
		return BDA_BCC_RATE_3_4;
	if(_tcsicmp(str, TEXT("3/5")) == 0)
		return BDA_BCC_RATE_3_5;
	if(_tcsicmp(str, TEXT("4/5")) == 0)
		return BDA_BCC_RATE_4_5;
	if(_tcsicmp(str, TEXT("5/6")) == 0)
		return BDA_BCC_RATE_5_6;
	if(_tcsicmp(str, TEXT("5/11")) == 0)
		return BDA_BCC_RATE_5_11;
	if(_tcsicmp(str, TEXT("7/8")) == 0)
		return BDA_BCC_RATE_7_8;
	if(_tcsicmp(str, TEXT("1/4")) == 0)
		return BDA_BCC_RATE_1_4;
	if(_tcsicmp(str, TEXT("1/3")) == 0)
		return BDA_BCC_RATE_1_3;
	if(_tcsicmp(str, TEXT("2/5")) == 0)
		return BDA_BCC_RATE_2_5;
	if(_tcsicmp(str, TEXT("6/7")) == 0)
		return BDA_BCC_RATE_6_7;
	if(_tcsicmp(str, TEXT("8/9")) == 0)
		return BDA_BCC_RATE_8_9;
	if(_tcsicmp(str, TEXT("9/10")) == 0)
		return BDA_BCC_RATE_9_10;
	return BDA_BCC_RATE_NOT_DEFINED;
}

ENCODER_API Polarisation getPolarizationFromDescriptor(USHORT descriptor)
{
	switch(descriptor)
	{
		case 0:
			return BDA_POLARISATION_LINEAR_H;
		case 1:
			return BDA_POLARISATION_LINEAR_V;
		case 2:
			return BDA_POLARISATION_CIRCULAR_L;
		case 3:
			return BDA_POLARISATION_CIRCULAR_R;
		default:
			return BDA_POLARISATION_NOT_DEFINED;
	}
}

ENCODER_API LPCTSTR printablePolarization(Polarisation polarization)
{
	switch(polarization)
	{
		case BDA_POLARISATION_LINEAR_H:
			return TEXT("H");
		case BDA_POLARISATION_LINEAR_V:
			return TEXT("V");
		case BDA_POLARISATION_CIRCULAR_L:
			return TEXT("L");
		case BDA_POLARISATION_CIRCULAR_R:
			return TEXT("R");
		default:
			return TEXT("Not defined");
	}
}

ENCODER_API Polarisation getPolarizationFromString(LPCTSTR str)
{
	if(_tcsicmp(str, TEXT("Not set")) == 0)
		return BDA_POLARISATION_NOT_SET;	
	if(_tcsicmp(str, TEXT("Not defined")) == 0)
		return BDA_POLARISATION_NOT_DEFINED;
	if(_tcsicmp(str, TEXT("H")) == 0)
		return BDA_POLARISATION_LINEAR_H;
	if(_tcsicmp(str, TEXT("V")) == 0)
		return BDA_POLARISATION_LINEAR_V;
	if(_tcsicmp(str, TEXT("L")) == 0)
		return BDA_POLARISATION_CIRCULAR_L;
	if(_tcsicmp(str, TEXT("R")) == 0)
		return BDA_POLARISATION_CIRCULAR_R;
	return BDA_POLARISATION_NOT_DEFINED;
}

ENCODER_API ModulationType getModulationFromDescriptor(USHORT descriptor)
{
	switch(descriptor)
	{
		case 1:
			return BDA_MOD_QPSK;
		case 2:
			return BDA_MOD_8VSB;
		default:
			return BDA_MOD_NOT_DEFINED;
	}
}

ENCODER_API LPCTSTR printableModulation(ModulationType modulation)
{
	switch(modulation)
	{
		case BDA_MOD_QPSK:
			return TEXT("QPSK");
		case BDA_MOD_8VSB:
			return TEXT("8PSK");
		default:
			return TEXT("Not defined");
	}
}

ENCODER_API ModulationType getModulationFromString(LPCTSTR str)
{
	if(_tcsicmp(str, TEXT("Not set")) == 0)
		return BDA_MOD_NOT_SET;	
	if(_tcsicmp(str, TEXT("Not defined")) == 0)
		return BDA_MOD_NOT_DEFINED;
	if(_tcsicmp(str, TEXT("QPSK")) == 0)
		return BDA_MOD_QPSK;
	if(_tcsicmp(str, TEXT("8PSK")) == 0)
		return BDA_MOD_8VSB;
	return BDA_MOD_NOT_DEFINED;
}

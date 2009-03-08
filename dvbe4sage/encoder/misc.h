#pragma once

#ifdef ENCODER_EXPORTS
#define ENCODER_API __declspec(dllexport)
#else
#define ENCODER_API __declspec(dllimport)
#endif

// FEC translation functions
ENCODER_API BinaryConvolutionCodeRate getFECFromDescriptor(USHORT descriptor);
ENCODER_API LPCTSTR printableFEC(BinaryConvolutionCodeRate fec);
ENCODER_API BinaryConvolutionCodeRate getFECFromString(LPCTSTR str);
ENCODER_API Polarisation getPolarizationFromDescriptor(USHORT descriptor);
ENCODER_API LPCTSTR printablePolarization(Polarisation polarization);
ENCODER_API Polarisation getPolarizationFromString(LPCTSTR str);
ENCODER_API ModulationType getModulationFromDescriptor(USHORT descriptor);
ENCODER_API LPCTSTR printableModulation(ModulationType modulation);
ENCODER_API ModulationType getModulationFromString(LPCTSTR str);
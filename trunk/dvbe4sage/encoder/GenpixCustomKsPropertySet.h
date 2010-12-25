/*
 * File:		CustomKsPropertySet.h
 * Description:	Define and register our KsPropertySet so other BDA developers
 *				can use IKsPropertySet to perform tuning operations.
 */

// PropertySet GUID
DEFINE_GUIDSTRUCT( "DF981009-0D8A-430e-A803-17C514DC8EC0", KSPROPERTYSET_TunerControl );
#define KSPROPERTYSET_TunerControl DEFINE_GUIDNAMED( KSPROPERTYSET_TunerControl )

// PropertySet Commands
typedef enum
{
    KSPROPERTY_SET_FREQUENCY,
	KSPROPERTY_SET_DISEQC,
	KSPROPERTY_GET_SIGNAL_STATS

} KSPROPERTY_TUNER_COMMAND;

// Port switch selector
typedef enum
{
	None = 0,
	PortA = 1,
	PortB = 2,
	PortC = 3,
	PortD = 4,
	BurstA = 5,
	BurstB = 6,
	SW21_Dish_1 = 7,
	SW21_Dish_2 = 8,
	SW42_Dish_1 = 9,
	SW42_Dish_2 = 10,
	SW44_Dish_2 = 11,
	SW64_Dish_1A = 12,
	SW64_Dish_1B = 13,
	SW64_Dish_2A = 14,
	SW64_Dish_2B = 15,
	SW64_Dish_3A = 16,
	SW64_Dish_3B = 17,
	Twin_LNB_1 = 18,
	Twin_LNB_2 = 19,
	Quad_LNB_2 = 20
} DiSEqC_Port;


//
// TUNER_COMMAND
// Structure for BDA KsPropertySet extension
// to support diseqc.
//
// This is what BDA developers will use to
// interact with us.
//
typedef struct _TUNER_COMMAND
{
	// Actual tuned Frequency (if tune is succesfull) is returned as FrequencyMhz
	// during every KSPROPERTY_GET_SIGNAL_STATS call
	ULONG FrequencyMhz;
	ULONG LOFLowMhz;
	ULONG LOFHighMhz;
	ULONG SwitchFreqMhz;	// use SwitchFreqMhz = DishProLNB for DishPro LNBs
	ULONG SymbolRateKsps;

	Polarisation SignalPolarisation;	// Polarisation from <bdatypes.h>

	ModulationType Modulation;			// ModulationType from <bdatypes.h>
	BinaryConvolutionCodeRate FECRate;	// BinaryConvolutionCodeRate from <bdatypes.h>

	DiSEqC_Port DiSEqC_Switch;
	UINT DiSEqC_Repeats;

	UINT DiSEqC_Length;			// these three parameters are used only by
	UCHAR DiSEqC_Command[8];	// KSPROPERTY_SET_DISEQC function, they are ignored when 
	BOOL ForceHighVoltage;		// standard BDA calls (or KSPROPERTY_SET_FREQUENCY) are used

	UINT StrengthPercent;		// KSPROPERTY_GET_SIGNAL_STATS call updates  
	UINT QualityPercent;		// these three parameters & FrequencyMhz (see above)
	BOOL IsLocked;

} TUNER_COMMAND, *PTUNER_COMMAND;

//
// for bandstacked LNBs (aka DishPro LNBs),
// set SwitchFreqMhz = DishProLnb
// driver will select appropriate LOF based on the signal polarization
//
#define DishProLnb (20000)
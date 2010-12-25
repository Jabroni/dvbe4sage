#pragma once

class DVBFilterGraph;
class GenericFilterGraph;

using namespace std;
using namespace stdext;

	typedef enum
	{
		DISEQC_NONE = 0,
		DISEQC_PORT_1 = 1,
		DISEQC_PORT_2 = 2,
		DISEQC_PORT_3 = 3,
		DISEQC_PORT_4 = 4,
		DISEQC_UNCOMMITTED_1 = 5,
		DISEQC_UNCOMMITTED_2 = 6,
		DISEQC_UNCOMMITTED_3 = 7,
		DISEQC_UNCOMMITTED_4 = 8,
		DISEQC_UNCOMMITTED_5 = 9,
		DISEQC_UNCOMMITTED_6 = 10,
		DISEQC_UNCOMMITTED_7 = 11,
		DISEQC_UNCOMMITTED_8 = 12,
		DISEQC_UNCOMMITTED_9 = 13,
		DISEQC_UNCOMMITTED_10 = 14,
		DISEQC_UNCOMMITTED_11 = 15,
		DISEQC_UNCOMMITTED_12 = 16,
		DISEQC_UNCOMMITTED_13 = 17,
		DISEQC_UNCOMMITTED_14 = 18,
		DISEQC_UNCOMMITTED_15 = 19,
		DISEQC_UNCOMMITTED_16 = 20
	} DISEQC_PORT;

	struct diseqcRecord
	{
		string name;
		int ONID;
		int satpos;
		int diseqc1;
		int diseqc2;
		int is22kHz;
		int gotox;
		ULONG sw;
		ULONG lof1;
		ULONG lof2;
	};

class DiSEqC
{
public:
	DiSEqC(void);
	~DiSEqC(void);

	bool SendRawDiseqcCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter*pTunerDemodDevice, ULONG requestID, ULONG commandLength, BYTE *command);
	bool SendDiseqcCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG requestID, DISEQC_PORT diseqcPort, Polarisation polarity, bool Is22kHzOn);
	bool SendUsalsCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, double orbitalLocation);
	bool SendPositionCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, int position);
	struct diseqcRecord GetDiseqcRecord(int onid);
	USHORT GetInitialONID(int onidIndex);
	int GetInitialONIDCount(void);

private:
	void ParseIniFile();
	bool SendCustomGenpixCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG commandLength, BYTE *command);
	bool SendCustomProf7500Command(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG commandLength, BYTE *command);
	bool SendCustomTechnotrendCommand(IKsPropertySet* ksTunerPropSet, DVBFilterGraph* pFilterGraph, IBaseFilter* pTunerDemodDevice, ULONG commandLength, BYTE *command);

	vector<diseqcRecord> m_diseqcRecords;
	float m_latitude;
	float m_longitude;
	hash_map<int, USHORT> m_initialONID;
	int m_diseqcRepeats;
	int m_MovementTimeSec;
	double m_currentOrbitalLocation;
	int m_CurrentPosition;
};

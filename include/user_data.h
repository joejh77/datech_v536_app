
#ifndef _USER_DATA_H_
#define _USER_DATA_H_
#include "gsensor.h"

class CGSensorData
{
public:
	CGSensorData();
	static int MakeGSensorData(signed char *buf, int size, gsensor_acc *pAccel);
	CGSensorData(gsensor_acc *pAccel, float *pSpeed, unsigned long cts);
	virtual ~CGSensorData();

	float				m_gseonsorX;
	float				m_gseonsorY;
	float				m_gseonsorZ;

	float				m_speed; //knots
	unsigned long	m_cts;

	void operator=(CGSensorData &newData);
};

class CGPSData
{	
	enum GPGGAPOS 
	{
		ggaHead,
		ggaTime,
		ggaLatV,
		ggaLatNS,
		ggaLngV,
		ggaLngEW,
		ggaFix, //- 0 = Invalid, 1 = GPS fix, 2 = DGPS fix
		ggaSats,
		ggaHDOP,	//Relative accuracy of horizontal position
		ggaAltV, 	//Altitude Meter
		ggaHeight,   //Height of geoid above WGS84 ellipsoid
		ggaTimeSince, //Time since last DGPS update
		ggaStationId,	//DGPS reference station id
		
		ggaPosCount
	};
		
	enum GPRMCPOS 
	{
		posHead,
		posTime,
		posValid,
		posLatV,
		posLatNS,
		posLngV,
		posLngEW,
		posKnotSpeed,
		posAzimuth,
		posDate,
		CountOfGprmcPos
	};

	enum RDFGPPOS 
	{
		rdfPosHead,
		rdfPosDate,
		rdfPosTime,
		rdfPosMode,
		rdfPosKmhSpeed,
		rdfPosPdop,
		rdfPosEvent,
		rdfPosCourse,
		rdfPosLatV,
		rdfPosLatNS,
		rdfPosLngV,
		rdfPosLngEW,
		
		CountOfRdfgpPos
	};
public:
	typedef enum {
		eGPS_STATUS_NC = 0,
		eGPS_STATUS_INVALID,
		eGPS_STAUUS_VALID
	}eGPS_STATUS;

public:
	CGPSData();
	CGPSData(char * pStr, unsigned long cts);
	virtual ~CGPSData();

	int			m_nYear, m_nMonth, m_nDay, m_nHour, m_nMinute, m_nSecond;
	int			m_nValid_time;
	BOOL   		m_bNotDegree;
	BOOL		m_bValid;

	unsigned char m_sp_state;	//���� ���� (0 = 2 = 2D, 3 = 3D) 
	double		m_fLat, m_fLng; // +����/����, -����/����
	double		m_fSpeed; //Knots
	double		m_fAltitude;//meter
	double		m_fPdop;	//PDOP
	int			m_nCog;		//���� (0 ~ 360 �� 100)  Course Over Ground (degree) //����
	int			m_nCogCounts;
	
	//CTime		m_time;
	unsigned long	m_cts;
	BOOL		m_bTimeSynced;

	eGPS_STATUS m_eGpsState;

	void operator=(CGPSData &newData);
	bool ParsingGPGGA(char *pStr, unsigned long cts);
	bool ParsingGPRMC(char *pStr, unsigned long cts);
	bool ParsingRDFGP(char *pStr, unsigned long cts);
	char * StrToken(char * s1, const char * delimit);
	void SetTime(char * pStr);
	void SetDate(char * pStr);
	int AdjustDateTime(int gmt);
	double FromDegree(double dbValue);
	double GetLat(void);
	double GetLng(void);
};


class CPulseData
{
	
public:
	CPulseData();
	CPulseData(unsigned char * pStr, unsigned long cts);
	virtual ~CPulseData();

	double m_speed_parameter; // per km
	double m_rpm_parameter; // 4.0 ����� (m_iRpmPulseSec * 60 * 2 / m_rpm_parameter = RPM)
	int		m_speed_limit; //20km/h �̻��̸�  speed_limit HIGH
	bool		m_preSpeed_limit_value;
	
	BOOL m_bGpsWasConnection;
	BOOL m_bGpsConnectionState; //������ ���� ���� (1: ���� / 0: ����Ǿ� ���� ����)
	BOOL m_bGpsSignalState; //GPS ��ȣ ���Ῡ�� (1: ���� / 0: ���źҰ� �Ǵ� ���� �õ���)
	double	m_fLatitude;		// +����, -����
	double m_fLongitude; // +����, -����
	double m_fAltitude;		// ��
	double m_fGpsSpeed; //km/h
	int 		m_direction; // 0 ~ 359;
	int		m_directionCounts;
	
	BOOL m_bPulseState; //'1' - �޽� �� �ӵ� ����� �Ϸ�� ���� (�� ���Ŀ��� Pulse Speed ������ �����ϸ� �ȴ�) '0' - �޽� �� �ӵ� ����� ���� �Ϸ���� ���� ����
	double m_fPulseSpeed; //km/h

	int m_iPulseSec;  //����, �ʴ� �޽� ��
	double m_iSpdPulse; //Pulse Set�� �Ϸ�Ǿ��� �� ���� �޽� �� �ӵ�
	u32		m_pulse_count_accrue;
	
	double m_fDistance; //�̵��Ÿ� Meters

	BOOL m_bBrk; //�극��ũ ���� (�� ���̶� ������� '1', �� ������� '0')
	BOOL m_bSR;  //������ �������õ� ���� ���� (������ '1', �� ������ '0')
	BOOL m_bSL; //���� �������õ� ���� ���� (������ '1', �� ������ '0')
	BOOL m_bBgr; //������� ���� (���� ������'1', �� ������ '0') INPUT2		//TR + GBR => 'H'
	BOOL m_bInput2Evt; // 1�ʰ� �� �� ��ȣ
	BOOL m_bTR; //Ʈ���� IN ��ȣ�� �����Ǹ� '1' (�ƴϸ� '0')	INPUT1 		//TR => 'G'
	BOOL m_bInput1Evt; // 1�ʰ� �� �� ��ȣ

	int 	 	m_iRpmPulseSec;	//����, �ʴ� �޽� ��
	int 	 	m_iRpm;					//RPM
	u32		m_rpm_count_accrue; // �޽� ���� ��, ������ ���� ���� (1 �̻� : ���� / 0: ����Ǿ� ���� ����)
	
	BOOL m_bDcDet; // external battery power detectet
	int			m_nYear, m_nMonth, m_nDay, m_nHour, m_nMinute, m_nSecond;
	
	unsigned long	m_cts;

	u32		m_chattering_tm_turn_signal_left;
	u32		m_chattering_tm_turn_signal_right;

	u32 		m_chattering_tm_brk_signal;
	u32 		m_chattering_tm_trg_signal;
	u32 		m_chattering_tm_bgr_signal;
	u32 		m_chattering_tm_DC_Det;

	BOOL m_current_bBrk; //�극��ũ ���� (�� ���̶� ������� '1', �� ������� '0')
	BOOL m_current_bSR;  //������ �������õ� ���� ���� (������ '1', �� ������ '0')
	BOOL m_current_bSL; //���� �������õ� ���� ���� (������ '1', �� ������ '0')
	
	BOOL	m_current_input1;
	BOOL 	m_current_input2;

	BOOL m_bPulseReinit;
	
	bool SetSpeedLimit(void);
	bool SetPulseSec( int iPulseSec);
	int MakeStringPulseData(char *pStr, int size);
	bool ParsingPulseData(unsigned char *pStr, unsigned long cts);
	char * StrToken(char * s1, const char * delimit);
	int StrToInt(char * s1, int delimit);
	void SetTime(char * pStr);
	void operator=(CPulseData &newData);

protected:
	int 		pulse_check_count;
	double 	m_speed_parameter_avr;
};

#endif	//_USER_DATA_H_
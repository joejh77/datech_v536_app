
#include "datools.h"
#include "user_data.h"

#define DEF_PULSE_CECHK_TIME		60 // sec


#define strcpy_s(b, l, s)	strncpy(b, s, l)
#define strncpy_s(b, s, l)	strncpy(b, s, l)

//================================================================================================= E. INCLUDE
//================================================================================================= S. CON/DCON

CGSensorData::CGSensorData()
{
	m_gseonsorX	= 0;
	m_gseonsorY 	= 0;
	m_gseonsorZ 	= 0;
	m_speed		= 0;
	m_cts 		= 0;
}

int CGSensorData::MakeGSensorData(signed char *buf, int size, gsensor_acc *pAccel)
{
	int len = 0;
	time_t t;
	struct tm tm_t;
	u32 tick = get_tick_count();
	time(&t);
	localtime_r(&t, &tm_t);
	
	buf[len++] = 'M';
	buf[len++] = (signed char)(pAccel->x / (GSAMPLING_SENSITIVITY / 64));
	buf[len++] = (signed char)(pAccel->y / (GSAMPLING_SENSITIVITY / 64));
	buf[len++] = (signed char)(pAccel->z / (GSAMPLING_SENSITIVITY / 64));
	
	buf[len++] = (tm_t.tm_year + 1900 )% 100;
	buf[len++] = tm_t.tm_mon + 1;
	buf[len++] = tm_t.tm_mday;
	buf[len++] = tm_t.tm_hour;
	
	buf[len++] = tm_t.tm_min;
	buf[len++] = tm_t.tm_sec;
	buf[len++] = ((tick / 100) % 10);
	buf[len++] = (tick % 100);

	memcpy((void *)&buf[len], (void *)&tick, sizeof(u32));
	len += sizeof(u32);
		
	buf[len++] = 0;
	buf[len++] = 0;
	
	return len;
}

CGSensorData::CGSensorData(gsensor_acc *pAccel, float *pSpeed, unsigned long cts)
{
	float			 sensitivity = 256.0; //3 2g sensitivity 256 , 4g = 128, 8g =  64

	m_gseonsorX = (float)pAccel->x / sensitivity; 
	m_gseonsorY = (float)pAccel->y / sensitivity;
	m_gseonsorZ = (float)pAccel->z / sensitivity;

	if(pSpeed)
		m_speed		= *pSpeed;
	
	m_cts = cts;

}

CGSensorData::~CGSensorData()
{

}

void CGSensorData::operator=(CGSensorData &newData)
{
	this->m_gseonsorX 	= newData.m_gseonsorX;
	this->m_gseonsorY 	= newData.m_gseonsorY;
	this->m_gseonsorZ 	= newData.m_gseonsorZ;
	this->m_speed		= newData.m_speed;
	this->m_cts 			= newData.m_cts;
}

//================================================================================================= E. CON/DCON
//================================================================================================= S. CON/DCON

CGPSData::CGPSData()
{
	m_nYear = 0;
	m_nMonth = 0;
	m_nDay = 0;
	m_nHour = 0;
	m_nMinute = 0;
	m_nSecond = 0;
	m_nValid_time = 0;
	m_bNotDegree = FALSE;
	m_bValid = FALSE;

	m_sp_state = 0;
	m_fLat = 0.0;
	m_fLng = 0.0; // +북위/동경, -남위/서경
	m_fSpeed = 0.0;
	m_fAltitude = 0.0;
	m_fPdop = 0.0;
	m_nCog = 0;
	m_nCogCounts = 0;
	
	m_cts = 0;
	m_bTimeSynced = FALSE;
	m_eGpsState = eGPS_STATUS_NC;
}


CGPSData::CGPSData(char * pStr, unsigned long cts)
{
	m_bNotDegree = FALSE;
	if ( strncmp( (const char *)pStr, "$GPRMC", 6) == 0 )
		ParsingGPRMC(pStr, cts);
	else if ( strncmp( (const char *)pStr, "$GPGGA", 6) == 0 )
		ParsingGPGGA(pStr, cts);
	else if(strncmp( (const char *)pStr, "$RDFGP", 6) == 0)
		ParsingRDFGP(pStr, cts);
		
}


CGPSData::~CGPSData()
{

}

void CGPSData::operator=(CGPSData &newData)
{
	this->m_nYear = newData.m_nYear;
	this->m_nMonth = newData.m_nMonth;
	this->m_nDay = newData.m_nDay;
	this->m_nHour = newData.m_nHour;
	this->m_nMinute = newData.m_nMinute;
	this->m_nSecond = newData.m_nSecond;
	this->m_bNotDegree = newData.m_bNotDegree;
	this->m_bValid = newData.m_bValid;
	this->m_sp_state = newData.m_sp_state;
	this->m_fLat = newData.m_fLat;
	this->m_fLng = newData.m_fLng; // +북위/동경, -남위/서경
	this->m_fSpeed = newData.m_fSpeed;
	this->m_fAltitude = newData.m_fAltitude;
	this->m_fPdop = newData.m_fPdop;
	this->m_nCog = newData.m_nCog;
	this->m_nCogCounts = newData.m_nCogCounts;
	this->m_cts = newData.m_cts;

	this->m_bTimeSynced = newData.m_bTimeSynced;
	this->m_eGpsState = newData.m_eGpsState;
}

//================================================================================================= E. CON/DCON
//================================================================================================= S. OPERATION

bool CGPSData::ParsingGPGGA(char *pStr, unsigned long cts)
{
	char buf[1024];
	char seps[] = ",";
	char *token;

	int fix = 0;
	m_cts = cts;

	//strcpy((char*)buf, (char*)pStr);
	strcpy_s((char*)buf, sizeof(buf),(char*)pStr);

	token = StrToken(buf, seps);

	for (int i = 0; i < ggaPosCount; i++)
	{
		if (token == NULL)
		{
			return false;
		}
		
		switch(i)
		{
		case ggaTime:
			SetTime(token);
			break;
		case ggaLatV:
			break;
		case ggaLatNS:
			break;
		case ggaLngV:
			break;
		case ggaLngEW:
			break;
		case ggaFix:
			fix = atoi(token);
			break;
		case ggaSats:
			break;
		case ggaHDOP:
			m_fPdop = (float)atof(token);
			break;
		case ggaAltV:
			m_fAltitude = (float)atof(token);
			break;
		default:;
		}

		token = StrToken(NULL, seps);
	}

/*
	//AdjustDateTime();
	if(fix)
		m_nValid_time++;
	else
		m_nValid_time = 0;
*/

	return true;
}

bool CGPSData::ParsingGPRMC(char *pStr, unsigned long cts)
{
	char buf[1024];
	char seps[] = ",";
	char *token;
	double		fLat = 0.0;
	double		fLng = 0.0;
	int			nCog = 0;
	
	m_cts = cts;

	//strcpy((char*)buf, (char*)pStr);
	strcpy_s((char*)buf, sizeof(buf),(char*)pStr);

	token = StrToken(buf, seps);

	for (int i = 0; i < CountOfGprmcPos; i++)
	{
		if (token == NULL)
		{
			return false;
		}

		switch(i)
		{
		case posTime:
			SetTime(token);
			break;
		case posValid:
			m_bValid = (*token == 'A');
			break;
		case posLatV:
			fLat = (double)atof(token);
			break;
		case posLatNS:
			if (*token != 'N')
			{
				fLat *= -1;
			}
			break;
		case posLngV:
			fLng = (double)atof(token);
			break;
		case posLngEW:
			if (*token != 'E')
			{
				fLng *= -1;
			}
			break;
		case posKnotSpeed:
			m_fSpeed = (float)atof(token);
			break;
		case posAzimuth:
			if(strlen(token)){
				m_nCogCounts++;
				nCog= (int)(((float)atof(token)) * 100);
			}
			else{
				m_nCogCounts = 0;
			}
				
			break;
		case posDate:
			SetDate(token);
			break;
		default:;
		}

		token = StrToken(NULL, seps);
	}

	//AdjustDateTime();
	if(m_bValid) {
		m_fLat = fLat;
		m_fLng = fLng;
		m_nCog = nCog;
		m_nValid_time++;
	}
	else {
		m_nValid_time = 0;
		m_nCogCounts = 0;
	}
	
	return true;
}

bool CGPSData::ParsingRDFGP(char *pStr, unsigned long cts)
{
	char buf[1024];
	char seps[] = ",";
	char *token;

	m_cts = cts;

	//strcpy((char*)buf, (char*)pStr);
	strcpy_s((char*)buf, sizeof(buf),(char*)pStr);

	token = StrToken(buf, seps);

	for (int i = 0; i < CountOfRdfgpPos; i++)
	{
		if (token == NULL)
		{
			return false;
		}

		switch(i)
		{
		case rdfPosDate:
			SetDate(token);
			break;
		case rdfPosTime:
			SetTime(token);
			break;
		case rdfPosMode:
			m_sp_state = (unsigned char)atoi(token);
			break;
		case rdfPosKmhSpeed:
			m_fSpeed = (float)atof(token)  / 1.853184; //Km/h to Knots
			break;
		case rdfPosPdop:
			m_fPdop = (float)atof(token);
			break;
		case rdfPosEvent:
			break;
		case rdfPosCourse:
			m_nCog = atoi(token);
			break;
		case rdfPosLatV:
			m_fLat = (double)atof(token);
			break;
		case rdfPosLatNS:
			if (*token != 'N')
			{
				m_fLat *= -1;
			}
			break;
		case rdfPosLngV:
			m_fLng = (double)atof(token);
			break;
		case rdfPosLngEW:
			if (*token != 'E')
			{
				m_fLng *= -1;
			}
			break;
		default:;
		}

		token = StrToken(NULL, seps);
	}

	if(m_sp_state >= 2 && m_fLat != 0.0 && m_fLng != 0.0)
		m_bValid = TRUE;
	
	//AdjustDateTime();

	return true;
}

char * CGPSData::StrToken(char * s1, const char * delimit)
{
	static char *lastToken = NULL;
	char *tmp;

	if (s1 == NULL)
	{
		s1 = lastToken;
		if (s1 == NULL)
		{
			return NULL;
		}
	}
	else 
	{
		s1 += strspn(s1, delimit);
	}

	tmp = strpbrk(s1, delimit);
	if (tmp)
	{
		*tmp = '\0';
		lastToken = tmp + 1;
	}
	else
	{
		lastToken = NULL;
	}

	return s1;
}

void CGPSData::SetTime(char * pStr)
{
	char buf[3] = {0};

	strncpy_s(buf, pStr, 2);
	m_nHour = atoi(buf) /* + 9 */;
	strncpy_s(buf, pStr+2, 2);
	m_nMinute = atoi(buf);
	strncpy_s(buf, pStr+4, 2);
	m_nSecond = atoi(buf);
}

void CGPSData::SetDate(char * pStr)
{
	char buf[3] = {0};

	strncpy_s(buf, pStr, 2);
	m_nDay = atoi(buf);
	strncpy_s(buf, pStr+2, 2);
	m_nMonth = atoi(buf);
	strncpy_s(buf, pStr+4, 2);
	m_nYear = atoi(buf);
}

int CGPSData::AdjustDateTime(int gmt)
{
	struct tm tmThis;
	time_t t_gps;
	time_t     t_rtc;

	tmThis.tm_year = 70+((m_nYear+30)%100);	// 2019 - 1900
	tmThis.tm_mon = m_nMonth-1;		// Month, 0 - jan
	tmThis.tm_mday = m_nDay;				// Day of the month
	tmThis.tm_hour = m_nHour;
	tmThis.tm_min = m_nMinute;
	tmThis.tm_sec = m_nSecond;

	tmThis.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown

	t_gps = mktime(&tmThis) + (gmt * 60 * 60);
	//t_gps = mktime(&tmThis);
	printf("GPS Time : %ld\n", (long) t_gps);
	// Get current time
    time(&t_rtc);
	printf("RTC Time : %ld\n", (long) t_rtc);
	// Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    //

	if(t_gps >= t_rtc + 2 ||t_gps <= t_rtc - 2){ // +- 15
		char szCmd[128];
#if 0		
		tmThis = *localtime(&t_gps);
#else
		time_t     t_gps_utc = mktime(&tmThis) + (gmt * 60 * 60);

		localtime_r(&t_gps_utc, &tmThis);
#endif

		sprintf(szCmd, "date -s \"%04d-%02d-%02d %02d:%02d:%02d\"", \
			tmThis.tm_year + 1900, \
			tmThis.tm_mon + 1, \
			tmThis.tm_mday, \
			tmThis.tm_hour, \
			tmThis.tm_min, \
			tmThis.tm_sec);

		printf("RTC Time Set : %s\n", szCmd);
		
		system(szCmd);

		system("hwclock -w");
		return 1;
	}
	//m_time = CTime(mktime(&tmThis));

	return 0;
}


double CGPSData::FromDegree(double dbValue)
{
	double d = (int)dbValue;
	double m = ((dbValue - d) * 100.0);

	return d + m / 60.0;
}


double CGPSData::GetLat(void)
{
	if(m_bNotDegree)
		return m_fLat;
	
	return FromDegree(m_fLat * 0.01);
}

double CGPSData::GetLng(void)
{
	if(m_bNotDegree)
		return m_fLng;
	
	return FromDegree(m_fLng * 0.01); 
}
//================================================================================================= E. OPERATION
//================================================================================================= S. CON/DCON

CPulseData::CPulseData()
{
	m_speed_parameter = 80.0; // per km
	m_speed_limit = 20; // km/h
	
	m_nYear = 0;
	m_nMonth = 0;
	m_nDay = 0;
	m_nHour = 0;
	m_nMinute = 0;
	m_nSecond = 0;

	m_bGpsWasConnection = 0;
	m_bGpsConnectionState = 0; //물리적 연결 여부 (1: 연결 / 0: 연결되어 있지 않음)
	m_bGpsSignalState = 0; //GPS 신호 연결여부 (1: 수신 / 0: 수신불가 또는 수신 시도중)
	m_fGpsSpeed = 0.0; //km/h
	m_direction = 0;
	m_directionCounts = 0;
	
	m_bPulseState = 0; //'1' - 펄스 당 속도 계산이 완료된 상태 (이 이후에는 Pulse Speed 내용을 참조하면 된다) '0' - 펄스 당 속도 계산이 아직 완료되지 않은 상태
	m_fPulseSpeed = 0.0; //km/h

	this->m_iPulseSec = 0;
	this->m_iSpdPulse = 0.0;
	this->m_pulse_count_accrue = 0;
	this->m_fDistance = 0.0;
	
	this->m_bBrk = 0;
	this->m_bSR = 0;
	this->m_bSL = 0;
	this->m_bBgr = 0;
	this->m_bInput2Evt = 0;
	this->m_bTR = 0;
	this->m_bInput1Evt = 0;
	this->m_bDcDet = 0;
	
	m_cts = 0;

	pulse_check_count = 0;
	m_speed_parameter_avr = 0.0;

	m_chattering_tm_turn_signal_left = 0;
	m_chattering_tm_turn_signal_right = 0;

	m_chattering_tm_brk_signal = 0;
	m_chattering_tm_trg_signal = 0;
	m_chattering_tm_bgr_signal = 0;
	m_chattering_tm_DC_Det = 0;


	m_iRpmPulseSec = 0;	//현재, 초당 펄스 수
	m_iRpm = 0;
	m_rpm_count_accrue = 0;
	m_rpm_parameter = 4.0; // 4.0 사기통 (m_iRpmPulseSec * 60 * 2 / m_rpm_parameter = RPM)

	this->m_current_bBrk = 0;
	this->m_current_bSR = 0;
	this->m_current_bSL = 0;
	m_current_input1 = 0;
	m_current_input2 = 0;

	m_bPulseReinit = 0;

	datool_ext_speed_limit_out_set(false);
	m_preSpeed_limit_value = false;
}


CPulseData::CPulseData(unsigned char * pStr, unsigned long cts)
{
	CPulseData();
	
	ParsingPulseData(pStr, cts);
}


CPulseData::~CPulseData()
{

}

void CPulseData::operator=(CPulseData &newData)
{
	this->m_nYear = newData.m_nYear;
	this->m_nMonth = newData.m_nMonth;
	this->m_nDay = newData.m_nDay;
	this->m_nHour = newData.m_nHour;
	this->m_nMinute = newData.m_nMinute;
	this->m_nSecond = newData.m_nSecond;
	this->m_bGpsConnectionState = newData.m_bGpsConnectionState;
	this->m_bGpsSignalState = newData.m_bGpsSignalState;
	this->m_fGpsSpeed = newData.m_fGpsSpeed;
	this->m_bPulseState = newData.m_bPulseState;
	this->m_fPulseSpeed = newData.m_fPulseSpeed;

	this->m_iPulseSec =  newData.m_iPulseSec;
	this->m_iSpdPulse = newData.m_iSpdPulse;
	this->m_bBrk = newData.m_bBrk;
	this->m_bSR = newData.m_bSR;
	this->m_bSL = newData.m_bSL;
	this->m_bBgr = newData.m_bBgr;
	this->m_bTR = newData.m_bTR;

	this->m_rpm_count_accrue = newData.m_rpm_count_accrue;
	this->m_iRpmPulseSec = newData.m_iRpmPulseSec;
	this->m_iRpm = newData.m_iRpm;
	
	this->m_bDcDet = newData.m_bDcDet;
	
	this->m_cts = newData.m_cts;
}

bool CPulseData::SetSpeedLimit(void)
{
	bool set = false;
	if((int)m_fPulseSpeed >= m_speed_limit || (m_bGpsSignalState && (int)m_fGpsSpeed >= m_speed_limit)){
		set = true;
	}

	if(m_preSpeed_limit_value != set){
		datool_ext_speed_limit_out_set(set);
		m_preSpeed_limit_value = set;
	}

	return set;
}

bool CPulseData::SetPulseSec(int iPulseSec)
{
	m_iPulseSec = iPulseSec;
#if DEF_PULSE_DEBUG
	printf("C:%d S:%d CN:%d SS:%d\n",iPulseSec, m_bPulseState, m_bGpsConnectionState, m_bGpsSignalState);
#endif	

	if( m_bGpsConnectionState && m_bGpsSignalState && m_fGpsSpeed > 40.0 &&  m_fGpsSpeed < 200.0){
		if(m_bPulseState == 0 || (m_bPulseState && (m_fPulseSpeed < m_fGpsSpeed - 10.0 || m_fPulseSpeed > m_fGpsSpeed + 10.0))){
		
			m_speed_parameter_avr += pulse_speed_to_parameter(m_iPulseSec, m_fGpsSpeed) / DEF_PULSE_CECHK_TIME;
			pulse_check_count++;

			if(pulse_check_count >= DEF_PULSE_CECHK_TIME){
				if(m_bPulseState == 0)
					m_bPulseState = 1;
				else 
					m_bPulseReinit = 1;
					
				pulse_check_count = 0;
				m_speed_parameter = m_speed_parameter_avr;
				m_iSpdPulse = pulse_count_to_speed( 1 , m_speed_parameter);
			}
		}
		else {
			pulse_check_count = 0;
			m_speed_parameter_avr = 0.0;
		}
	}
	else {
		pulse_check_count = 0;
		m_speed_parameter_avr = 0.0;
	}

	if(m_bPulseState){
		m_fPulseSpeed = pulse_count_to_speed(m_iPulseSec, m_speed_parameter);
#if DEF_PULSE_DEBUG		
		printf("count = %d (speed G:%0.2fkm P:%0.2fkm , %0.2f)\n",m_iPulseSec, m_fGpsSpeed, m_fPulseSpeed, m_speed_parameter);
#endif
	}
	
	SetSpeedLimit();
	
	return true;
}

int CPulseData::MakeStringPulseData(char *pStr, int size)
{
	int len = 0;
	time_t t;
	struct tm tm_t;

	time(&t);
	localtime_r(&t, &tm_t);

	len += snprintf(pStr + len, size - len, "P$");
	len += snprintf(pStr + len, size - len, "%01d%01d%03d", this->m_bGpsConnectionState, this->m_bGpsSignalState, (int)(this->m_fGpsSpeed + 0.5));
	len += snprintf(pStr + len, size - len, "%01d%03d%03d%03d", this->m_bPulseState, (int)(this->m_fPulseSpeed + 0.5), this->m_iPulseSec, (int)(this->m_iSpdPulse * 10));
	len += snprintf(pStr + len, size - len, "%01d%01d%01d%01d%01d", this->m_bBrk, this->m_bSR, this->m_bSL, this->m_bBgr, this->m_bTR);

	len += snprintf(pStr + len, size - len, "%01d%04d%05d", m_rpm_count_accrue ? 1 : 0, this->m_iRpmPulseSec, this->m_iRpm);
	
	len += snprintf(pStr + len, size - len, ",%02d%02d%02d%02d%02d%02d\r\n", (tm_t.tm_year + 1900 )% 100, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);

	pStr[len++] = 0;
	pStr[len++] = 0;
	
	return len;
}

bool CPulseData::ParsingPulseData(unsigned char *pStr, unsigned long cts)
{
	char buf[1024];
	char seps[] = ",";
	char *token;

	m_cts = cts;

	//strcpy((char*)buf, (char*)pStr);
	strcpy_s((char*)buf, sizeof(buf),(char*)pStr);

	token = StrToken(buf, seps);

	for (int i = 0; i < 2; i++)
	{
		if (token == NULL)
		{
			return false;
		}

		switch(i)
		{
		case 0:
			this->m_bGpsConnectionState = (BOOL)StrToInt(token + 2, 1);
			this->m_bGpsSignalState = (BOOL)StrToInt(token + 3, 1);
			this->m_fGpsSpeed = (double)StrToInt(token + 4, 3);
			this->m_bPulseState = (BOOL)StrToInt(token + 7, 1);
			this->m_fPulseSpeed = (double)StrToInt(token + 8, 3);
			this->m_iPulseSec = StrToInt(token + 11, 3);
			this->m_iSpdPulse = StrToInt(token + 14, 3) / 100.0;
			
			this->m_bBrk = (BOOL)StrToInt(token + 17, 1);
			this->m_bSR = (BOOL)StrToInt(token + 18, 1);
			this->m_bSL = (BOOL)StrToInt(token + 19, 1);
			this->m_bBgr = (BOOL)StrToInt(token + 20, 1);
			this->m_bTR = (BOOL)StrToInt(token + 21, 1);
			this->m_rpm_count_accrue = (u32)StrToInt(token + 22, 1);
			this->m_iRpmPulseSec = StrToInt(token + 23, 4);
			this->m_iRpm = StrToInt(token + 27, 5);
			break;
		case 1:
			SetTime(token);
			break;
		default:;
		}

		token = StrToken(NULL, seps);
	}

	return true;
}

char * CPulseData::StrToken(char * s1, const char * delimit)
{
	static char *lastToken = NULL;
	char *tmp;

	if (s1 == NULL)
	{
		s1 = lastToken;
		if (s1 == NULL)
		{
			return NULL;
		}
	}
	else 
	{
		s1 += strspn(s1, delimit);
	}

	tmp = strpbrk(s1, delimit);
	if (tmp)
	{
		*tmp = '\0';
		lastToken = tmp + 1;
	}
	else
	{
		lastToken = NULL;
	}

	return s1;
}

int CPulseData::StrToInt(char * s1, int delimit)
{
	char buf[1024];

	if(delimit < (int)sizeof(buf)){
		strncpy(buf, s1, delimit);
		buf[delimit] = 0;

		return atoi(buf);
	}

	return 0;
}

void CPulseData::SetTime(char * pStr)
{
	char buf[3] = {0};

	strncpy_s(buf, pStr, 2);
	m_nYear = atoi(buf);
	strncpy_s(buf, pStr+2, 2);
	m_nMonth = atoi(buf);
	strncpy_s(buf, pStr+4, 2);
	m_nDay = atoi(buf);

	strncpy_s(buf, pStr+6, 2);
	m_nHour = atoi(buf);
	strncpy_s(buf, pStr+8, 2);
	m_nMinute = atoi(buf);
	strncpy_s(buf, pStr+10, 2);
	m_nSecond = atoi(buf);
}


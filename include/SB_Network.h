#ifndef SB_Network_H
#define SB_Network_H

#include <list>

#ifdef __cplusplus
extern "C"
{
#endif

#define WIFI_APMODE 			0
#define WIFI_STATIONMODE		1


typedef struct{
	char strBSS[32];
    int iFreq;
	double dSignal;		//dbm
	char strSSID[64];
}ST_WIFI_STATIONS, *LPST_WIFI_STATIONS;

typedef std::list<ST_WIFI_STATIONS>					WIFI_STATIONS_POOL;
typedef WIFI_STATIONS_POOL::iterator					ITER_WIFI_STATIONS;

int	SB_GetStations(WIFI_STATIONS_POOL &list_st);

int SB_StationsStatus(const char *strBSS, double sys_tick);

bool SB_Net_Is_RNDIS(void);
bool SB_Net_Connectd_check(); //true connected

int getIPAddress(char *ip_addr);
int getSubnetMask(char *sub_addr);
int getBroadcastAddress(char *br_addr);
int SB_GetMacAddress(char *mac);
void convrt_mac(const char *data, char *cvrt_str, int sz);
int getNIC(char * g_NIC);

/*--------------------------------------------------------------------
  Argment 		:  
			dest_IP     =>  destinaton Server IP 
			dest_port   =>  destination port number			
 			Timeout     =>  Wait for CONNECT Time (1 ~ N sec)
  Return Value : 
			1 ~ N	   =>  handle no
			-1		   =>  failed	
---------------------------------------------------------------------*/
int SB_ConnectTcp (char *IP_address, unsigned int socket_no, int wait_sec, int Tx_Size, int Rx_Size);

//------------------------------------------------------------------------
// Argment 		:  
// 			Port	=>  Socket Port Number for CONNECTING
// Return Value : 
//			1 ~ N	=>  handle no
//			-1		=>  failed
//------------------------------------------------------------------------
int SB_ListenTcp (unsigned int socket_no, int Tx_Size, int Rx_Size);
//------------------------------------------------------------------------
// Argment 		:  
// 			s		=>  Socket File Descripter
// 			sec	    =>  How long to wait for the required condition
//			usec    =>	before returning to the caller.
// Return Value : 
//			1 ~ N	=>  handle no
//			0		=>  wsiting
//			-1		=>  failed
//------------------------------------------------------------------------
int SB_AcceptTcp (int fd, int msec);

//------------------------------------------------------------------------
int SB_AcceptTcpMulti (int fd, int msec);
//------------------------------------------------------------------------
// Argment 		:  
// 			LAN_FD	  =>  handle no
//			BUFF	  =>  Receive Workarea 		
//			buff_zise =>  buffer size	
//		
// Return Value :  Receive data length
//			n : read len,     -1 : Lan error (disCONNECT)
//------------------------------------------------------------------------
int SB_ReadTcp (int fd, char *buff, int buff_size);
//=============================================================
void SB_CloseTcp (int fd) ;
//=============================================================
int SB_BindUdp (unsigned int socket_no);
//=============================================================
int SB_ReadUdp (int fd, char *buff, int buffer_size);

//=============================================================
void SB_SendUdpServer (int fd, char *buff, int len);
//=============================================================
void SB_SendUdpClient (int fd, char *buff, int len, char *ServerIP, int ServerPort);
//=============================================================
unsigned int SB_GetIp (char *eth_name);
//=============================================================
unsigned int SB_GetMask (char *eth_name);
//===============================================================================
unsigned int SB_GetGateway ();

int SB_RNDIS_Start(void);

//mode : 0 = AP, 1=Slave
int SB_WiFi_Start(int mode, const char * ssid, const char * pw);

#ifdef __cplusplus
}
#endif

#endif // SB_Network_H
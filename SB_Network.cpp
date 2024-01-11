
//----------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>

#include <net/if.h>
#include <sys/ioctl.h>
 #include <sys/stat.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "sysfs.h"
#include "system_log.h"
#include "SB_System.h"
#include "SB_Network.h"

//debug define
#define DEBUG_LEVEL_	3
//#define DEBUG_LEVEL_	1

#define DEBUG_ERROR 4
#define DEBUG_WARN		3
#define DEBUG_INFO		2
#define DEBUG_MSG		1

#ifdef  DEBUG_LEVEL_
#define dp(n, fmt, args...)	if (DEBUG_LEVEL_ <= n) printf("%s:%d," fmt, __FILE__, __LINE__, ## args)
#define dp0(n, fmt)		if (DEBUG_LEVEL_ <= n) printf("%s:%d," fmt, __FILE__, __LINE__)
#define _dp(n, fmt, args...)	if (DEBUG_LEVEL_ <= n) printf(" " fmt, ## args)
#else	/* DEBUG_LEVEL_ */
#define dp(n, fmt, args...)
#define dp0(n, fmt)
#define _dp(n, fmt, args...)
#endif	/* DEBUG_LEVEL_ */

/*
BSS ae:0d:1b:f3:01:02(on wlan0)
        TSF: 1055644378 usec (0d, 00:17:35)
        freq: 2412
        beacon interval: 100 TUs
        capability: ESS Privacy SpectrumMgmt ShortSlotTime RadioMeasure (0x1511)
        signal: -43.00 dBm
        last seen: 780 ms ago
        Information elements from Probe Response frame:
        SSID: G5_7979
        Supported rates: 6.0* 9.0 12.0* 18.0 24.0* 36.0 48.0 54.0
        DS Parameter set: channel 1
        Country: KR     Environment: Indoor/Outdoor
                Channels [1 - 13] @ 20 dBm
        Power constraint: 0 dB
        TPC report: TX power: 16 dBm
        ERP: <no flags>
        RSN:     * Version: 1
                 * Group cipher: CCMP
                 * Pairwise ciphers: CCMP
                 * Authentication suites: PSK
                 * Capabilities: 16-PTKSA-RC 1-GTKSA-RC (0x000c)
        HT capabilities:
                Capabilities: 0x2d
                        RX LDPC
                        HT20
                        SM Power Save disabled
                        RX HT20 SGI
                        No RX STBC
                        Max AMSDU length: 3839 bytes
                        No DSSS/CCK HT40
                Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
                Minimum RX AMPDU time spacing: 4 usec (0x05)
                HT RX MCS rate indexes supported: 0-15
                HT TX MCS rate indexes are undefined
        HT operation:
                 * primary channel: 1
                 * secondary channel offset: no secondary
                 * STA channel width: 20 MHz
                 * RIFS: 1
                 * HT protection: no
                 * non-GF present: 0
                 * OBSS non-GF present: 0
                 * dual beacon: 0
                 * dual CTS protection: 0
                 * STBC beacon: 0
                 * L-SIG TXOP Prot: 0
                 * PCO active: 0
                 * PCO phase: 0
        Extended capabilities: Extended Channel Switching, BSS Transition, 6
        WPS:     * Version: 1.0
                 * Wi-Fi Protected Setup State: 2 (Configured)
                 * Response Type: 3 (AP)
                 * UUID: fa047214-a61b-5064-8b6d-42c519b5627c
                 * Manufacturer:
                 * Model:
                 * Model Number:
                 * Serial Number:
                 * Primary Device Type: 10-0050f204-5
                 * Device name: G5
                 * Config methods: Display, Keypad
                 * Unknown TLV (0x1049, 6 bytes): 00 37 2a 00 01 20
        WMM:     * Parameter version 1
                 * u-APSD
                 * BE: CW 15-1023, AIFSN 3
                 * BK: CW 15-1023, AIFSN 7
                 * VI: CW 7-15, AIFSN 2, TXOP 3008 usec
                 * VO: CW 3-7, AIFSN 2, TXOP 1504 usec
*/
#define	MESSAGE_BUFFER_SIZE (100 * 1024)

int SB_GetStations(WIFI_STATIONS_POOL &list_st)
{
	char * msg_buffer = NULL;
	char strLine[1024] = { 0,};
	int strLineIndex = 0;
	int msg_size = 0;
	int stations = 0;
	int i;

	ST_WIFI_STATIONS wifi_info;
	memset((void *)&wifi_info, 0, sizeof(wifi_info));

	msg_buffer = new char[  MESSAGE_BUFFER_SIZE ];
	if(msg_buffer == NULL){
		_dp(DEBUG_ERROR, "%s() : memory allocation error!(size:%d)", __func__, MESSAGE_BUFFER_SIZE);
		return 0;
	}
	
	{
		system("ifconfig wlan0 down");

		SB_KillProcessName("hostapd");
		SB_KillProcessName("udhcpc");
		SB_KillProcessName("wpa_supplicant");
	}
	
	system("ifconfig wlan0 up");
	sleep(1);

	msg_size = SB_Cat("iw wlan0 scan\n", msg_buffer, MESSAGE_BUFFER_SIZE);
	for(i=0, strLineIndex=0; i < msg_size; i++){
		if( msg_buffer[i] == '\r' || msg_buffer[i] == '\n'){
			const char * s;
			
			strLine[strLineIndex]  = 0;

			 
				
			s = strstr( strLine, "BSS ");
			if(s) {
				if(sscanf( s, "BSS %s", &wifi_info.strBSS))
					wifi_info.strBSS[17] = 0;
			}

			s = strstr( strLine, "freq: ");
			if(s) {
				
				sscanf( s, "freq: %d", &wifi_info.iFreq);
			}

			s = strstr( strLine, "signal: ");
			if(s) sscanf( s, "signal: %lf", &wifi_info.dSignal);

			s = strstr( strLine, "SSID: ");
			if(s) {
				//sscanf( s, "SSID: %s", wifi_info.strSSID); //공백 문자 처리가 안됨
				//strcpy(wifi_info.strSSID, &s[6]); //Unicode 처리가 안됨
				int i = 0;
				s += 6; //Skip "SSID: "
				do{
					if(s[0] == '\\' && (s[1] == 'x' || s[1] == 'X')){
						char hex[3];
						hex[0] = s[2];
						hex[1] = s[3];
						hex[2] = 0;

						wifi_info.strSSID[i++] = (char)strtoul(hex, NULL, 16);
						s += 4;
					}
					else {
						wifi_info.strSSID[i++] = *s;
						s++;
					}
				}while(*s != 0 && i < sizeof(strLine));

				_dp(DEBUG_WARN, "%d: BSS %s	freq: %d	signal: %lf	SSID: %s\r\n", list_st.size(), wifi_info.strBSS, wifi_info.iFreq, wifi_info.dSignal, wifi_info.strSSID);
				
				list_st.push_back(wifi_info);
				memset((void *)&wifi_info, 0, sizeof(wifi_info));
				stations++;
			}
			
			strLineIndex = 0;
		}
		if(strLineIndex < sizeof(strLine)-1)
			strLine[strLineIndex++] = msg_buffer[i];
	}

	delete [] msg_buffer;
	
	return stations;
}

/*
Successfully initialized wpa_supplicant
Long line in configuration file truncated
nl80211: Could not re-add multicast membership for vendor events: -2 (No such file or directory)
[ 1810.810051] [BH_WRN] miss interrupt!
wlan0: SME: Trying to authentica[ 1811.478287] wlan0: authenticate with ae:0d:1b:f3:01:02
te with ae:0d:1b:f3:01:02 (SSID='G5_7979' freq=2412 MHz)
[ 1811.530357] wlan0: send auth to ae:0d:1b:f3:01:02 (try 1/3)
[ 1811.697894] wlan0: authenticated
wlan0: Trying to associate with ae:0d:1b:f3:01:02 (SSID='G5_7979' freq=2412 MHz)
[ 1811.710197] wlan0: associate with ae:0d:1b:f3:01:02 (try 1/3)
[ 1811.764424] wlan0: RX AssocResp from ae:0d:1b:f3:01:02 (capab=0x431 status=0 aid=1)
[ 1811.774928] [AP_WRN] [STA] ASSOC HTCAP 11N 58
[ 1811.837718] wlan0: associated
wlan0: Associated with ae:0d:1b:f3:01:02
wlan0: CTRL-EVENT-SUBNET-STATUS-UPDATE status=0
[ 1811.964253] [WSM_WRN] Issue unjoin command (RX).
[ 1811.969572] wlan0: deauthenticated from ae:0d:1b:f3:01:02 (Reason: 3)
wlan0: CTRL-EVENT-DISCONNECTED b[ 1812.061463] cfg80211: Calling CRDA for country: KR
ssid=ae:0d:1b:f3:01:02 reason=3
wlan0: WPA: 4-Way Handshake failed - pre-shared key may be incorrect
wlan0: CTRL-EVENT-SSID-TEMP-DISABLED id=0 ssid="G5_7979" auth_failures=1 duration=10 reason=WRONG_KEY
*/

/*
[ 1847.731463] wlan0: deauthenticated from ae:0d:1b:f3:01:02 (Reason: 3)
*/

/* OK
Successfully initialized wpa_supplicant
Long line in configuration file truncated
nl80211: Could not re-add multicast membership for vendor events: -2 (No such file or directory)
wlan0: SME: Trying to authentica[ 2396.662249] wlan0: authenticate with ae:0d:1b:f3:01:02
te with ae:0d:1b:f3:01:02 (SSID='G5_7979' freq=2412 MHz)
[ 2396.700371] wlan0: send auth to ae:0d:1b:f3:01:02 (try 1/3)
[ 2396.867883] wlan0: authenticated
wlan0: Trying to associate with ae:0d:1b:f3:01:02 (SSID='G5_7979' freq=2412 MHz)
[ 2396.880179] wlan0: associate with ae:0d:1b:f3:01:02 (try 1/3)
[ 2396.891966] wlan0: RX AssocResp from ae:0d:1b:f3:01:02 (capab=0x431 status=0 aid=1)
[ 2396.902205] [AP_WRN] [STA] ASSOC HTCAP 11N 58
[ 2396.910888] wlan0: associated
wlan0: Associated with ae:0d:1b:f3:01:02
wlan0: CTRL-EVENT-SUBNET-STATUS-UPDATE status=0
wlan0: WPA: Key negotiation completed with ae:0d:1b:f3:01:02 [PTK=CCMP GTK=CCMP]
wlan0: CTRL-EVENT-CONNECTED - Connection to ae:0d:1b:f3:01:02 completed [id=0 id_str=]
*/

int SB_StationsStatus(const char *strBSS, double sys_tick)
{
	const char *messages_path = "/tmp/messages";
	char strMsgBSS[32] = {0,};
	int iReason = 0;
	double dMsgTick = 0.0;

	int result = 0;
	
	char msg[1024 * 5] = {0,};
	int offset = 0;
	int msg_size = 0;
	int i;
	int messages_size = sysfs_getsize(messages_path);
	if(messages_size > sizeof(msg) / 2)
		offset = messages_size - (sizeof(msg) / 2);

	msg_size = sysfs_read_offset(messages_path, msg, sizeof(msg), offset);

	//dp(DEBUG_INFO, "%s (size %d) read %d byte\r\n", messages_path, messages_size, msg_size);
	
	if(msg_size){
		char strLine[256] = { 0,};
		int strLineIndex = 0;

		for(i=0, strLineIndex=0; i < msg_size; i++){
			if( msg[i] == '\r' || msg[i] == '\n'){
				const char * s;
				
				strLine[strLineIndex]  = 0;
				strLineIndex = 0;
				//dp(DEBUG_INFO, "%s\r\n", strLine);
				
				s = strstr( strLine, "wlan0: deauthenticated from ");
				if(s) {
					s = strstr( strLine, "[ ");
					if(sscanf( s, "[ %lf] wlan0: deauthenticated from %s  (Reason: %d)", &dMsgTick, strMsgBSS, &iReason)){

						//dp(DEBUG_INFO, "%s [%lf	BSS %s[%lf %s]	(Reason: %d)]\r\n", s, dMsgTick, strMsgBSS, sys_tick, strBSS, iReason);
						
						if(sys_tick < dMsgTick && strcmp(strBSS, strMsgBSS) == 0){
							result = iReason;
							_dp(DEBUG_ERROR, "%lf	BSS %s	(Reason: %d)\r\n", dMsgTick, strMsgBSS, iReason);
							break;
						}
					}
				}
			}
			else if(strLineIndex < sizeof(strLine)-1)
				strLine[strLineIndex++] = msg[i];
		}
	}

	return result;
}

bool SB_Net_Is_RNDIS(void)
{
	if(access("/sys/class/net/usb0", F_OK) == 0 || access("/sys/class/net/eth0", F_OK) == 0){
		return 1;
	}	
}

// Link Check
bool SB_Net_Connectd_check()
{
	char msg[256];
	char cmd[256];

	if(SB_Net_Is_RNDIS()){
		//strcpy(cmd, "iw usb0 link\n");

		///////////////////////
		return 1;
	}	
	else if(access("/sys/class/net/wlan0", F_OK) == 0){
		strcpy(cmd, "iw wlan0 link\n");
	
		if(SB_Cat(cmd, msg, sizeof(msg))){
			int result = strncmp("Not connected.", msg, 10);
			//printf("%s [%d]\r\n", msg, result);
			//printf("%s [%d]\r\n", __func__, result);

			if(result != 0)
				return true;

		}
	}
	return false;
}

//IP 정보 가져오기
int getIPAddress(char *ip_addr)
{
	int sock;
	struct ifreq ifr;
	struct sockaddr_in *sin;

	sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock < 0) 
	{
		dp(4, "socket");
		return 0;
	}

	if(access("/sys/class/net/usb0", F_OK) == 0)
		strcpy(ifr.ifr_name, "usb0");
	else if(access("/sys/class/net/eth0", F_OK) == 0)
		strcpy(ifr.ifr_name, "eth0");
	else
		strcpy(ifr.ifr_name, "wlan0");
	
	if (ioctl(sock, SIOCGIFADDR, &ifr)< 0)    
	{
		dp(4, "ioctl() - get ip");
		close(sock);
		return 0;
	}

	sin = (struct sockaddr_in*)&ifr.ifr_addr;
	strcpy(ip_addr, inet_ntoa(sin->sin_addr));

	close(sock);
	return 1;

}

//Subnetmask 정보 가져오기
int getSubnetMask(char *sub_addr)
{
	int sock;
	struct ifreq ifr;
	struct sockaddr_in *sin;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) 
	{
		dp(4, "socket");
		return 0;
	}

	if(access("/sys/class/net/usb0", F_OK) == 0)
		strcpy(ifr.ifr_name, "usb0");
	else if(access("/sys/class/net/eth0", F_OK) == 0)
		strcpy(ifr.ifr_name, "eth0");
	else
		strcpy(ifr.ifr_name, "wlan0");
	
	if (ioctl(sock, SIOCGIFNETMASK, &ifr)< 0)    
	{
		dp(4, "ioctl() - get subnet");
		close(sock);
		return 0;
	}
	
	sin = (struct sockaddr_in*)&ifr.ifr_addr;
	strcpy(sub_addr, inet_ntoa(sin->sin_addr));

	close(sock);
	return 1;

}

//Broadcast 정보 가져오기
int getBroadcastAddress(char *br_addr)
{
	int sock;
	struct ifreq ifr;
	struct sockaddr_in *sin;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) 
	{
		dp(4, "socket");
		return 0;
	}

	if(access("/sys/class/net/usb0", F_OK) == 0)
		strcpy(ifr.ifr_name, "usb0");
	else if(access("/sys/class/net/eth0", F_OK) == 0)
		strcpy(ifr.ifr_name, "eth0");
	else
		strcpy(ifr.ifr_name, "wlan0");
	
	if (ioctl(sock, SIOCGIFBRDADDR, &ifr)< 0)    
	{
		dp(4, "ioctl() - get subnet");
		close(sock);
		return 0;
	}

	sin = (struct sockaddr_in*)&ifr.ifr_addr;
	strcpy(br_addr, inet_ntoa(sin->sin_addr));
	close(sock);
	return 1;
}

//Mac address 정보 가져오기
int SB_GetMacAddress(char *mac)
{
	int sock;
	struct ifreq ifr;
	char mac_adr[18] = {0,};

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) 
	{
		dp(4, "socket");
		return 0;
	}
	
	if(access("/sys/class/net/usb0", F_OK) == 0)
		strcpy(ifr.ifr_name, "usb0");
	else if(access("/sys/class/net/eth0", F_OK) == 0)
		strcpy(ifr.ifr_name, "eth0");
	else
		strcpy(ifr.ifr_name, "wlan0");
	
	if (ioctl(sock, SIOCGIFHWADDR, &ifr)< 0)    
	{
		dp(4, "ioctl() - get mac");
		close(sock);
		return 0;
	}

	//convert format ex) 00:00:00:00:00:00
	convrt_mac( ether_ntoa((struct ether_addr *)(ifr.ifr_hwaddr.sa_data)), mac_adr, sizeof(mac_adr) -1 );

	strcpy(mac, mac_adr);

	close(sock);
	return 1;
}


//보기 편한 포맷으로 Mac address를 변경해준다. - 함수 출처 : http://hkpco.kr/code/netinfo.c
void convrt_mac(const char *data, char *cvrt_str, int sz)
{
     char buf[128] = {0,};
     char t_buf[8];
     char *stp = strtok( (char *)data , ":" );
     int temp=0;

     do
     {
          memset( t_buf, 0, sizeof(t_buf) );
          sscanf( stp, "%x", &temp );
          snprintf( t_buf, sizeof(t_buf)-1, "%02X", temp );
          strncat( buf, t_buf, sizeof(buf)-1 );
          strncat( buf, ":", sizeof(buf)-1 );
     } while( (stp = strtok( NULL , ":" )) != NULL );

     buf[strlen(buf) -1] = '\0';
     strncpy( cvrt_str, buf, sz );
}


int getNIC(char * g_NIC) //eth0와 같은 NIC을 가져온다.
{
	int sock;
	struct ifconf ifconf;
	struct ifreq ifr[50];
	int ifs;
	int i;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) 
	{
		dp(4, "socket");
		return 0;

	}

	ifconf.ifc_buf = (char *) ifr;
	ifconf.ifc_len = sizeof ifr;

	if (ioctl(sock, SIOCGIFCONF, &ifconf) == -1) 
	{
		dp(4, "ioctl");
		return 0;
	}

	ifs = ifconf.ifc_len / sizeof(ifr[0]);
	//printf("interfaces = %d:\n", ifs);
	for (i = 0; i < ifs; i++) 
	{
		if(strcmp(ifr[i].ifr_name, "lo") != 0) //'lo'를 제외한 나머지 NIC을 가져온다.
		{
			strcpy(g_NIC, ifr[i].ifr_name);
			break;
		}
	}

	close(sock);
	return 1;
}

//----------------------------------------------------------------

#if 0
struct sockaddr_in SB_UDP_ADDR;

int SB_CheckConnect (int fd, struct sockaddr *saptr, socklen_t salen, long wait_sec);
/*--------------------------------------------------------------------
  Argment 		:  
			dest_IP     =>  destinaton Server IP 
			dest_port   =>  destination port number			
 			Timeout     =>  Wait for CONNECT Time (1 ~ N sec)
  Return Value : 
			1 ~ N	   =>  handle no
			-1		   =>  failed	
---------------------------------------------------------------------*/
int SB_ConnectTcp (char *IP_address, unsigned int socket_no, int wait_sec, int Tx_Size, int Rx_Size)
{																											
struct sockaddr_in  servaddr;
int fd, opt_val;
struct linger ling;

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0) return (-1);
	
	ling.l_onoff = 1;
	ling.l_linger = 0;
	setsockopt (fd, SOL_SOCKET, SO_LINGER, 	 &ling, sizeof(ling));
	opt_val = 1;
	setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, &opt_val, sizeof(int));
	
	if (Tx_Size <= 0) Tx_Size = 4;
	if (Tx_Size > 64) Tx_Size = 64;	
	opt_val = 1024 * Tx_Size;
	setsockopt (fd, SOL_SOCKET, SO_SNDBUF, &opt_val, sizeof(int));
	if (Rx_Size <= 0) Rx_Size = 4;
	if (Rx_Size > 64) Rx_Size = 64;	
	opt_val = 1024 * Rx_Size;
	setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &opt_val, sizeof(int));		
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(socket_no);
	inet_pton(AF_INET, (char *)IP_address, &servaddr.sin_addr);

	if (SB_CheckConnect (fd, (struct sockaddr *) &servaddr, sizeof(servaddr), wait_sec) < 0) 
		{    	
		close(fd);
		return (-1);
		}
	else 
		return (fd);
}
//---------------------------------------------------------------------
int SB_CheckConnect (int fd, struct sockaddr *saptr, socklen_t salen, long wait_sec)
//---------------------------------------------------------------------
{
//int flags;
int n, error=0;
socklen_t len;
fd_set rset, wset;
struct timeval tval;

	//flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, O_NONBLOCK);

	if ((n = connect (fd, saptr, salen)) < 0) 
		if (errno != EINPROGRESS) return (-1); 
	if (n == 0) goto done;
    
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	wset = rset;
	tval.tv_sec  = wait_sec;
	tval.tv_usec = 0;
	if (select (fd + 1, &rset, &wset, NULL, &tval) < 0)
		{
		errno = ETIMEDOUT;
		return (-1);
		}
	if (FD_ISSET(fd, &rset) || FD_ISSET(fd, &wset)) 
		{
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)	return (-1);
		}

done :
	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (error) 
		{
		errno = error;
		return (-1);
    	}
    if ((n = connect (fd, saptr, salen)) < 0) return (-1);
	return (0);
}
//------------------------------------------------------------------------
// Argment 		:  
// 			Port	=>  Socket Port Number for CONNECTING
// Return Value : 
//			1 ~ N	=>  handle no
//			-1		=>  failed
//------------------------------------------------------------------------
int SB_ListenTcp (unsigned int socket_no, int Tx_Size, int Rx_Size)
{
int		fd;
struct	sockaddr_in	svr;
int opt_val;
struct linger ling;

  	if ( (fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) 
		{
		close (fd);
		return -1;
		}

	ling.l_onoff = 1;
	ling.l_linger = 0;
	setsockopt (fd, SOL_SOCKET, SO_LINGER, 			 &ling, sizeof(ling));
	opt_val = 1;
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int));
	opt_val = 1;
	setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, &opt_val, sizeof(int));
	if (Tx_Size <= 0) Tx_Size = 4;
	if (Tx_Size > 64) Tx_Size = 64;	
	opt_val = 1024 * Tx_Size;
	setsockopt (fd, SOL_SOCKET, SO_SNDBUF, &opt_val, sizeof(int));
	if (Rx_Size <= 0) Rx_Size = 4;
	if (Rx_Size > 64) Rx_Size = 64;	
	opt_val = 1024 * Rx_Size;
	setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &opt_val, sizeof(int));		
		
	bzero(&svr, sizeof(svr));
	svr.sin_family = AF_INET;
	svr.sin_addr.s_addr = htonl(INADDR_ANY);
	svr.sin_port = htons(socket_no);
	
	if (bind (fd, (struct sockaddr *)&svr, sizeof(svr)) < 0) 
		{
		close (fd);
		return -1;
		}

	if ( listen (fd, 1) == -1 )
		{
		close (fd);
		return -1;
		}
	return fd;
}
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
int SB_AcceptTcp (int fd, int msec)
{
fd_set		lfd_set;
int			new_fd;
struct		timeval timeout;
socklen_t	soc_len;
struct		sockaddr_in client;
int ret;

	timeout.tv_sec  = (msec / 1000);
	timeout.tv_usec = (msec % 1000);
	FD_ZERO ( &lfd_set );
	FD_SET   (fd, &lfd_set );
	ret = select( fd+1, &lfd_set, NULL, NULL, & timeout);
	if (ret == 0) return 0;	
	if (ret <  0)
		{
//		shutdown (fd, SHUT_RDWR);
//		close (fd);
		return -1;
		}	
		
	if ( FD_ISSET (fd, &lfd_set)) 
		{
		new_fd = accept (fd, (struct sockaddr *)&client, &soc_len);
		if (new_fd == -1) return 0;
		shutdown (fd, SHUT_RDWR);
		close (fd);
			
		fcntl(new_fd, F_SETFL, O_NDELAY);
		return new_fd;
		}
	return 0;
}
//------------------------------------------------------------------------
int SB_AcceptTcpMulti (int fd, int msec)
{
fd_set		lfd_set;
int			new_fd;
struct		timeval timeout;
socklen_t	soc_len;
struct		sockaddr_in client;
int ret;

	timeout.tv_sec  = (msec / 1000);
	timeout.tv_usec = (msec % 1000);
	FD_ZERO ( &lfd_set );
	FD_SET   (fd, &lfd_set );
	ret = select( fd+1, &lfd_set, NULL, NULL, & timeout);
	if (ret == 0) return 0;	
	if (ret <  0) return -1;
		
	if ( FD_ISSET (fd, &lfd_set)) 
		{
		new_fd = accept (fd, (struct sockaddr *)&client, &soc_len);
		if (new_fd == -1) return 0;
		fcntl(new_fd, F_SETFL, O_NDELAY);
		return new_fd;
		}
	return 0;
}
//------------------------------------------------------------------------
// Argment 		:  
// 			LAN_FD	  =>  handle no
//			BUFF	  =>  Receive Workarea 		
//			buff_zise =>  buffer size	
//		
// Return Value :  Receive data length
//			n : read len,     -1 : Lan error (disCONNECT)
//------------------------------------------------------------------------
int SB_ReadTcp (int fd, char *buff, int buff_size)
{
struct timeval timeout;
fd_set  readable;
int len, ret;    

	FD_ZERO (&readable);
	FD_SET  (fd, &readable); 
	timeout.tv_sec   = 0;
	timeout.tv_usec = 1;

	ret = select(fd+1, &readable, NULL, NULL, &timeout);
	if (ret == 0) return 0;				// timeout
	if (ret <  0)						// select error
		{
//		shutdown (fd, SHUT_RDWR);
//		close (fd);
		return (-1);
		}
    
	len = read (fd, buff, buff_size);
	if (len <  0) return 0;
	if (len == 0)
		{
//		shutdown (fd, SHUT_RDWR);
//		close (fd);
		return (-1);
		}
	else
		return len;
}		
//=============================================================
void SB_CloseTcp (int fd) 
{
	shutdown (fd, SHUT_RDWR);
	close (fd);
}
//=============================================================
int SB_BindUdp (unsigned int socket_no)
{
int fd;
struct   sockaddr_in my_addr;

   fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);	
   if(fd == -1)
           {
           perror("socket");
           return 0;
           }

   my_addr.sin_family = AF_INET;
   my_addr.sin_port = htons(socket_no);
   my_addr.sin_addr.s_addr = INADDR_ANY;
   if (bind(fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)))
           {
           printf ("bind error");
           close (fd);
           return 0;
           }
   return fd;
}
//=============================================================
int SB_ReadUdp (int fd, char *buff, int buffer_size)
{
int ret;
struct   timeval  tv;
fd_set   rfds;
socklen_t  DEST_LEN;

   FD_ZERO(&rfds);
   FD_SET(fd, &rfds);
   tv.tv_sec  = 0;
   tv.tv_usec = 1;
   ret = select(fd+1, &rfds, (fd_set *) NULL, (fd_set *) NULL, &tv);
   switch (ret)
      {
      case -1 :               // sock err
          return -1;
      case 0 :                // Event not found
          return 0;
      default :               // sa_data
          if (FD_ISSET(fd, &rfds))
              {
              DEST_LEN = sizeof(SB_UDP_ADDR);
              ret = recvfrom( fd, buff, buffer_size, 0, (struct sockaddr *)&SB_UDP_ADDR, &DEST_LEN);
              return ret;
              }
          return 0;
      }
}
//=============================================================
void SB_SendUdpServer (int fd, char *buff, int len)
{
    sendto(fd, buff, len, 0, (struct sockaddr*)&SB_UDP_ADDR, sizeof(SB_UDP_ADDR));
}
//=============================================================
void SB_SendUdpClient (int fd, char *buff, int len, char *ServerIP, int ServerPort)
{
struct sockaddr_in saddr; 

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(ServerPort);
	saddr.sin_addr.s_addr = inet_addr((char *)ServerIP);
	if((int)saddr.sin_addr.s_addr == -1) return;
    sendto(fd, buff, len, 0, (struct sockaddr*)&saddr, sizeof(saddr));
}
//=============================================================
unsigned int SB_GetIp (char *eth_name)
{
int fd;
unsigned int ip=0;
struct ifreq ifr;						
struct sockaddr_in *pAddr;
 
	fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0)
   		{    	
		strcpy (ifr.ifr_name, eth_name);
		ioctl(fd, SIOCGIFADDR, &ifr);
		pAddr = (struct sockaddr_in *)&ifr.ifr_addr;
		ip = (unsigned int)pAddr->sin_addr.s_addr;
//		printf("IP Address : %s %x\n", inet_ntoa(pAddr->sin_addr), ip);
		close(fd);
		}
	return ip;
}
//=============================================================
unsigned int SB_GetMask (char *eth_name)
{
int fd;
unsigned int ip=0;
struct ifreq ifr;						
struct sockaddr_in *pAddr;
 
	fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0)
   		{    	
		strcpy (ifr.ifr_name, eth_name);
		ioctl(fd, SIOCGIFNETMASK, &ifr);
		pAddr = (struct sockaddr_in *)&ifr.ifr_addr;
		ip = (unsigned int)pAddr->sin_addr.s_addr;
//		printf("Mask Address : %s %x\n", inet_ntoa(pAddr->sin_addr), ip);
		close(fd);
		}
	return ip;
}
//===============================================================================
unsigned int SB_GetGateway ()
{
char buff[256];
int  nl = 0 ;
struct in_addr dest;
int flgs, ref, use, metric;
unsigned long int d,m,g=0;

    FILE *fp = fopen("/proc/net/route", "r");
    while( fgets(buff, sizeof(buff), fp) != NULL ) 
		{
		if(nl) 
			{
			int ifl = 0;
			while(buff[ifl]!=' ' && buff[ifl]!='\t' && buff[ifl]!='\0') ifl++;
			buff[ifl]=0;    /* interface */
			if(sscanf(buff+ifl+1, "%lx%lx%X%d%d%d%lx", &d, &g, &flgs, &ref, &use, &metric, &m)!=7) break;
			dest.s_addr = d;
			if (dest.s_addr == 0)  	break;
			}
		nl++;
    	}
	return g;
}
#endif

//==============================
int SB_RNDIS_Start(void)
{
	char cmd[2048];
	char device[64];
	
	if(access("/sys/class/net/usb0", F_OK) == 0)
		strcpy(device, "usb0");
	else if(access("/sys/class/net/eth0", F_OK) == 0)
		strcpy(device, "eth0");
	else{
		_dp(DEBUG_WARN, "%s() : USB Network module not connected.\r\n", __func__);
		return 0;
	}

	SYSTEMLOG(LOG_TYPE_COMM, LOG_COMM_WIFI, 0, "RNDIS");
	
	_dp(DEBUG_WARN, "[RNDIS MODE] RUN!\r\n");
	

	sprintf(cmd, "ifconfig %s down", device);
	system(cmd);
	SB_KillProcessName("udhcpc");

	sprintf(cmd, "ifconfig %s up", device);
	system(cmd);
	sleep(3);

	sprintf(cmd, "udhcpc -i %s &", device);
	dp(DEBUG_INFO, "%s\r\n", cmd);
	system(cmd);

	return 1;
}

//mode : 0 = WIFI_APMODE, 1=WIFI_STATIONMODE
int SB_WiFi_Start(int mode, const char * ssid, const char * pw)
{
/*
ctrl_interface=/var/run/wpa_supplicant
ap_scan=1

network={
#       ssid="HyperDev"
#       proto=WPA
#       key_mgmt=WPA-PSK
#       pairwise=CCMP
#       group=CCMP
#       psk=db3d45c93c931fed959af8535afff3c1cacf4c356d0b6b0e4be8368e18bd4948
        ssid="SK_WiFiGIGA6989"
        psk="1603078128"
#       ssid="DATECH2"
#       psk="da77797779"
        id_str="PAIR"
}
*/
	int fd = 0;
	char cmd[2048];
	//const char * hostapd_pid_file = "/var/run/hostapd.pid";
	//const char * wpa_supplicant_pid_file = "/var/run/wpa_supplicant.pid";
	//const char * udhcpc_pid_file = "/var/run/udhcpc.pid";

	if(access("/sys/class/net/wlan0", F_OK) != 0){
		_dp(DEBUG_WARN, "%s() : WiFi module not connected.\r\n", __func__);
		return 0;
	}
	
	SYSTEMLOG(LOG_TYPE_COMM, LOG_COMM_WIFI, 0, "%s (%s)", mode == WIFI_APMODE ? "AP" : "ST", ssid);
	
	_dp(DEBUG_WARN, "[WIFI %s MODE] RUN!, ssid = %s \r\n", mode == WIFI_APMODE ? "AP" : "STATION", ssid);
	
	system("ifconfig wlan0 down");

	SB_KillProcessName("hostapd");
	SB_KillProcessName("udhcpc");
	SB_KillProcessName("wpa_supplicant");
#if 0	
	if ( access(hostapd_pid_file, R_OK ) == 0){
		SB_KillProcessFromPid(hostapd_pid_file);
	}
	if ( access(wpa_supplicant_pid_file, R_OK ) == 0){
		SB_KillProcessFromPid(wpa_supplicant_pid_file);
	}
	if ( access(udhcpc_pid_file, R_OK ) == 0){
		SB_KillProcessFromPid(udhcpc_pid_file);
	}
#endif
	system("ifconfig wlan0 up");
	sleep(1);
	
	if(mode == WIFI_APMODE) // AP mode
	{
		SB_KillProcessName("dnsmasq");

		
		const char * hostapd_conf_file = "/tmp/hostapd.conf";
		
		fd = open(hostapd_conf_file, O_WRONLY | O_CREAT);
		if (fd > 0)
		{
			int len = 0;
			//ssid=DA_BLACKBOX
			const char * hostapd_conf = { \
				"interface=wlan0\n" \
				"driver=nl80211\n" \
				"logger_syslog=-1\n" \
				"logger_syslog_level=2\n" \
				"logger_stdout=-1\n" \
				"logger_stdout_level=2\n" \
				"ctrl_interface=/var/run/hostapd\n" \
				"ctrl_interface_group=0\n" \
				"utf8_ssid=1\n" \
				"country_code=KR\n" \
				"hw_mode=g\n" \
				"channel=1\n" \
				"beacon_int=100\n" \
				"dtim_period=2\n" \
				"max_num_sta=255\n" \
				"rts_threshold=-1\n" \
				"fragm_threshold=-1\n" \
				"macaddr_acl=0\n" \
				"auth_algs=3\n" \
				"ignore_broadcast_ssid=0\n" \
				"wmm_enabled=1\n" \
				"wmm_ac_bk_cwmin=4\n" \
				"wmm_ac_bk_cwmax=10\n" \
				"wmm_ac_bk_aifs=7\n" \
				"wmm_ac_bk_txop_limit=0\n" \
				"wmm_ac_bk_acm=0\n" \
				"wmm_ac_be_aifs=3\n" \
				"wmm_ac_be_cwmin=4\n" \
				"wmm_ac_be_cwmax=10\n" \
				"wmm_ac_be_txop_limit=0\n" \
				"wmm_ac_be_acm=0\n" \
				"wmm_ac_vi_aifs=2\n" \
				"wmm_ac_vi_cwmin=3\n" \
				"wmm_ac_vi_cwmax=4\n" \
				"wmm_ac_vi_txop_limit=94\n" \
				"wmm_ac_vi_acm=0\n" \
				"wmm_ac_vo_aifs=2\n" \
				"wmm_ac_vo_cwmin=2\n" \
				"wmm_ac_vo_cwmax=3\n" \
				"wmm_ac_vo_txop_limit=47\n" \
				"wmm_ac_vo_acm=0\n" \
				"ap_max_inactivity=180\n" \
				"skip_inactivity_poll=1\n"
				"ieee80211n=1\n" \
				"eapol_key_index_workaround=0\n" \
				"own_ip_addr=127.0.0.1\n" \
			};
						
			lseek (fd, 0, 0);
			len = sprintf (cmd, "ssid=%s\n%s", ssid, hostapd_conf);
			
			dp(DEBUG_INFO, "%s\r\n", cmd);
			
			write(fd, cmd, len+1);
			close(fd);
			chmod (hostapd_conf_file, S_IWUSR|S_IRUSR);

			sprintf(cmd, "hostapd -dd %s &", hostapd_conf_file);
		}
		else
			sprintf(cmd, "hostapd -dd /data/etc/hostapd.conf &");

		dp(DEBUG_INFO, "%s\r\n", cmd);
		system(cmd);

		sprintf(cmd, "ifconfig wlan0 192.168.10.1 netmask 255.255.255.0");
		dp(DEBUG_INFO, "%s\r\n", cmd);
		system(cmd);

		sprintf(cmd, "dnsmasq --no-daemon --no-resolv --no-poll --dhcp-range=192.168.10.10,192.168.10.200,1h &");
		dp(DEBUG_INFO, "%s\r\n", cmd);
		system(cmd);
		
		return 1;
	}
	else 
	{
		const char * wpa_supplicant_conf_file = "/tmp/wpa_supplicant.conf";

		system("rm /tmp/wpa_supplicant.conf");
		
		fd = open(wpa_supplicant_conf_file, O_WRONLY | O_CREAT);
		if (fd > 0)
		{
			int len = 0;
			lseek (fd, 0, 0);
			len = sprintf (cmd, \
				"ctrl_interface=/var/run/wpa_supplicant\n" \
				"ap_scan=1\n" \
				"network={\n" \
				"	ssid=\"%s\"\n" \
				, ssid);

			if(strlen(pw) > 63) {
				len += sprintf(&cmd[len], 	"	psk=%s\n}\n", pw);
			}
			else if(strlen(pw) > 2) {
				len += sprintf(&cmd[len], 	"	psk=\"%s\"\n}\n", pw);
			}
			else {
				len += sprintf(&cmd[len], 	"	key_mgmt=NONE\n}\n");
			}

			dp(DEBUG_INFO, "%s\r\n", cmd);
			
			write(fd, cmd, len+1);
			close(fd);
			chmod (wpa_supplicant_conf_file, S_IWUSR|S_IRUSR);

			sprintf(cmd, "wpa_supplicant -iwlan0 -Dnl80211 -c %s &", wpa_supplicant_conf_file);
			dp(DEBUG_INFO, "%s\r\n", cmd);
			system(cmd);

			sleep(3);
			
			sprintf(cmd, "udhcpc -i wlan0 &");
			dp(DEBUG_INFO, "%s\r\n", cmd);
			system(cmd);

			return 1;
		}		
	}

	return 0;
}

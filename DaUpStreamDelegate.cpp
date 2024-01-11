
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <string.h>

#include <thread>
#include <mutex>
#include <memory>

#include "daversion.h"

#include "DaUpStreamDelegate.h"

#if ENABLE_NETWORKING

//default port numbers
//default port numbers
#ifdef NO_ROOT_GROUP
static uint16_t _rtsp_server_port = 5454;
static uint16_t _http_server_port = 8080;
#define HTTP_ROOT "./"
#define RTSP_ROOT "./"
#else
static uint16_t _rtsp_server_port = 554;
//static uint16_t _http_server_port = 80;
static uint16_t _http_server_port = 8080;
//#define HTTP_ROOT "/data/home"
//#define RTSP_ROOT "/data/home"
#define HTTP_ROOT "/mnt/extsd"
#define RTSP_ROOT "/mnt/extsd"
#endif

#if 0
#define STUN_SERVER "stun.stunprotocol.org"
#else
#define STUN_SERVER "218.38.18.70"
#endif
#define STUN_PORT "3478"

static bool continue_working = true;

#define BATTERY_ONLINE_FILE		"/sys/class/power_supply/battery/online"
#define BATTERY_CAPACITY		"/sys/class/power_supply/battery/capacity"
#define BATTERY_STATUS		"/sys/class/power_supply/battery/status"
#define AC_ONLINE_FILE			"/sys/class/power_supply/ac/online"
#define USB_ONLINE				"/sys/class/power_supply/usb/online"


static int32_t http_id = -1;
static oasis::RtspServerRef rtsp_server;
static int32_t rtsp_is_play = 0;
//std::thread t;

static std::mutex battery_lock_;
static int32_t battery_cap_ = 0;
static bool battery_online_ = false;
static std::string battery_status_ = "Full";


#if ENABLE_HTTP	

DaUpStreamDelegate::DaUpStreamDelegate()
{
	proximitor_distance_ = 0;
}

DaUpStreamDelegate::~DaUpStreamDelegate()
{
}

void DaUpStreamDelegate::onRequest(const oasis::NetMessageRef &msg)
{
	oasis::key_value_map_t parameters;
	std::string uri_path;
	
	oasis::netMessageUriGetPath(msg, uri_path);
	oasis::netMessageUriGetParameters(msg, parameters);
		
	fprintf(stdout, "%s: uri is \"%s\". parameters#%zu\n", __FUNCTION__, uri_path.c_str(), parameters.size());

	for(oasis::key_value_map_t::iterator item = parameters.begin() ; item != parameters.end(); item++){
		fprintf(stdout, " %s:%s\n", item->first.c_str(), item->second.c_str());
	}
	
	//check the document root path.
	if (strcmp(uri_path.c_str(), "command") && strcmp(uri_path.c_str(), "/command")) {
		#if 0
		std::string help;

		help += "<h2>Streaming URLs</h2>";

		help += oasis::format("<p>1080p@30fps<br>");
		help += oasis::format("<a href=\"rtsp://%s:%d/resolution=1080p&fps=30&rotate=&status=on/Live/0\">rtsp://%s:%d/resolution=1080p&fps=30&rotate=&status=on/Live/0</a>", oasis::getLocalAddressString(context), _rtsp_server_port, oasis::getLocalAddressString(context), _rtsp_server_port);

		help += oasis::format("<p>1080p@30fps, rotate in 270 degree<br>");
		help += oasis::format("<a href=\"rtsp://%s:%d/resolution=1080p&fps=30&rotate=270&status=on/Live/0\">rtsp://%s:%d/resolution=1080p&fps=30&rotate=270&status=on/Live/0</a>", oasis::getLocalAddressString(context), _rtsp_server_port, oasis::getLocalAddressString(context), _rtsp_server_port);

		help += oasis::format("<p>720p@30fps<br>");
		help += oasis::format("<a href=\"rtsp://%s:%d/resolution=720p&fps=30&rotate=&status=on/Live/0\">rtsp://%s:%d/resolution=720p&fps=30&rotate=&status=on/Live/0</a>", oasis::getLocalAddressString(context), _rtsp_server_port, oasis::getLocalAddressString(context), _rtsp_server_port);

		help += oasis::format("<p>720@30fps, rotate in 90 degree<br>");
		help += oasis::format("<a href=\"rtsp://%s:%d/resolution=720p&fps=30&rotate=90&status=on/Live/0\">rtsp://%s:%d/resolution=720p&fps=30&rotate=90&status=on/Live/0</a>", oasis::getLocalAddressString(context), _rtsp_server_port, oasis::getLocalAddressString(context), _rtsp_server_port);

		oasis::sendResponseMessage(context, 400, "Bad Request", "text/html", help.size(), (char*)help.c_str());
		#else
		oasis::netMessageSetStatusCodeAndReasonString(msg, 400, "Bad Request");
		#endif
		return;
	}

	std::string cmd = parameters["cmd"];
	oasis::NetMessageRef request_msg = oasis::netMessageCreate(msg, false);
	
	if (cmd == "recording") {
		//other parameters
		oasis::key_value_map_t::iterator it;
		//int32_t i;
		//const char *keys[] = { "cmd", "resolution", "rotate", "fps", "status" };
		Json::Value root;
		std::string rtsp_parameters;
		char url[PATH_MAX];
		std::string local_address_string;
		std::string local_port_string;

		for(it = parameters.begin(); it != parameters.end(); it++) {
			fprintf(stdout, "%s=%s\n", it->first.c_str(), it->second.c_str());
			root[it->first.c_str()] = it->second.c_str();
			if (rtsp_parameters.size() > 0) {
				rtsp_parameters += "&";
			}
			rtsp_parameters += it->first.c_str();
			rtsp_parameters += "=";
			rtsp_parameters += it->second.c_str();
		}

		oasis::netMessageLocalAddressString(msg, local_address_string, local_port_string);
			
		if (rtsp_parameters.size() > 0) {
			sprintf(url, "rtsp://%s:%d/%s/Live/0", local_address_string.c_str(), _rtsp_server_port, rtsp_parameters.c_str());
		} else {
			sprintf(url, "rtsp://%s:%d/Live/0", local_address_string.c_str(), _rtsp_server_port);
		}
		root["url"] = url;
		root["error"] = "0";

		Json::FastWriter writer;
		std::string content = writer.write(root);
		fprintf(stdout, "RESPONSE: %s\n", content.c_str());
		//oasis::sendJsonResponseMessage(context, content);
		oasis::netMessageSetContent(request_msg, "text/html", content.c_str(), content.length());

	} else if (cmd == "battery") {

		Json::Value root;
		root["cmd"] = cmd;
		{
			std::lock_guard<std::mutex> _l(battery_lock_);
			//root["error"] = battery_online_ ? "0" : "offline";
			root["error"] = 0;
			root["capacity"] = oasis::format("%d", battery_cap_);
			root["status"] = battery_status_;
		}
		Json::FastWriter writer;
		std::string content = writer.write(root);
		fprintf(stdout, "RESPONSE: %s\n", content.c_str());
		//oasis::sendJsonResponseMessage(context, content);
		oasis::netMessageSetContent(request_msg, "text/html", content.c_str(), content.length());

	} else if (cmd == "snapshot") {
		int32_t width, height, camera;

		width = atoi(parameters["width"].c_str());
		height = atoi(parameters["height"].c_str());
		camera = atoi(parameters["camera"].c_str());

		parameters["quality"] = "90";
		parameters["timeout"] = "3000"; //wait timeout max in msec
#if 0
		if(width > 640){
			width = 640;
			parameters["width"] = std::to_string(width);
		}

		if(height > 360){
			height = 360;
			parameters["height"] = std::to_string(height);
		}
#endif		
		fprintf(stdout, "snapshot: %d (%d x %d)\n", camera, width, height);
		struct timeval timestamp;
		std::vector<char> image_data;
		
		if(oasis::takePicture(camera/*camera id*/, parameters, image_data, &timestamp) < 0) {
			//oasis::sendErrorResponseMessage(context, 408, "Request Timeout");
			oasis::netMessageSetStatusCodeAndReasonString(request_msg, 408, "Request Timeout");
		} else {
			//oasis::sendResponseMessage(context, 200, "OK", "image/jpeg", image_data.size(), image_data.data());

			oasis::netMessageSetStatusCodeAndReasonString(request_msg, 200, "OK");
			oasis::netMessageSetMethod(request_msg, "POST");
			oasis::netMessageSetContent(request_msg, "image/jpeg", image_data.data(), image_data.size());

			fprintf(stdout, "netMessagePost: %d bytes\n", image_data.size());
		}

	} else if (cmd == "flash") {
		//oasis::sendErrorResponseMessage(context, 501, "Not Implemented");
		oasis::netMessageSetStatusCodeAndReasonString(request_msg, 501, "Not Implemented");
	} else if (cmd == "distance") {
		Json::Value root;
		root["cmd"] = cmd;
		{
			std::lock_guard<std::mutex> _l(proximitor_lock_);
			root["error"] = 0;
			root["distance"] = oasis::format("%d", proximitor_distance_);
		}
		Json::FastWriter writer;
		std::string content = writer.write(root);
		fprintf(stdout, "RESPONSE: %s\n", content.c_str());
		//oasis::sendJsonResponseMessage(context, content);
		oasis::netMessageSetContent(msg, "text/html", content.c_str(), content.length());
		
	} else {
		//oasis::sendErrorResponseMessage(context, 400, "Bad Request");
		oasis::netMessageSetStatusCodeAndReasonString(request_msg, 400, "Bad Request");
	}

	oasis::netMessagePost(request_msg);
}

void DaUpStreamDelegate::onResponse(const oasis::NetMessageRef &msg)
{
	//fprintf(stdout, " context=%s\n", context);
	//fprintf(stdout, " user=%s\n", user);
	//fprintf(stdout, " response_code=%d\n", response_code);
	//fprintf(stdout, " response_message=%s\n", response_message);
}



//working thread to monitor things.


static int readOneLine(const char *path, std::string &value)
{
	FILE * fp;
	char line[1024];

	value.clear();

	if (access(path, F_OK) != 0) {
		fprintf(stderr, "\"%s\" not exist\n", path);
		return -1;
	}

	if (access(path, R_OK) != 0) {
		fprintf(stderr, "\"%s\" can not be read\n", path);
		return -1;
	}

	fp = fopen(path, "r");
	if (fp == NULL) {
		fprintf(stderr, "\"%s\" open failed: error %d(%s)\n", path, errno, strerror(errno));
		return -1;
	}

	char *p = fgets(line, sizeof(line), fp);
	fclose(fp);

	if (!p) {
		fprintf(stderr, "\"%s\" read failed: error %d(%s)\n", path, errno, strerror(errno));
		return -1;
	}

	value = line;
	return 0;
}

void worker(void)
{
	std::string value;
	int32_t err;

	fprintf(stdout, "worker starts\n");

	while (continue_working) {
#if 0
		err = readOneLine(BATTERY_ONLINE_FILE, value);
		{
			std::lock_guard<std::mutex> _l(battery_lock_);
			if (err < 0 || atoi(value.c_str()) == 0) {
				battery_online_ = false;
			} else {
				battery_online_ = true;
			}
		}

		if (readOneLine(BATTERY_CAPACITY, value) == 0) {
			//value/20 ==> level (0 ~ 5)
			int32_t cap = atoi(value.c_str());
			std::lock_guard<std::mutex> _l(battery_lock_);
			battery_cap_ = cap;
		}

		if (readOneLine(BATTERY_STATUS, value) == 0 && !value.empty()) {
			//Charging, Discharging, Not charging, Full
			if (value[value.length() - 1] == '\n') {
				value.erase(value.length() - 1);
			}
			std::lock_guard<std::mutex> _l(battery_lock_);
			battery_status_ = value;
		}
#endif
		//read proximity sensor events
		{
			//PROXIMITY_EVENT
		}


		//fprintf(stdout, "battery online %d, capacity %d, status %s\n", battery_online_, battery_cap_, battery_status_.c_str());


		usleep(500000);
	}
	fprintf(stdout, "worker ended.\n");
}

#endif //ENABLE_HTTP

#if ENABLE_RTSP
DaRtspEventDelegate::DaRtspEventDelegate(){}
DaRtspEventDelegate::~DaRtspEventDelegate(){}

void DaRtspEventDelegate::onDescription(const char *uri, const char *remote, const char *sdp){}
void DaRtspEventDelegate::onSetup(const char *uri, const char *remote, const char *transport_line)
{
	//fprintf(stdout, "%s() : url:%s remote:%s transport_line:%s\n", __func__, uri, remote, transport_line);
}
void DaRtspEventDelegate::onPlay(const char *uri, const char *remote, const char *rtp_info)
{
	rtsp_is_play++;
	fprintf(stdout, "%s() : %d url:%s remote:%s rtp_info:%s\n", __func__, rtsp_is_play, uri, remote, rtp_info);
}
void DaRtspEventDelegate::onTeardown(const char *uri, const char *remote){}
void DaRtspEventDelegate::onPause(const char *uri, const char *remote){}
void DaRtspEventDelegate::onError(int error_code, const char *error_msg, const char *error_desc)
{
	fprintf(stdout, "%s() : error_code:%d error_msg:%s error_desc:%s\n", __func__, error_code, error_msg, error_desc);
}
void DaRtspEventDelegate::onClosed(const char *uri, const char *remote)
{
	if(rtsp_is_play)
		rtsp_is_play --;

	fprintf(stdout, "%s() : %d url:%s remote:%s \n", __func__, rtsp_is_play, uri, remote);
}

#endif


int streaming_start(u16 http_server_port, u16 rtsp_server_port)
{
	
	int32_t err;
	oasis::key_value_map_t parameters;
#if ENABLE_HTTP	
	static std::shared_ptr<DaUpStreamDelegate> delegate = std::make_shared<DaUpStreamDelegate>();
#endif
#if ENABLE_RTSP	
	static std::shared_ptr<DaRtspEventDelegate> rtsp_delegate = std::make_shared<DaRtspEventDelegate>();
#endif
	if(http_server_port)
		_http_server_port = http_server_port;

	if(rtsp_server_port)
		_rtsp_server_port = rtsp_server_port;
		
		
	continue_working = true;
#if ENABLE_STUN
	////////////////////////////////////////////////////////////////////////////////////////////
	// find network config
	char ifnames[5][IFNAMSIZ];
	char local_netaddr[16] = {0,}, netmask[16] = {0,}, gateway[16] = {0,};
	int32_t netif_count = oasis::get_netif_list(ifnames, 5);
	
	DLOG(DLOG_INFO, "netif#%d\n", netif_count);
	
	for(int32_t i=0; i<netif_count; i++) {
		oasis::get_current_netif_config(ifnames[i], local_netaddr, netmask, gateway);
		DLOG(DLOG_INFO, "NETIF<%d>: %s %s/%s, GW %s\n", i, ifnames[i], local_netaddr, netmask, gateway);
	}

 #if 0
	uint32_t addr4;
	uint16_t port;
	err = resolve_net_address("stun.l.google.com", kNetServiceStun, kNetTransportDatagram, &addr4, &port);
 #endif

	if(netif_count > 0 && local_netaddr[0] != 0) {
		char pub_netaddr[16], pub_tpaddr[8], local_port[16];
 #if ENABLE_HTTP
		sprintf(local_port, "%d", _http_server_port);
		err = oasis::stun_get_mapped_address(STUN_SERVER, STUN_PORT, local_netaddr, local_port, pub_netaddr, pub_tpaddr, 3000);
		if(err == 0) {
			DLOG(DLOG_INFO, "STUN: HTTP %s/%s -> %s/%s\n", local_netaddr, local_port, pub_netaddr, pub_tpaddr);
		} else {
			DLOG(DLOG_INFO, "STUN: HTTP %s/%s -> ???\n", local_netaddr, local_port);
		}
 #endif		
 #if ENABLE_RTSP
		sprintf(local_port, "%d", _rtsp_server_port);
		//stun.sipgate.net, stun.stunprotocol.org
		err = oasis::stuns_get_mapped_address(STUN_SERVER, STUN_PORT, local_netaddr, local_port, pub_netaddr, pub_tpaddr, 3000);
		if(err == 0) {
			DLOG(DLOG_INFO, "STUNS: RTSP %s/%s -> %s/%s\n", local_netaddr, local_port, pub_netaddr, pub_tpaddr);
		} else {
			DLOG(DLOG_INFO, "STUNS: RTSP %s/%s -> ???\n", local_netaddr, local_port);
		}
 #endif		
	}
#endif

#if ENABLE_HTTP
	parameters.clear();
	parameters["port"] = std::to_string(_http_server_port);
	parameters["root-dir"] = HTTP_ROOT;

	http_id = oasis::createHttpServer(parameters, delegate);
	if (http_id < 0) {
		DLOG(DLOG_ERROR, "createHttpServer() failed\n");
		goto failed;
	}

	err = oasis::startHttpServer(http_id);
	if (err < 0) {
		ASSERT(err == 0);
		DLOG(DLOG_INFO, "HTTP running at %d...\n", _http_server_port);
		goto failed;
	}
#endif	

#if ENABLE_RTSP
	parameters.clear();
	parameters["port"] = std::to_string(_rtsp_server_port);
	parameters["root-dir"] = RTSP_ROOT;
	parameters["pool-size"] = std::to_string(10); //max con-current network connections
	parameters["scheduler-size"] = std::to_string(5); //max working threads
	parameters["upstreamer-size"] = std::to_string(5); //max working threads
	parameters["timer-size"] = std::to_string(3);
	parameters["disable-rtcp"] = "1";
	parameters["enable-stun"] = std::to_string(ENABLE_STUN);
	parameters["stun-server-addr"] = STUN_SERVER;
	parameters["stun-server-port"] = STUN_PORT;

	parameters["file-session-max"] = std::to_string(1);	 //0: unlimited
	parameters["live-session-max"] = std::to_string(2);	 //0: unlimited	
	parameters["linger-time-max"] = std::to_string(1000);	
	parameters["rtp-data-size-max"] = std::to_string(1024);	
	parameters["rtp-packet-pool-count-max"] = std::to_string(6);	
	parameters["rtp-packet-pool-size"] = std::to_string(256);	
	
	rtsp_server = oasis::createRtspServer(parameters, rtsp_delegate);
	if (rtsp_server < 0) {
		DLOG(DLOG_ERROR, "createRtspServer() failed\n");
		goto failed;
	}
	
	err = oasis::startRtspServer(rtsp_server);
	if (err < 0) {
		ASSERT(err == 0);
		DLOG(DLOG_ERROR, "RTSP server failed\n");
		goto failed;
	}
	DLOG(DLOG_INFO, "RTSP running at %d...\n", _rtsp_server_port);
#endif	
	//t = std::thread(worker);	


	return 0;
	
failed:

	streaming_end();

	return -1;
}

int streaming_end(void)
{
	continue_working = false;
	//t.join();
#if ENABLE_HTTP
	if (http_id >= 0) {
		oasis::destroyHttpServer(http_id);
	}
#endif

#if ENABLE_RTSP
	if (rtsp_server) {
		oasis::destroyRtspServer(rtsp_server);
	}
#endif

	return 0;
}

int streaming_is_rtsp_play(void)
{
	return rtsp_is_play;
}

#endif // ENABLE_NETWORKING


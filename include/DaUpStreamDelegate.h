#ifndef DaUpStreamDelegate_H
#define DaUpStreamDelegate_H

#include "OasisAPI.h"
#include "OasisLog.h"

#include "OasisRtsp.h"

#include "datypes.h"
#include "daappconfigs.h"

#ifndef HTTPD_EMBEDDED
#define ENABLE_HTTP		0
#else
#define ENABLE_HTTP		0
#endif

#define ENABLE_RTSP 	1
#define ENABLE_STUN		0

#if ENABLE_HTTP	
class DaUpStreamDelegate : public oasis::HttpUpStreamDelegate
{
public:
	//proximity 
	#define PROXIMITY_EVENT		"/dev/input/event3"
	std::mutex proximitor_lock_;
	int32_t proximitor_distance_;

	
	DaUpStreamDelegate();
	virtual ~DaUpStreamDelegate();
public:
	virtual void onRequest(const oasis::NetMessageRef &msg);
	virtual void onResponse(const oasis::NetMessageRef &msg);
};
#endif

#if ENABLE_RTSP
class DaRtspEventDelegate : public oasis::RtspEventDelegate
{
public:
	DaRtspEventDelegate();
	virtual ~DaRtspEventDelegate();

public:
	virtual void onDescription(const char *uri, const char *remote, const char *sdp);
	virtual void onSetup(const char *uri, const char *remote, const char *transport_line);
	virtual void onPlay(const char *uri, const char *remote, const char *rtp_info);
	virtual void onTeardown(const char *uri, const char *remote);
	virtual void onPause(const char *uri, const char *remote);
	virtual void onError(int error_code, const char *error_msg, const char *error_desc);
	virtual void onClosed(const char *uri, const char *remote);
};
#endif

#ifdef __cplusplus
extern "C"
{
#endif

int streaming_start(u16 http_server_port, u16 rtsp_server_port);
int streaming_end(void);
int streaming_is_rtsp_play(void);

#ifdef __cplusplus
}
#endif

#endif // DaUpStreamDelegate_H

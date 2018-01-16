
#pragma once

#include <deque>
#include <vector>
#include  "sv_gen_sdk.h"
using namespace std;


class SVCamAcquisition
{
public:

	SVCamAcquisition(SV_DEVICE_HANDLE _hDevice, SV_REMOTE_DEVICE_HANDLE _hRemoteDev);
	~SVCamAcquisition();

	/*
	open the Streamming channel with the given buffer count.
	*/
	void AcquisitionStart(size_t bufcount);

	/*
	close the streamming channel.
	the announced buffer will be removed from the acquisition engine
	*/
	void AcquisitionStop();


	int AcquisitionThread();
	HANDLE m_acquisitionThread;
	HANDLE m_acquisitionstopThread;
	bool acqTerminated;


	HANDLE m_newImage;
	int64_t payloadSize;
	size_t dsBufcount;
	deque<SV_BUFFER_INFO *> imageBufferInfo;

public:
	SV_DEVICE_HANDLE hDevice;
	SV_REMOTE_DEVICE_HANDLE hRemoteDev;
	SV_STREAM_HANDLE hDS;
	size_t ImagerWidth;
	size_t ImagerHeight;
private:

	CRITICAL_SECTION  csStream;

};
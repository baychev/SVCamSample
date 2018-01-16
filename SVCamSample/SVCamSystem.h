#pragma once

#include <vector>

#include  "sv_gen_sdk.h"

#include "SVCamFeature.h"
#include "SVCamAcquisation.h"
using namespace std;

class Camera
{
public:
	Camera(SV_TL_TYPE tl_typ, SV_DEVICE_INFO _devInfo)
	{

		devInfo = _devInfo;
		sv_cam_type = tl_typ;
		hDevice = NULL;
		hRemoteDevice = NULL;
		sv_cam_acq = NULL;
		sv_cam_feature = NULL;
		isInvalidateCB = false;

		//Open the device with device id (devInfo.uid) connected to the interface (devInfo.hParentIF)
		//Each camera contains acquisition instance for streaming and feature instance for controlling
		int Ret = SVInterfaceDeviceOpen(devInfo.hParentIF, devInfo.uid, SV_DEVICE_ACCESS_CONTROL, &hDevice, &hRemoteDevice);
		if (Ret == SV_ERROR_SUCCESS)
		{
			//New instances will be intialised only if the device is sucessufly oppened.
			sv_cam_acq = new SVCamAcquisition(hDevice, hRemoteDevice);
			sv_cam_feature = new SVCamFeature(hRemoteDevice);
		}
	}

	~Camera()
	{
		// Delete all related instances.
		if (sv_cam_acq)
		{
			delete sv_cam_acq;
			sv_cam_acq = NULL;
		}
		if (sv_cam_feature)
		{
			delete sv_cam_feature;
			sv_cam_feature = NULL;
		}
		//Close the device module and free all the allocated resources.
		SVDeviceClose(hDevice);
		hDevice = NULL;
		hRemoteDevice = NULL;



	}


	SV_TL_TYPE sv_cam_type;
	SVCamAcquisition *sv_cam_acq;
	SVCamFeature	*sv_cam_feature;
	SV_REMOTE_DEVICE_HANDLE hRemoteDevice;
	SV_DEVICE_INFO devInfo;
	SV_DEVICE_HANDLE hDevice;
	bool isInvalidateCB;

};

class SVCamSystem
{
public:
	SVCamSystem(SV_TL_TYPE tl_typ);
	~SVCamSystem();
	bool SVCamSystemInit(uint32_t sysindex);

	void EnumDevices(unsigned int timeout);
	void openDevice(SV_DEVICE_INFO devInfo);

	SV_RETURN closeDevice(SV_DEVICE_HANDLE hDev);
	vector<SV_DEVICE_INFO *>  devInfoList;
	vector<Camera * > sv_cam_list;

private:

	SV_TL_TYPE sv_cam_type;
	SV_SYSTEM_HANDLE sv_cam_sys_hdl;

};
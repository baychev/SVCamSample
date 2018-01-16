#include "SVCamSystem.h"


SVCamSystem::SVCamSystem(SV_TL_TYPE tl_typ)
{
	sv_cam_type = tl_typ;
	sv_cam_sys_hdl = NULL;
}

SVCamSystem::~SVCamSystem()
{
	// Delete all cameras in the list


	//printf("%s delete all cameras. \n", __FUNCTION__);
	DSDeleteContainer(sv_cam_list);


	//printf("%s Close all interfaces. \n", __FUNCTION__);
	// Close all interfaces and free all the resources allocated by this module
	for (vector<SV_DEVICE_INFO *>::iterator it = devInfoList.begin(); it != devInfoList.end(); ++it)
		if (*it)
		{

			SVInterfaceClose((*it)->hParentIF);
		}
	// Delete and clear all elements in the device info list
	DSDeleteContainer(devInfoList);

	//printf("%s Close system module. \n", __FUNCTION__);
	//Close system module and free all the resources
	if (sv_cam_sys_hdl)
		SVSystemClose(sv_cam_sys_hdl);
}

bool  SVCamSystem::SVCamSystemInit(uint32_t Sysindex)
{
	//Opens the System module and puts the instance in the phSystem handle. This allocates all system wide
	//resources. A System module can only be opened once.


	if (sv_cam_sys_hdl == NULL)
	{
		int ret = SVLibSystemOpen(Sysindex, &sv_cam_sys_hdl);
		if (SV_ERROR_SUCCESS != ret)
			return false;
	}
	return true;
}

void SVCamSystem::openDevice(SV_DEVICE_INFO devInfo)
{

	//Once the device is successfully opened an acquisition and feature instance are initialized and the new camera object is added to the camera list
	Camera* newCam = new Camera(sv_cam_type, devInfo);
	if (newCam->sv_cam_acq)
	{
		sv_cam_list.push_back(newCam);
	}
	else
		delete newCam;
}

SV_RETURN  SVCamSystem::closeDevice(SV_DEVICE_HANDLE hDevice)
{
	//Close the device module and frees all the resources allocated by this module.
	return SVDeviceClose(hDevice);
}

void SVCamSystem::EnumDevices(unsigned int timeout)
{
	bool bChanged = false;

	//Updates the internal list of available interfaces.
	SV_RETURN ret = SVSystemUpdateInterfaceList(sv_cam_sys_hdl, &bChanged, timeout);
	if (SV_ERROR_SUCCESS != ret)
		return;

	uint32_t numInterface = 0;

	//Queries the number of available interfaces on this System module.
	ret = SVSystemGetNumInterfaces(sv_cam_sys_hdl, &numInterface);
	if (SV_ERROR_SUCCESS != ret)
		return;


	for (uint32_t i = 0; i < numInterface; i++)
	{
		char interfaceId[SV_STRING_SIZE] = { 0 };
		size_t interfaceIdSize = sizeof(interfaceId);

		//Queries the ID of the interface at iIndex in the internal interface list .
		ret = SVSystemGetInterfaceId(sv_cam_sys_hdl, i, interfaceId, &interfaceIdSize);
		if (SV_ERROR_SUCCESS != ret)
		{
			continue;
		}
		SV_INTERFACE_INFO interfaceInfo = { 0 };
		ret = SVSystemInterfaceGetInfo(sv_cam_sys_hdl, interfaceId, &interfaceInfo);
		if (SV_ERROR_SUCCESS != ret)
		{
			continue;
		}

		SV_INTERFACE_HANDLE hInterface = NULL;
		// Queries the information about the interface on this System module without opening the interface.
		ret = SVSystemInterfaceOpen(sv_cam_sys_hdl, interfaceId, &hInterface);
		if (SV_ERROR_SUCCESS != ret)
		{
			continue;
		}

		//Updates the internal list of available devices on this interface.
		ret = SVInterfaceUpdateDeviceList(hInterface, &bChanged, 1000);
		if (SV_ERROR_SUCCESS != ret)
		{
			continue;
		}

		uint32_t numDevices = 0;
		//Queries the number of available devices on this interface
		ret = SVInterfaceGetNumDevices(hInterface, &numDevices);
		if (SV_ERROR_SUCCESS != ret || 0 == numDevices)
		{
			SVInterfaceClose(hInterface);
			continue;
		}

		// Get device info for all available devices and add it to the device info list. 
		for (uint32_t j = 0; j < numDevices; j++)
		{
			char deviceId[SV_STRING_SIZE] = { 0 };
			size_t deviceIdSize = sizeof(deviceId);
			ret = SVInterfaceGetDeviceId(hInterface, j, deviceId, &deviceIdSize);
			if (SV_ERROR_SUCCESS != ret)
			{
				continue;
			}

			SV_DEVICE_INFO * devInfo = new   SV_DEVICE_INFO();
			ret = SVInterfaceDeviceGetInfo(hInterface, deviceId, devInfo);
			if (SV_ERROR_SUCCESS != ret)
			{
				delete devInfo;
				continue;
			}

			devInfoList.push_back(devInfo);
		}
	}

	return;
}

#pragma once

#include <vector>
#include  "sv_gen_sdk.h"
#include "ds_commonwin.h"
using namespace std;



typedef struct _SVCamFeaturInf
{
	int64_t intValue;
	double doubleValue;
	bool booValue;
	char strValue[SV_STRING_SIZE];
	char subFeatureName[SV_STRING_SIZE];

	SV_FEATURE_INFO  SVFeaturInf;
	SV_FEATURE_HANDLE hFeature;
	SV_REMOTE_DEVICE_HANDLE hRemoteDevice;
}
SVCamFeaturInf, *PSVCamFeaturInf;


class SVCamFeature
{
public:
	SVCamFeature(SV_REMOTE_DEVICE_HANDLE _hRemoteDevice);
	~SVCamFeature();

	/*
	Update Feature Info.
	All feature values will be converted to a string (strValue) to be displayed later in the tree.
	*/
	void getFeatureValue(SV_FEATURE_HANDLE hFeature, SVCamFeaturInf* camFeatureInfo);

	/*
	Clean up the previous feature info list if it exists.
	For each feature create a new Feature structure SVCamFeaturInf and fill it with all information needed to control the camera.
	*/
	void getDeviceFeatureList(SV_FEATURE_VISIBILITY visibility);

	/*
	Register callback to be called when hFeature is invalidated.
	*/
	SV_RETURN RegisterInvalidateCB(char*featureName, SV_CB_OBJECT objCb, SV_CB_FEATURE_INVALIDATED_PFN pfnFeatureInvalidateCb);

	/*
	Unregister callback function registered on RegisterInvalidateCB.
	The streaming has to be stopped before calling this function.
*/
	SV_RETURN UnRegisterInvalidateCB(SV_FEATURE_HANDLE hFeature);

	vector <SVCamFeaturInf*> featureInfolist;
	SV_REMOTE_DEVICE_HANDLE hRemoteDevice;
};
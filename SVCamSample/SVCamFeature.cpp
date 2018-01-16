
#include "SVCamFeature.h"

SVCamFeature::SVCamFeature(SV_REMOTE_DEVICE_HANDLE _hRemoteDevice)
{
	hRemoteDevice = _hRemoteDevice;
}

SVCamFeature::~SVCamFeature()
{
	for (vector <SVCamFeaturInf*>::iterator it = featureInfolist.begin(); it != featureInfolist.end(); ++it)
	{
		UnRegisterInvalidateCB((*it)->hFeature);
	}
	DSDeleteContainer(featureInfolist);

}

void SVCamFeature::getFeatureValue(SV_FEATURE_HANDLE hFeature, SVCamFeaturInf *SvCamfeatureInfo)
{

	SVFeatureGetInfo(hRemoteDevice, hFeature, &SvCamfeatureInfo->SVFeaturInf);


	memset(SvCamfeatureInfo->strValue, 0, sizeof(char)*SV_STRING_SIZE);
	memset(SvCamfeatureInfo->subFeatureName, 0, sizeof(char)*SV_STRING_SIZE);

	if (SV_intfIInteger == SvCamfeatureInfo->SVFeaturInf.type)
	{
		int64_t value = 0;
		SVFeatureGetValueInt64(hRemoteDevice, hFeature, &value);
		SvCamfeatureInfo->intValue = value;
		string st = to_string(value);


		st._Copy_s(SvCamfeatureInfo->strValue, sizeof(char)*SV_STRING_SIZE, SV_STRING_SIZE, 0);

	}
	else if (SV_intfIFloat == SvCamfeatureInfo->SVFeaturInf.type)
	{
		double value = 0.0f;
		SVFeatureGetValueFloat(hRemoteDevice, hFeature, &value);
		SVFeatureGetValueString(hRemoteDevice, hFeature, SvCamfeatureInfo->strValue, 512);

		SvCamfeatureInfo->doubleValue = value;
		to_string(value)._Copy_s(SvCamfeatureInfo->strValue, sizeof(char)*SV_STRING_SIZE, SV_STRING_SIZE, 0);

	}
	else if (SV_intfIBoolean == SvCamfeatureInfo->SVFeaturInf.type)
	{
		bool value = false;
		SVFeatureGetValueBool(hRemoteDevice, hFeature, &value);
		SvCamfeatureInfo->booValue = value;
		if (value == 0)
		{
			string s("False");
			s._Copy_s(SvCamfeatureInfo->strValue, sizeof(char)*SV_STRING_SIZE, SV_STRING_SIZE, 0);
		}
		else
		{
			string s("True");
			s._Copy_s(SvCamfeatureInfo->strValue, sizeof(char)*SV_STRING_SIZE, SV_STRING_SIZE, 0);
		}
	}
	else if (SV_intfICommand == SvCamfeatureInfo->SVFeaturInf.type)
	{
		string s(" = > Execute Command");
		s._Copy_s(SvCamfeatureInfo->strValue, sizeof(char)*SV_STRING_SIZE, SV_STRING_SIZE, 0);
	}

	else if (SV_intfIString == SvCamfeatureInfo->SVFeaturInf.type)
	{

		SVFeatureGetValueString(hRemoteDevice, hFeature, SvCamfeatureInfo->strValue, SV_STRING_SIZE);
		SVFeatureGetValueString(hRemoteDevice, hFeature, SvCamfeatureInfo->strValue, SV_STRING_SIZE);

	}
	else if (SV_intfIEnumeration == SvCamfeatureInfo->SVFeaturInf.type)
	{
		int64_t pInt64Value = 0;
		SV_RETURN  ret = SVFeatureEnumSubFeatures(hRemoteDevice, hFeature, SvCamfeatureInfo->SVFeaturInf.enumSelectedIndex, SvCamfeatureInfo->subFeatureName, sizeof(SvCamfeatureInfo->subFeatureName)); //, &pInt64Value);

		string s(SvCamfeatureInfo->subFeatureName);
		if (sizeof(s) > 0)
			s._Copy_s(SvCamfeatureInfo->strValue, sizeof(char)*SV_STRING_SIZE, SV_STRING_SIZE, 0);
		SvCamfeatureInfo->intValue = pInt64Value;
	}

}

void SVCamFeature::getDeviceFeatureList(SV_FEATURE_VISIBILITY visibility)
{
	DSDeleteContainer(featureInfolist);
	uint32_t iIndex = 0;
	SV_RETURN ret = SV_ERROR_SUCCESS;

	while (true)
	{
		SV_FEATURE_HANDLE hFeature = NULL;
		ret = SVFeatureGetByIndex(hRemoteDevice, iIndex++, &hFeature);
		if (SV_ERROR_SUCCESS != ret)
			break;

		//Create a new Feature structure and
		SVCamFeaturInf *camFeatureInfo = new SVCamFeaturInf();

		ret = SVFeatureGetInfo(hRemoteDevice, hFeature, &camFeatureInfo->SVFeaturInf);
		if (SV_ERROR_SUCCESS != ret)
		{
			//printf(":%s SVFeatureGetInfo Failed!:%d\n", __FUNCTION__, ret);
			continue;
		}

		//	retrive only a specific features 
		if (camFeatureInfo->SVFeaturInf.visibility > visibility || !camFeatureInfo->SVFeaturInf.isImplemented || SV_intfIPort == camFeatureInfo->SVFeaturInf.type)
		{
			delete camFeatureInfo;
			continue;
		}
		//fill the structure with all information needed to control the camera.

	   // get the current value and feature info 
		getFeatureValue(hFeature, camFeatureInfo);

		//add the feature handle and remote device handle 
		camFeatureInfo->hFeature = hFeature;
		camFeatureInfo->hRemoteDevice = hRemoteDevice;
		featureInfolist.push_back(camFeatureInfo);
	}

}


SV_RETURN SVCamFeature::RegisterInvalidateCB(char*featureName, SV_CB_OBJECT objCb, SV_CB_FEATURE_INVALIDATED_PFN pfnFeatureInvalidateCb)

{
	SV_FEATURE_HANDLE hFeature = NULL;
	SV_RETURN ret = SVFeatureGetByName(hRemoteDevice, featureName, &hFeature);
	if (SV_ERROR_SUCCESS != ret)
		return  ret;

	ret = SVFeatureRegisterInvalidateCB(hRemoteDevice, hFeature, objCb, pfnFeatureInvalidateCb);
	return ret;

}

SV_RETURN SVCamFeature::UnRegisterInvalidateCB(SV_FEATURE_HANDLE hFeature)

{
	return SVFeatureUnRegisterInvalidateCB(hRemoteDevice, hFeature);
}


/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEnginePrivatePCH.h"


const FString kResultStringSuccess(TEXT("Success"));
const FString kResultStringFailure(TEXT("Generic Failure"));
const FString kResultStringAlreadyInitialized(TEXT("Already Initialized"));
const FString kResultStringNotInitialized(TEXT("Not Initialized"));
const FString kResultStringCannotLoadFile(TEXT("Unable to Load File"));
const FString kResultStringParmSetFailed(TEXT("Failed Setting Parameter"));
const FString kResultStringInvalidArgument(TEXT("Invalid Argument"));
const FString kResultStringCannotLoadGeo(TEXT("Uneable to Load Geometry"));
const FString kResultStringCannotGeneratePreset(TEXT("Uneable to Generate Preset"));
const FString kResultStringCannotLoadPreset(TEXT("Uneable to Load Preset"));


const FString
FHoudiniEngineUtils::GetErrorDescription(HAPI_Result Result)
{
	if(HAPI_RESULT_SUCCESS == Result)
	{
		return kResultStringSuccess;
	}
	else
	{
		switch (Result)
		{
			case HAPI_RESULT_FAILURE:
			{
				return kResultStringAlreadyInitialized;
			}

			case HAPI_RESULT_ALREADY_INITIALIZED:
			{
				return kResultStringAlreadyInitialized;
			}

			case HAPI_RESULT_NOT_INITIALIZED:
			{
				return kResultStringNotInitialized;
			}

			case HAPI_RESULT_CANT_LOADFILE:
			{
				return kResultStringCannotLoadFile;
			}

			case HAPI_RESULT_PARM_SET_FAILED:
			{
				return kResultStringParmSetFailed;
			}

			case HAPI_RESULT_INVALID_ARGUMENT:
			{
				return kResultStringInvalidArgument;
			}

			case HAPI_RESULT_CANT_LOAD_GEO:
			{
				return kResultStringCannotLoadGeo;
			}

			case HAPI_RESULT_CANT_GENERATE_PRESET:
			{
				return kResultStringCannotGeneratePreset;
			}

			case HAPI_RESULT_CANT_LOAD_PRESET:
			{
				return kResultStringCannotLoadPreset;
			}

			default:
			{
				ASSUME(0);
			}
		};
	}
}


bool
FHoudiniEngineUtils::IsInitialized()
{
	return(HAPI_RESULT_SUCCESS == HAPI_IsInitialized());
}


bool
FHoudiniEngineUtils::GetAssetName(int AssetName, std::string& AssetNameString)
{
	if(-1 == AssetName)
	{
		return false;
	}

	// For now we will load first asset only.
	int AssetNameLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStringBufLength(AssetName, &AssetNameLength), false);

	// Retrieve name of first asset.
	if(AssetNameLength)
	{
		AssetNameString.reserve(AssetNameLength + 1);
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetString(AssetName, &AssetNameString[0], AssetNameLength), false);
		AssetNameString.resize(AssetNameLength + 1);
	}

	return true;
}


bool
FHoudiniEngineUtils::GetAssetName(int AssetName, FString& AssetNameString)
{
	std::string AssetNamePlain;

	if(FHoudiniEngineUtils::GetAssetName(AssetName, AssetNamePlain))
	{
		AssetNameString = ANSI_TO_TCHAR(AssetNamePlain.c_str());
		return true;
	}

	return false;
}

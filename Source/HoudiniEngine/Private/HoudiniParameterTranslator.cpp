/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniParameterTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEnginePrivatePCH.h"

#include "HoudiniAsset.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterButton.h"
#include "HoudiniParameterButtonStrip.h"
#include "HoudiniParameterChoice.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterFolder.h"
#include "HoudiniParameterFolderList.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterLabel.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterSeparator.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterOperatorPath.h"

#include "HoudiniInput.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniParameter.h"
#include "HoudiniAssetComponent.h"


// Default values for certain UI min and max parameter values
#define HAPI_UNREAL_PARAM_INT_UI_MIN				0
#define HAPI_UNREAL_PARAM_INT_UI_MAX				10
#define HAPI_UNREAL_PARAM_FLOAT_UI_MIN				0.0f
#define HAPI_UNREAL_PARAM_FLOAT_UI_MAX				10.0f

// Some default parameter name
#define HAPI_UNREAL_PARAM_TRANSLATE					"t"
#define HAPI_UNREAL_PARAM_ROTATE					"r"
#define HAPI_UNREAL_PARAM_SCALE						"s"
#define HAPI_UNREAL_PARAM_PIVOT						"p"
#define HAPI_UNREAL_PARAM_UNIFORMSCALE				"scale"

// 
bool 
FHoudiniParameterTranslator::UpdateParameters(UHoudiniAssetComponent* HAC)
{
	if (!IsValid(HAC))
		return false;

	// When recooking/rebuilding the HDA, force a full update of all params
	const bool bForceFullUpdate = HAC->HasRebuildBeenRequested() || HAC->HasRecookBeenRequested() || HAC->IsParameterDefinitionUpdateNeeded();

	TArray<UHoudiniParameter*> NewParameters;
	if (FHoudiniParameterTranslator::BuildAllParameters(HAC->GetAssetId(), HAC, HAC->Parameters, NewParameters, true, bForceFullUpdate, HAC->GetHoudiniAsset(), HAC->GetHapiAssetName()))
	{
		/*
		// DO NOT MANUALLY DESTROY THE OLD/DANGLING PARAMETERS!
		// This messes up unreal's Garbage collection and would cause crashes on duplication

		// Destroy old/dangling parameters
		for (auto& OldParm : HAC->Parameters)
		{
			if (!IsValid(OldParm))
				continue;

			OldParm->ConditionalBeginDestroy();
			OldParm = nullptr;
		}
		*/

		// Replace with the new parameters
		HAC->Parameters = NewParameters;

		// Update the details panel after the parameter changes/updates
		FHoudiniEngineUtils::UpdateEditorProperties(HAC, true);
	}


	return true;
}

bool
FHoudiniParameterTranslator::OnPreCookParameters(UHoudiniAssetComponent* HAC)
{
	// Call OnPreCook for all parameters.
	// Parameters can use this to ensure that any cached / non-cooking state is properly
	// synced before the cook starts (Looking at you, ramp parameters!)
	for (UHoudiniParameter* Param : HAC->Parameters)
	{
		if (!IsValid(Param))
			continue;

		Param->OnPreCook();
	}

	return true;
}

// 
bool
FHoudiniParameterTranslator::UpdateLoadedParameters(UHoudiniAssetComponent* HAC)
{
	if (!IsValid(HAC))
		return false;

	// Update all the parameters using the loaded parameter object
	// We set "UpdateValues" to false because we do not want to "read" the parameter value
	// from Houdini but keep the loaded value

	// Share AssetInfo if needed
	bool bNeedToFetchAssetInfo = true;
	HAPI_AssetInfo AssetInfo;

	// This is the first cook on loading after a save or duplication
	for (int32 Idx = 0; Idx < HAC->Parameters.Num(); ++Idx)
	{
		UHoudiniParameter* Param = HAC->Parameters[Idx];

		if (!IsValid(Param))
			continue;

		switch(Param->GetParameterType())
		{
			case EHoudiniParameterType::ColorRamp:
			case EHoudiniParameterType::FloatRamp:
			case EHoudiniParameterType::MultiParm:
			{
				// We need to sync the Ramp parameters first, so that their child parameters can be kept
				if (bNeedToFetchAssetInfo)
				{
					FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), HAC->AssetId, &AssetInfo);
					bNeedToFetchAssetInfo = false;
				}

				// TODO: Simplify this, should be handled in BuildAllParameters
				SyncMultiParmValuesAtLoad(Param, HAC->Parameters, HAC->AssetId, AssetInfo);
			}
			break;

			case EHoudiniParameterType::Button:
			case EHoudiniParameterType::ButtonStrip:
			{
				// Do not trigger buttons upon loading
				Param->MarkChanged(false);
			}
			break;

			default:
				break;
		}
	}

	// When recooking/rebuilding the HDA, force a full update of all params
	const bool bForceFullUpdate = HAC->HasRebuildBeenRequested() || HAC->HasRecookBeenRequested() || HAC->IsParameterDefinitionUpdateNeeded();

	// This call to BuildAllParameters will keep all the loaded parameters (in the HAC's Parameters array)
	// that are still present in the HDA, and keep their loaded value.
	TArray<UHoudiniParameter*> NewParameters;
	// We don't need to fetch defaults from the asset definition for a loaded HAC
	const UHoudiniAsset* const HoudiniAsset = nullptr;
	const FString HoudiniAssetName = FString();
	if (FHoudiniParameterTranslator::BuildAllParameters(HAC->GetAssetId(), HAC, HAC->Parameters, NewParameters, false, bForceFullUpdate, HoudiniAsset, HoudiniAssetName))
	{
		/*
		// DO NOT DESTROY OLD PARAMS MANUALLY HERE
		// This causes crashes upon duplication due to uncollected zombie objects...
		// GC is supposed to handle this by itself
		// Destroy old/dangling parameters
		for (auto& OldParm : HAC->Parameters)
		{
			if (!IsValid(OldParm))
				continue;

			OldParm->ConditionalBeginDestroy();
			OldParm = nullptr;
		}
		*/

		// Simply replace with the new parameters
		HAC->Parameters = NewParameters;

		// Update the details panel after the parameter changes/updates
		FHoudiniEngineUtils::UpdateEditorProperties(HAC, true);
	}

	return true;
}

bool
FHoudiniParameterTranslator::BuildAllParameters(
	const HAPI_NodeId& AssetId, 
	class UObject* Outer,
	TArray<UHoudiniParameter*>& CurrentParameters,
	TArray<UHoudiniParameter*>& NewParameters,
	const bool& bUpdateValues,
	const bool& InForceFullUpdate,
	const UHoudiniAsset* InHoudiniAsset,
	const FString& InHoudiniAssetName)
{
	const bool bIsAssetValid = IsValid(InHoudiniAsset);
	
	// Ensure the asset has a valid node ID
	if (AssetId < 0 && !bIsAssetValid)
	{	
		return false;
	}

	int32 ParmCount = 0;

	// Default value counts and arrays for if we need to fetch those from Houdini.
	int DefaultIntValueCount = 0;
	int DefaultFloatValueCount = 0;
	int DefaultStringValueCount = 0;
	int DefaultChoiceValueCount = 0;
	TArray<int> DefaultIntValues;
	TArray<float> DefaultFloatValues;
	TArray<HAPI_StringHandle> DefaultStringValues;
	TArray<HAPI_ParmChoiceInfo> DefaultChoiceValues;
	
	HAPI_NodeId NodeId = -1;
	HAPI_AssetLibraryId AssetLibraryId = -1;
	FString HoudiniAssetName;
	
	if (AssetId >= 0)
	{
		// Get the asset's info
		HAPI_AssetInfo AssetInfo;
		FHoudiniApi::AssetInfo_Init(&AssetInfo);
		HAPI_Result Result = FHoudiniApi::GetAssetInfo(
			FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo);
		
		if (Result != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_ERROR(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			return false;
		}

		NodeId = AssetInfo.nodeId;

		// .. the asset's node info
		HAPI_NodeInfo NodeInfo;
		FHoudiniApi::NodeInfo_Init(&NodeInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &NodeInfo), false);

		ParmCount = NodeInfo.parmCount;
	}
	else
	{
		if (!FHoudiniEngineUtils::LoadHoudiniAsset(InHoudiniAsset, AssetLibraryId) )
		{
			HOUDINI_LOG_ERROR(TEXT("Cancelling BuildAllParameters - could not load Houdini Asset."));
			return false;
		}

		// Handle hda files that contain multiple assets
		TArray<HAPI_StringHandle> AssetNames;
		if (!FHoudiniEngineUtils::GetSubAssetNames(AssetLibraryId, AssetNames))
		{
			HOUDINI_LOG_ERROR(TEXT("Cancelling BuildAllParameters - unable to retrieve asset names."));
			return false;
		}
		
		if (AssetNames.Num() == 0)
		{
			HOUDINI_LOG_ERROR(TEXT("Cancelling BuildAllParameters - unable to retrieve asset names."));
			return false;
		}

		// If no InHoudiniAssetName was specified, pick the first asset from the library
		if (InHoudiniAssetName.IsEmpty())
		{
			const FHoudiniEngineString HoudiniEngineString(AssetNames[0]);
			HoudiniEngineString.ToFString(HoudiniAssetName);
		}
		else
		{
			// Ensure that the specified asset name is in the library
			for (const HAPI_StringHandle& Handle : AssetNames)
			{
				const FHoudiniEngineString HoudiniEngineString(Handle);
				FString AssetNameStr;
				HoudiniEngineString.ToFString(AssetNameStr);
				if (AssetNameStr == InHoudiniAssetName)
				{
					HoudiniAssetName = AssetNameStr;
					break;
				}
			}
		}

		if (HoudiniAssetName.IsEmpty())
		{
			HOUDINI_LOG_ERROR(TEXT("Cancelling BuildAllParametersFromAssetDefinition - could not find asset in library."));
			return false;
		}

		HAPI_Result Result = FHoudiniApi::GetAssetDefinitionParmCounts(
			FHoudiniEngine::Get().GetSession(), AssetLibraryId, TCHAR_TO_UTF8(*HoudiniAssetName), &ParmCount,
			&DefaultIntValueCount, &DefaultFloatValueCount, &DefaultStringValueCount, &DefaultChoiceValueCount);
		
		if (Result != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_ERROR(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			return false;
		}

		if (ParmCount > 0)
		{
			// Allocate space in the default value arrays
			// Fetch default values from HAPI
			DefaultIntValues.SetNumZeroed(DefaultIntValueCount);
			DefaultFloatValues.SetNumZeroed(DefaultFloatValueCount);
			DefaultStringValues.SetNumZeroed(DefaultStringValueCount);
			DefaultChoiceValues.SetNumZeroed(DefaultChoiceValueCount);
			
			Result = FHoudiniApi::GetAssetDefinitionParmValues(
				FHoudiniEngine::Get().GetSession(), AssetLibraryId, TCHAR_TO_UTF8(*HoudiniAssetName),
				DefaultIntValues.GetData(), 0, DefaultIntValueCount,
				DefaultFloatValues.GetData(), 0, DefaultFloatValueCount,
				false, DefaultStringValues.GetData(), 0, DefaultStringValueCount,
				DefaultChoiceValues.GetData(), 0, DefaultChoiceValueCount);
			
			if (Result != HAPI_RESULT_SUCCESS)
			{
				HOUDINI_LOG_ERROR(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());
				return false;
			}
		}
	}

	NewParameters.Empty();
	if (ParmCount == 0)
	{
		// The asset doesnt have any parameter, we're done.
		return true;
	}
	else if (ParmCount < 0)
	{
		// Invalid parm count
		return false;
	}

	// Retrieve all the parameter infos either from instantiated node or from asset definition.
	TArray<HAPI_ParmInfo> ParmInfos;
	ParmInfos.SetNumUninitialized(ParmCount);

	if (AssetId >= 0)
	{
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParameters(
				FHoudiniEngine::Get().GetSession(), NodeId, &ParmInfos[0], 0, ParmCount), false);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetDefinitionParmInfos(
				FHoudiniEngine::Get().GetSession(), AssetLibraryId, TCHAR_TO_UTF8(*HoudiniAssetName), &ParmInfos[0], 0, ParmCount), false);
	}

	// Create a name lookup cache for the current parameters
	// Use an array has in some cases, multiple parameters can have the same name!
	TMap<FString, TArray<UHoudiniParameter*>> CurrentParametersByName;
	CurrentParametersByName.Reserve(CurrentParameters.Num());
	for (const auto& Parm : CurrentParameters)
	{
		if (!IsValid(Parm))
			continue;

		FString ParmName = Parm->GetParameterName();
		TArray<UHoudiniParameter*>* FoundParmArray = CurrentParametersByName.Find(ParmName);
		if (!FoundParmArray)
		{
			// Create a new array
			TArray<UHoudiniParameter*> ParmArray;
			ParmArray.Add(Parm);

			// add the new array to the map
			CurrentParametersByName.Add(ParmName, ParmArray);
		}
		else
		{		
			// add this parameter to the existing array
			FoundParmArray->Add(Parm);
		}
	}

	// Create properties for parameters.
	TMap<UHoudiniParameterRampFloat*, int32> FloatRampsToIndex;
	TMap<UHoudiniParameterRampColor*, int32> ColorRampsToIndex;
	TArray<HAPI_ParmId> NewParmIds;
	TArray<int32> AllMultiParams;
	for (int32 ParamIdx = 0; ParamIdx < ParmCount; ++ParamIdx)
	{
		// Retrieve param info at this index.
		const HAPI_ParmInfo & ParmInfo = ParmInfos[ParamIdx];

		// If the parameter is corrupt, skip it
		if (ParmInfo.id < 0 || ParmInfo.childIndex < 0)
		{
			HOUDINI_LOG_WARNING(TEXT("Corrupt parameter %d detected, skipping."), ParamIdx);
			continue;
		}
		
		// If the parameter is invisible, skip it.
		//if (ParmInfo.invisible)
		//	continue;
		
		// Check if any parent folder of this parameter is invisible 
		bool SkipParm = false;
		bool ParentFolderVisible = true;
		HAPI_ParmId ParentId = ParmInfo.parentId;
		while (ParentId > 0 && !SkipParm)
		{
			if (const HAPI_ParmInfo* ParentInfoPtr = ParmInfos.FindByPredicate([=](const HAPI_ParmInfo& Info) {
				return Info.id == ParentId;
			}))
			{
				// We now keep invisible parameters but show/hid them in UpdateParameterFromInfo().
				if (ParentInfoPtr->invisible && ParentInfoPtr->type == HAPI_PARMTYPE_FOLDER)
					ParentFolderVisible = false;

				// Prevent endless loops!
				if (ParentId != ParentInfoPtr->parentId)
					ParentId = ParentInfoPtr->parentId;
				else
					ParentId = -1;
			}
			else
			{
				HOUDINI_LOG_ERROR(TEXT("Could not find parent of parameter %d"), ParmInfo.id);
				SkipParm = true;
			}
		}

		if (SkipParm)
			continue;
		
		// See if this parameter has already been created.
		// We can't use the HAPI_ParmId because it is not unique to parameter instances,
		// so instead, try to find the existing parameter by name using the lookup table
		FString NewParmName;
		FHoudiniEngineString(ParmInfo.nameSH).ToFString(NewParmName);

		EHoudiniParameterType ParmType = EHoudiniParameterType::Invalid;
		FHoudiniParameterTranslator::GetParmTypeFromParmInfo(ParmInfo, ParmType);

		// Not using the name lookup map!
		UHoudiniParameter* FoundHoudiniParameter = nullptr;
		TArray<UHoudiniParameter*>* MatchingParameters = CurrentParametersByName.Find(NewParmName);
		if ((ParmType != EHoudiniParameterType::Invalid) && MatchingParameters)
		{
			//for (auto& CurrentParm : *MatchingParameters)
			for(int32 Idx = MatchingParameters->Num() - 1; Idx >= 0; Idx--)
			{
				UHoudiniParameter* CurrentParm = (*MatchingParameters)[Idx];
				if (!CurrentParm)
					continue;

				// First Check the parameter types match
				if (ParmType != CurrentParm->GetParameterType())
				{
					// Types do not match
					continue;
				}

				// Then, make sure the tuple size hasn't changed
				if (CurrentParm->GetTupleSize() != ParmInfo.size)
				{
					// Tuple do not match
					continue;
				}

				if (!CheckParameterTypeAndClassMatch(CurrentParm, ParmType))
				{
					// Wrong class
					continue;
				}

				// We can reuse this parameter
				FoundHoudiniParameter = CurrentParm;

				// Remove it from the array/map
				MatchingParameters->RemoveAt(Idx);
				if (MatchingParameters->Num() <= 0)
					CurrentParametersByName.Remove(NewParmName);

				break;
			}
		}

		UHoudiniParameter * HoudiniAssetParameter = nullptr;
		if (FoundHoudiniParameter)
		{
			// We can reuse the parameter we found
			HoudiniAssetParameter = FoundHoudiniParameter;

			// Transfer param object from current map to new map
			CurrentParameters.Remove(HoudiniAssetParameter);

			// Do a fast update of this parameter
			if (!FHoudiniParameterTranslator::UpdateParameterFromInfo(
					HoudiniAssetParameter, NodeId, ParmInfo, InForceFullUpdate, bUpdateValues, 
					AssetId >= 0 ? nullptr : &DefaultIntValues,
					AssetId >= 0 ? nullptr : &DefaultFloatValues,
					AssetId >= 0 ? nullptr : &DefaultStringValues,
					AssetId >= 0 ? nullptr : &DefaultChoiceValues))
				continue;

			// Reset the states of ramp parameters.
			switch (HoudiniAssetParameter->GetParameterType())
			{

				case EHoudiniParameterType::FloatRamp:
				{
					UHoudiniParameterRampFloat* FloatRampParam = Cast<UHoudiniParameterRampFloat>(HoudiniAssetParameter);
					if (FloatRampParam)
					{
						// Record float and color ramps for further processing (creating their Points arrays)
						FloatRampsToIndex.Add(FloatRampParam, NewParameters.Num());
						UHoudiniAssetComponent* ParentHAC = Cast<UHoudiniAssetComponent>(FloatRampParam->GetOuter());
						if (ParentHAC && !ParentHAC->HasBeenLoaded() && !ParentHAC->HasBeenDuplicated())
							FloatRampParam->bCaching = false;
					}

					break;
				}

				case EHoudiniParameterType::ColorRamp:
				{
					UHoudiniParameterRampColor* ColorRampParam = Cast<UHoudiniParameterRampColor>(HoudiniAssetParameter);
					if (ColorRampParam)
					{
						// Record float and color ramps for further processing (creating their Points arrays)
						ColorRampsToIndex.Add(ColorRampParam, NewParameters.Num());
						UHoudiniAssetComponent* ParentHAC = Cast<UHoudiniAssetComponent>(ColorRampParam->GetOuter());
						if (ParentHAC && !ParentHAC->HasBeenLoaded() && !ParentHAC->HasBeenDuplicated())
							ColorRampParam->bCaching = false;
					}

					break;
				}
			}

		}
		else
		{	
			// Create a new parameter object of the appropriate type
			HoudiniAssetParameter = CreateTypedParameter(Outer, ParmType, NewParmName);
			// Fully update this parameter
			if (!FHoudiniParameterTranslator::UpdateParameterFromInfo(
					HoudiniAssetParameter, NodeId, ParmInfo, true, true,
					AssetId >= 0 ? nullptr : &DefaultIntValues,
					AssetId >= 0 ? nullptr : &DefaultFloatValues,
					AssetId >= 0 ? nullptr : &DefaultStringValues,
					AssetId >= 0 ? nullptr : &DefaultChoiceValues))
				continue;

			// Record float and color ramps for further processing (creating their Points arrays)
			const EHoudiniParameterType NewParamType = HoudiniAssetParameter->GetParameterType();
			if (NewParamType == EHoudiniParameterType::FloatRamp)
			{
				UHoudiniParameterRampFloat* FloatRampParam = Cast<UHoudiniParameterRampFloat>(HoudiniAssetParameter);
				if (FloatRampParam)
					FloatRampsToIndex.Add(FloatRampParam, NewParameters.Num());
			}
			else if (NewParamType == EHoudiniParameterType::ColorRamp)
			{
				UHoudiniParameterRampColor* ColorRampParam = Cast<UHoudiniParameterRampColor>(HoudiniAssetParameter);
				if (ColorRampParam)
					ColorRampsToIndex.Add(ColorRampParam, NewParameters.Num());
			}
		}

		HoudiniAssetParameter->SetVisibleParent(ParentFolderVisible);
		
		// Add the new parameters
		NewParameters.Add(HoudiniAssetParameter);
		NewParmIds.Add(ParmInfo.id);

		// Check if the parameter is a direct child of a multiparam.
		if (HoudiniAssetParameter->GetParameterType() == EHoudiniParameterType::MultiParm)
			AllMultiParams.Add(HoudiniAssetParameter->GetParmId());

		if (AllMultiParams.Contains(HoudiniAssetParameter->GetParentParmId()))
		{
			HoudiniAssetParameter->SetIsDirectChildOfMultiParm(true);

			// Treat the folderlist whose direct parent is a multi param as a multi param too.
			if (HoudiniAssetParameter->GetParameterType() == EHoudiniParameterType::FolderList)
				AllMultiParams.Add(HoudiniAssetParameter->GetParmId());
		}
	}

	// Assign folder type to all folderlists, 
	// if the first child of the folderlist is Tab or Radio button, set the bIsTabMenu of the folderlistParam to be true, otherwise false
	for (int32 Idx = 0; Idx < NewParameters.Num(); ++Idx) 
	{
		UHoudiniParameter * CurParam = NewParameters[Idx];
		if (!IsValid(CurParam))
			continue;

		if (CurParam->GetParameterType() == EHoudiniParameterType::FolderList) 
		{
			UHoudiniParameterFolderList* CurFolderList = Cast<UHoudiniParameterFolderList>(CurParam);
			if (!IsValid(CurFolderList))
				continue;

			int32 FirstChildIdx = Idx + 1;
			if (!NewParameters.IsValidIndex(FirstChildIdx))
				continue;

			UHoudiniParameterFolder* FirstChildFolder = Cast<UHoudiniParameterFolder>(NewParameters[FirstChildIdx]);
			if (!IsValid(FirstChildFolder))
				continue;

			if (FirstChildFolder->GetFolderType() == EHoudiniFolderParameterType::Radio ||
				FirstChildFolder->GetFolderType() == EHoudiniFolderParameterType::Tabs) 
			{
				// If this is the first time build
				if (!CurFolderList->IsTabMenu())
				{
					// Set the folderlist to be tabs
					CurFolderList->SetIsTabMenu(true);
					// Select the first child tab folder by default.
					FirstChildFolder->SetChosen(true); 
				}
			}
			else
				CurFolderList->SetIsTabMenu(false);
		}
	}

	// Create / update the Points arrays for the ramp parameters
	if (FloatRampsToIndex.Num() > 0)
	{
		for (TPair<UHoudiniParameterRampFloat*, int32> const& Entry : FloatRampsToIndex)
		{
			UHoudiniParameterRampFloat* const RampFloatParam = Entry.Key;
			const int32 ParamIndex = Entry.Value;
			if (!IsValid(RampFloatParam))
				continue;

			RampFloatParam->UpdatePointsArray(NewParameters, ParamIndex + 1);
		}
	}
	if (ColorRampsToIndex.Num() > 0)
	{
		for (TPair<UHoudiniParameterRampColor*, int32> const& Entry : ColorRampsToIndex)
		{
			UHoudiniParameterRampColor* const RampColorParam = Entry.Key;
			const int32 ParamIndex = Entry.Value;
			if (!IsValid(RampColorParam))
				continue;

			RampColorParam->UpdatePointsArray(NewParameters, ParamIndex + 1);
		}
	}
	
	return true;
}


void
FHoudiniParameterTranslator::GetParmTypeFromParmInfo(
	const HAPI_ParmInfo& ParmInfo,
	EHoudiniParameterType& ParmType)
{
	ParmType = EHoudiniParameterType::Invalid;
	//ParmValueType = EHoudiniParameterValueType::Invalid;

	switch (ParmInfo.type)
	{
		case HAPI_PARMTYPE_BUTTON:
			ParmType = EHoudiniParameterType::Button;
			//ParmValueType = EHoudiniParameterValueType::Int;
			break;

		case HAPI_PARMTYPE_STRING:
		{
			if (ParmInfo.choiceCount > 0)
			{
				ParmType = EHoudiniParameterType::StringChoice;
				//ParmValueType = EHoudiniParameterValueType::String;
			}
			else
			{
				ParmType = EHoudiniParameterType::String;
				//ParmValueType = EHoudiniParameterValueType::String;
			}
			break;
		}

		case HAPI_PARMTYPE_INT:
		{
			if (ParmInfo.scriptType == HAPI_PrmScriptType::HAPI_PRM_SCRIPT_TYPE_BUTTONSTRIP) 
			{
				ParmType = EHoudiniParameterType::ButtonStrip;
				break;
			}

			if (ParmInfo.choiceCount > 0)
			{
				ParmType = EHoudiniParameterType::IntChoice;
				//ParmValueType = EHoudiniParameterValueType::Int;
			}
			else
			{
				ParmType = EHoudiniParameterType::Int;
				//ParmValueType = EHoudiniParameterValueType::Int;
			}
			break;
		}

		case HAPI_PARMTYPE_FLOAT:
		{
			ParmType = EHoudiniParameterType::Float;
			//ParmValueType = EHoudiniParameterValueType::Float;
			break;
		}

		case HAPI_PARMTYPE_TOGGLE:
		{
			ParmType = EHoudiniParameterType::Toggle;
			//ParmValueType = EHoudiniParameterValueType::Int;
			break;
		}

		case HAPI_PARMTYPE_COLOR:
		{
			ParmType = EHoudiniParameterType::Color;
			//ParmValueType = EHoudiniParameterValueType::Float;
			break;
		}

		case HAPI_PARMTYPE_LABEL:
		{
			ParmType = EHoudiniParameterType::Label;
			//ParmValueType = EHoudiniParameterValueType::String;
			break;
		}

		case HAPI_PARMTYPE_SEPARATOR:
		{
			ParmType = EHoudiniParameterType::Separator;
			//ParmValueType = EHoudiniParameterValueType::None;
			break;
		}

		case HAPI_PARMTYPE_FOLDERLIST:
		{
			ParmType = EHoudiniParameterType::FolderList;
			//ParmValueType = EHoudiniParameterValueType::None;
			break;
		}

		// Treat radio folders as tab folders for now
		case HAPI_PARMTYPE_FOLDERLIST_RADIO: 
		{
			ParmType = EHoudiniParameterType::FolderList;
			break;
		}

		case HAPI_PARMTYPE_FOLDER:
		{
			ParmType = EHoudiniParameterType::Folder;
			//ParmValueType = EHoudiniParameterValueType::None;
			break;
		}

		case HAPI_PARMTYPE_MULTIPARMLIST:
		{
			if (HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType)
			{
				ParmType = EHoudiniParameterType::FloatRamp;
				//ParmValueType = EHoudiniParameterValueType::Float;
			}
			else if (HAPI_RAMPTYPE_COLOR == ParmInfo.rampType)
			{
				ParmType = EHoudiniParameterType::ColorRamp;
				//ParmValueType = EHoudiniParameterValueType::Float;
			}
			else
			{
				ParmType = EHoudiniParameterType::MultiParm;
				//ParmValueType = EHoudiniParameterValueType::Int;
			}
			break;
		}

		case HAPI_PARMTYPE_PATH_FILE:
		{
			ParmType = EHoudiniParameterType::File;
			//ParmValueType = EHoudiniParameterValueType::String;
			break;
		}

		case HAPI_PARMTYPE_PATH_FILE_DIR:
		{
			ParmType = EHoudiniParameterType::FileDir;
			//ParmValueType = EHoudiniParameterValueType::String;
			break;
		}

		case HAPI_PARMTYPE_PATH_FILE_GEO:
		{
			ParmType = EHoudiniParameterType::FileGeo;
			//ParmValueType = EHoudiniParameterValueType::String;
			break;
		}

		case HAPI_PARMTYPE_PATH_FILE_IMAGE:
		{
			ParmType = EHoudiniParameterType::FileImage;
			//ParmValueType = EHoudiniParameterValueType::String;
			break;
		}

		case HAPI_PARMTYPE_NODE:
		{
			if (ParmInfo.inputNodeType == HAPI_NODETYPE_ANY ||
				ParmInfo.inputNodeType == HAPI_NODETYPE_SOP ||
				ParmInfo.inputNodeType == HAPI_NODETYPE_OBJ)
			{
				ParmType = EHoudiniParameterType::Input;
			}
			else
			{
				ParmType = EHoudiniParameterType::String;
			}
			break;
		}

		default:
		{
			// Just ignore unsupported types for now.
			HOUDINI_LOG_WARNING(TEXT("Parameter Type (%d) is unsupported"), static_cast<int32>(ParmInfo.type));
			break;
		}
	}
}

UClass* 
FHoudiniParameterTranslator::GetDesiredParameterClass(const HAPI_ParmInfo& ParmInfo)
{
	UClass* FoundClass = nullptr;

	switch (ParmInfo.type)
	{
	case HAPI_PARMTYPE_STRING:
		if (!ParmInfo.choiceCount)
			FoundClass = UHoudiniParameterString::StaticClass();
		else
			FoundClass = UHoudiniParameterChoice ::StaticClass();
		break;

	case HAPI_PARMTYPE_INT:
		if (!ParmInfo.choiceCount)
			FoundClass = UHoudiniParameterInt::StaticClass();
		else
			FoundClass = UHoudiniParameterChoice::StaticClass();
		break;

	case HAPI_PARMTYPE_FLOAT:
		FoundClass = UHoudiniParameterFloat::StaticClass();
		break;

	case HAPI_PARMTYPE_TOGGLE:
		FoundClass = UHoudiniParameterToggle::StaticClass();
		break;

	case HAPI_PARMTYPE_COLOR:
		FoundClass = UHoudiniParameterColor::StaticClass();
		break;

	case HAPI_PARMTYPE_LABEL:
		FoundClass = UHoudiniParameterLabel::StaticClass();
		break;

	case HAPI_PARMTYPE_BUTTON:
		FoundClass = UHoudiniParameterButton::StaticClass();
		break;

	case HAPI_PARMTYPE_SEPARATOR:
		FoundClass = UHoudiniParameterSeparator::StaticClass();
		break;

	case HAPI_PARMTYPE_FOLDERLIST:
		FoundClass = UHoudiniParameterFolderList::StaticClass();
		break;

	case HAPI_PARMTYPE_FOLDER:
		FoundClass = UHoudiniParameterFolder::StaticClass();
		break;

	case HAPI_PARMTYPE_MULTIPARMLIST:
	{
		if (HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType || HAPI_RAMPTYPE_COLOR == ParmInfo.rampType)
			FoundClass = UHoudiniParameterRampFloat::StaticClass();
		else if (HAPI_RAMPTYPE_COLOR == ParmInfo.rampType)
			FoundClass = UHoudiniParameterRampColor::StaticClass();
	}
		break;

	case HAPI_PARMTYPE_PATH_FILE:
		FoundClass = UHoudiniParameterFile::StaticClass();
		break;
	case HAPI_PARMTYPE_PATH_FILE_DIR:
		FoundClass = UHoudiniParameterFile::StaticClass();
		break;
	case HAPI_PARMTYPE_PATH_FILE_GEO:
		FoundClass = UHoudiniParameterFile::StaticClass();
		break;
	case HAPI_PARMTYPE_PATH_FILE_IMAGE:
		FoundClass = UHoudiniParameterFile::StaticClass();
		break;

	case HAPI_PARMTYPE_NODE:
		if (ParmInfo.inputNodeType == HAPI_NODETYPE_ANY ||
			ParmInfo.inputNodeType == HAPI_NODETYPE_SOP ||
			ParmInfo.inputNodeType == HAPI_NODETYPE_OBJ)
		{
			FoundClass = UHoudiniParameter::StaticClass();
		}
		else
		{
			FoundClass = UHoudiniParameterString::StaticClass();
		}
		break;
	}

	if (FoundClass == nullptr)
		return UHoudiniParameter::StaticClass();

	return FoundClass;
}

bool
FHoudiniParameterTranslator::CheckParameterTypeAndClassMatch(UHoudiniParameter* Parameter, const EHoudiniParameterType& ParmType)
{
	UClass* FoundClass = Parameter->GetClass();
	bool FailedTypeCheck = true;

	switch (ParmType)
	{
		case EHoudiniParameterType::Invalid:
		{
			FailedTypeCheck = true;
			break;
		}

		case EHoudiniParameterType::Button:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterButton >();
			break;
		}

		case EHoudiniParameterType::Color:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterColor >();
			break;
		}

		case EHoudiniParameterType::ColorRamp:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterRampColor >();
			break;
		}
		case EHoudiniParameterType::FloatRamp:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterRampFloat >();
			break;
		}
		
		case EHoudiniParameterType::File:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterFile >();
			break;
		}
		case EHoudiniParameterType::FileDir:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterFile >();
			break;
		}
		case EHoudiniParameterType::FileGeo:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterFile >();
			break;
		}
		case EHoudiniParameterType::FileImage:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterFile >();
			break;
		}
				
		case EHoudiniParameterType::Float:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterFloat >();
			break;
		}
		
		case EHoudiniParameterType::Folder:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterFolder >();
			break;
		}

		case EHoudiniParameterType::FolderList:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterFolderList >();
			break;
		}

		case EHoudiniParameterType::Input:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterOperatorPath >();
			break;
		}

		case EHoudiniParameterType::Int:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterInt >();
			break;
		}

		case EHoudiniParameterType::IntChoice:
		case EHoudiniParameterType::StringChoice:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterChoice >();
			break;
		}

		case EHoudiniParameterType::Label:
		{ 
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterLabel >();
			break;
		}

		case EHoudiniParameterType::MultiParm:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterMultiParm >();
			break;
		}

		case EHoudiniParameterType::Separator:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterSeparator >();
			break; 
		}

		case EHoudiniParameterType::String:
		case EHoudiniParameterType::StringAssetRef:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterString >();
			break;
		}

		case EHoudiniParameterType::Toggle:
		{
			FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterToggle >();
			break;
		}
	};

	return !FailedTypeCheck;
}
/*
bool
FHoudiniParameterTranslator::CheckParameterClassAndInfoMatch(UHoudiniParameter* Parameter, const HAPI_ParmInfo& ParmInfo)
{
	if (!IsValid(Parameter))
		return false;

	UClass* FoundClass = Parameter->GetClass();
	bool FailedTypeCheck = true;
	switch (ParmInfo.type)
	{
		case HAPI_PARMTYPE_STRING:
			if (!ParmInfo.choiceCount)
				FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterString >();
			else
				FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniParameterChoice >();
			break;

		case HAPI_PARMTYPE_INT:
			if (!ParmInfo.choiceCount)
				FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterInt>();
			else
				FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterChoice>();
			break;

		case HAPI_PARMTYPE_FLOAT:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterFloat>();
			break;

		case HAPI_PARMTYPE_TOGGLE:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterToggle>();
			break;

		case HAPI_PARMTYPE_COLOR:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterColor>();
			break;

		case HAPI_PARMTYPE_LABEL:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterLabel>();
			break;

		case HAPI_PARMTYPE_BUTTON:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterButton>();
			break;

		case HAPI_PARMTYPE_SEPARATOR:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterSeparator>();
			break;

		case HAPI_PARMTYPE_FOLDERLIST:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterFolderList>();
			break;

		case HAPI_PARMTYPE_FOLDER:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterFolder>();
			break;

		case HAPI_PARMTYPE_MULTIPARMLIST:
			if (HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType || HAPI_RAMPTYPE_COLOR == ParmInfo.rampType)
			{
				FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterRamp>();
			}
			break;

		case HAPI_PARMTYPE_PATH_FILE:
		case HAPI_PARMTYPE_PATH_FILE_DIR:
		case HAPI_PARMTYPE_PATH_FILE_GEO:
		case HAPI_PARMTYPE_PATH_FILE_IMAGE:
			FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterFile>();
			break;

		case HAPI_PARMTYPE_NODE:
			if (ParmInfo.inputNodeType == HAPI_NODETYPE_ANY ||
				ParmInfo.inputNodeType == HAPI_NODETYPE_SOP ||
				ParmInfo.inputNodeType == HAPI_NODETYPE_OBJ)
			{
				FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameter>();
			}
			else
			{
				FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniParameterString>();
			}
			break;
	}

	return FailedTypeCheck;
}
*/

UHoudiniParameter *
FHoudiniParameterTranslator::CreateTypedParameter(UObject * Outer, const EHoudiniParameterType& ParmType, const FString& ParmName)
{
	UHoudiniParameter* HoudiniParameter = nullptr;
	// Create a parameter of the desired type
	switch (ParmType)
	{
		case EHoudiniParameterType::Button:
			HoudiniParameter = UHoudiniParameterButton::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::ButtonStrip:
			HoudiniParameter = UHoudiniParameterButtonStrip::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::Color:
			HoudiniParameter = UHoudiniParameterColor::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::ColorRamp:
			HoudiniParameter = UHoudiniParameterRampColor::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::FloatRamp:
			HoudiniParameter = UHoudiniParameterRampFloat::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::File:
			HoudiniParameter = UHoudiniParameterFile::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::FileDir:
			HoudiniParameter = UHoudiniParameterFile::Create(Outer, ParmName);
			HoudiniParameter->SetParameterType(EHoudiniParameterType::FileDir);
			break;

		case EHoudiniParameterType::FileGeo:
			HoudiniParameter = UHoudiniParameterFile::Create(Outer, ParmName);
			HoudiniParameter->SetParameterType(EHoudiniParameterType::FileGeo);
			break;

		case EHoudiniParameterType::FileImage:
			HoudiniParameter = UHoudiniParameterFile::Create(Outer, ParmName);
			HoudiniParameter->SetParameterType(EHoudiniParameterType::FileImage);
			break;

		case EHoudiniParameterType::Float:
			HoudiniParameter = UHoudiniParameterFloat::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::Folder:
			HoudiniParameter = UHoudiniParameterFolder::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::FolderList:
			HoudiniParameter = UHoudiniParameterFolderList::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::Input:
			// Input parameter simply use the base class as all the processingsince is handled by UHoudiniInput
			HoudiniParameter = UHoudiniParameterOperatorPath::Create(Outer, ParmName);
			HoudiniParameter->SetParameterType(ParmType);
			break;

		case EHoudiniParameterType::Int:
			HoudiniParameter = UHoudiniParameterInt::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::IntChoice:
			HoudiniParameter = UHoudiniParameterChoice::Create(Outer, ParmName, EHoudiniParameterType::IntChoice);
			break;

		case EHoudiniParameterType::StringChoice:
			HoudiniParameter = UHoudiniParameterChoice::Create(Outer, ParmName, EHoudiniParameterType::StringChoice);
			break;

		case EHoudiniParameterType::Label:
			HoudiniParameter = UHoudiniParameterLabel::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::MultiParm:
			HoudiniParameter = UHoudiniParameterMultiParm::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::Separator:
			HoudiniParameter = UHoudiniParameterSeparator::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::String:
		case EHoudiniParameterType::StringAssetRef:
			HoudiniParameter = UHoudiniParameterString::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::Toggle:
			HoudiniParameter = UHoudiniParameterToggle::Create(Outer, ParmName);
			break;

		case EHoudiniParameterType::Invalid:
			// TODO handle invalid params
			HoudiniParameter = UHoudiniParameter::Create(Outer, ParmName);
			break;
	}

	return HoudiniParameter;
}

bool
FHoudiniParameterTranslator::UpdateParameterFromInfo(
	UHoudiniParameter * HoudiniParameter, const HAPI_NodeId& InNodeId, const HAPI_ParmInfo& ParmInfo,
	const bool& bFullUpdate, const bool& bUpdateValue,
	const TArray<int>* DefaultIntValues,
	const TArray<float>* DefaultFloatValues,
	const TArray<HAPI_StringHandle>* DefaultStringValues,
	const TArray<HAPI_ParmChoiceInfo>* DefaultChoiceValues)
{
	if (!IsValid(HoudiniParameter))
		return false;

	// Copy values from the ParmInfos
	const bool bHasValidNodeId = InNodeId >= 0;
	if (bHasValidNodeId)
		HoudiniParameter->SetNodeId(InNodeId);
	HoudiniParameter->SetParmId(ParmInfo.id);
	HoudiniParameter->SetParentParmId(ParmInfo.parentId);

	HoudiniParameter->SetChildIndex(ParmInfo.childIndex);
	HoudiniParameter->SetTagCount(ParmInfo.tagCount);
	HoudiniParameter->SetTupleSize(ParmInfo.size);

	HoudiniParameter->SetVisible(!ParmInfo.invisible);
	HoudiniParameter->SetDisabled(ParmInfo.disabled);
	HoudiniParameter->SetSpare(ParmInfo.spare);
	HoudiniParameter->SetJoinNext(ParmInfo.joinNext);

	HoudiniParameter->SetTagCount(ParmInfo.tagCount);
	HoudiniParameter->SetIsChildOfMultiParm(ParmInfo.isChildOfMultiParm);

	UHoudiniParameterMultiParm* MultiParm = Cast<UHoudiniParameterMultiParm>(HoudiniParameter);
	if(MultiParm)
		MultiParm->InstanceStartOffset = ParmInfo.instanceStartOffset;

	

	// Get the parameter type
	EHoudiniParameterType ParmType = HoudiniParameter->GetParameterType();

	// We need to set string values from the parmInfo
	if (bFullUpdate)
	{
		FString Name;
		{
			// Name
			FHoudiniEngineString HoudiniEngineStringName(ParmInfo.nameSH);
			if (HoudiniEngineStringName.ToFString(Name))
				HoudiniParameter->SetParameterName(Name);
		}

		{
			// Label
			FString Label;
			FHoudiniEngineString HoudiniEngineStringLabel(ParmInfo.labelSH);
			if (HoudiniEngineStringLabel.ToFString(Label))
				HoudiniParameter->SetParameterLabel(Label);
		}

		{
			// Help
			FString Help;
			FHoudiniEngineString HoudiniEngineStringHelp(ParmInfo.helpSH);
			if (HoudiniEngineStringHelp.ToFString(Help))
				HoudiniParameter->SetParameterHelp(Help);
		}

		if (bHasValidNodeId &&
			(ParmType == EHoudiniParameterType::String
			|| ParmType == EHoudiniParameterType::Int
			|| ParmType == EHoudiniParameterType::Float
			|| ParmType == EHoudiniParameterType::Toggle
			|| ParmType == EHoudiniParameterType::Color))
		{
			// See if the parm has an expression
			int32 TupleIdx = ParmInfo.intValuesIndex;
			bool bHasExpression = false;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::ParmHasExpression(
				FHoudiniEngine::Get().GetSession(), InNodeId,
				TCHAR_TO_UTF8(*Name), TupleIdx, &bHasExpression))
			{
				// ?
			}

			FString ParmExprString = TEXT("");
			if (bHasExpression)
			{
				// Try to get the expression's value
				HAPI_StringHandle StringHandle;
				if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmExpression(
					FHoudiniEngine::Get().GetSession(), InNodeId,
					TCHAR_TO_UTF8(*Name), TupleIdx, &StringHandle))
				{
					FHoudiniEngineString HoudiniEngineString(StringHandle);
					HoudiniEngineString.ToFString(ParmExprString);
				}

				// Check if we actually have an expression
				// String parameters return true even if they do not have one
				bHasExpression = ParmExprString.Len() > 0;

			}

			HoudiniParameter->SetHasExpression(bHasExpression);
			HoudiniParameter->SetExpression(ParmExprString);
		}
		else
		{
			HoudiniParameter->SetHasExpression(false);
			HoudiniParameter->SetExpression(FString());
		}
		
		// Get parameter tags.
		if (bHasValidNodeId)
		{
			int32 TagCount = HoudiniParameter->GetTagCount();
			for (int32 Idx = 0; Idx < TagCount; ++Idx)
			{
				HAPI_StringHandle TagNameSH;
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmTagName(
					FHoudiniEngine::Get().GetSession(),
					InNodeId, ParmInfo.id, Idx, &TagNameSH))
				{
					HOUDINI_LOG_WARNING(TEXT("Failed to retrive parameter tag name: parmId: %d, tag index: %d"), ParmInfo.id, Idx);
					continue;
				}

				FString NameString = TEXT("");
				FHoudiniEngineString::ToFString(TagNameSH, NameString);
				if (NameString.IsEmpty())
				{
					HOUDINI_LOG_WARNING(TEXT("Failed to retrive parameter tag name: parmId: %d, tag index: %d"), ParmInfo.id, Idx);
					continue;
				}

				HAPI_StringHandle TagValueSH;
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmTagValue(
					FHoudiniEngine::Get().GetSession(),
					InNodeId, ParmInfo.id, TCHAR_TO_ANSI(*NameString), &TagValueSH))
				{
					HOUDINI_LOG_WARNING(TEXT("Failed to retrive parameter tag value: parmId: %d, tag: %s"), ParmInfo.id, *NameString);
				}

				FString ValueString = TEXT("");
				FHoudiniEngineString::ToFString(TagValueSH, ValueString);

				HoudiniParameter->GetTags().Add(NameString, ValueString);
			}
		}
	}

	//
	// Update properties specific to parameter classes
	switch (ParmType)
	{
		case EHoudiniParameterType::Button:
		{
			UHoudiniParameterButton* HoudiniParameterButton = Cast<UHoudiniParameterButton>(HoudiniParameter);
			if (IsValid(HoudiniParameterButton))
			{
				HoudiniParameterButton->SetValueIndex(ParmInfo.intValuesIndex);
			}
		}
		break;

		case EHoudiniParameterType::ButtonStrip:
		{
			UHoudiniParameterButtonStrip* HoudiniParameterButtonStrip = Cast<UHoudiniParameterButtonStrip>(HoudiniParameter);
			if (IsValid(HoudiniParameterButtonStrip)) 
			{
				HoudiniParameterButtonStrip->SetValueIndex(ParmInfo.intValuesIndex);
				HoudiniParameterButtonStrip->Count = ParmInfo.choiceCount;
			}

			if (bFullUpdate && ParmInfo.choiceCount > 0)
			{
				// Get the choice descriptors.
				TArray< HAPI_ParmChoiceInfo > ParmChoices;

				ParmChoices.SetNum(ParmInfo.choiceCount);
				for (int32 Idx = 0; Idx < ParmChoices.Num(); Idx++)
					FHoudiniApi::ParmChoiceInfo_Init(&(ParmChoices[Idx]));

				if (bHasValidNodeId)
				{
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmChoiceLists(
						FHoudiniEngine::Get().GetSession(),
						InNodeId, &ParmChoices[0],
						ParmInfo.choiceIndex, ParmInfo.choiceCount), false);
				}
				else if (DefaultChoiceValues && DefaultChoiceValues->IsValidIndex(ParmInfo.choiceIndex) &&
					DefaultChoiceValues->IsValidIndex(ParmInfo.choiceIndex + ParmInfo.choiceCount - 1))
				{
					FPlatformMemory::Memcpy(
						ParmChoices.GetData(),
						DefaultChoiceValues->GetData() + ParmInfo.choiceIndex,
						sizeof(HAPI_ParmChoiceInfo) * ParmInfo.choiceCount);
				}
				else
				{
					return false;
				}
				
				HoudiniParameterButtonStrip->InitializeLabels(ParmInfo.choiceCount);

				for (int32 ChoiceIdx = 0; ChoiceIdx < ParmChoices.Num(); ++ChoiceIdx)
				{
					FString * ButtonLabel = HoudiniParameterButtonStrip->GetStringLabelAt(ChoiceIdx);
					if (ButtonLabel)
					{
						FHoudiniEngineString HoudiniEngineString(ParmChoices[ChoiceIdx].labelSH);
						if (!HoudiniEngineString.ToFString(*ButtonLabel))
							return false;
					}
				}

				if (bHasValidNodeId)
				{
					if (FHoudiniApi::GetParmIntValues(
						FHoudiniEngine::Get().GetSession(), InNodeId,
						HoudiniParameterButtonStrip->GetValuesPtr(),
						ParmInfo.intValuesIndex, ParmInfo.choiceCount) != HAPI_RESULT_SUCCESS)
					{
						return false;
					}
				}
				else if (DefaultIntValues && DefaultIntValues->IsValidIndex(ParmInfo.intValuesIndex) &&
					DefaultIntValues->IsValidIndex(ParmInfo.intValuesIndex + ParmInfo.choiceCount - 1))
				{
					for (int32 Index = 0; Index < ParmInfo.choiceCount; ++Index)
					{
						HoudiniParameterButtonStrip->SetValueAt(
							Index, (*DefaultIntValues)[ParmInfo.intValuesIndex + Index]);
					}
				}
				else
				{
					return false;
				}
			}
		}
		break;

		case EHoudiniParameterType::Color:
		{
			UHoudiniParameterColor* HoudiniParameterColor = Cast<UHoudiniParameterColor>(HoudiniParameter);
			if (IsValid(HoudiniParameterColor))
			{
				// Set the valueIndex
				HoudiniParameterColor->SetValueIndex(ParmInfo.floatValuesIndex);

				// Update the Parameter value if we want to
				if (bUpdateValue)
				{
					// Get the actual value for this property.
					FLinearColor Color = FLinearColor::White;
					if (bHasValidNodeId)
					{
						if (FHoudiniApi::GetParmFloatValues(
							FHoudiniEngine::Get().GetSession(), InNodeId,
							(float *)&Color.R, ParmInfo.floatValuesIndex, ParmInfo.size) != HAPI_RESULT_SUCCESS)
						{
							return false;
						}
					}
					else if (DefaultFloatValues && DefaultFloatValues->IsValidIndex(ParmInfo.floatValuesIndex) &&
						DefaultFloatValues->IsValidIndex(ParmInfo.floatValuesIndex + ParmInfo.size - 1))
					{
						FPlatformMemory::Memcpy(
							&Color.R,
							DefaultFloatValues->GetData() + ParmInfo.floatValuesIndex,
							sizeof(float) * ParmInfo.size);
					}
					else
					{
						return false;
					}

					HoudiniParameterColor->SetColorValue(Color);
				}

				if (bFullUpdate) 
				{
					// Set the default value at parameter created.
					HoudiniParameterColor->SetDefaultValue();
				}
			}
		}
		break;

		case EHoudiniParameterType::ColorRamp:
		{
			UHoudiniParameterRampColor* HoudiniParameterRampColor = Cast<UHoudiniParameterRampColor>(HoudiniParameter);
			if (IsValid(HoudiniParameterRampColor))
			{
				HoudiniParameterRampColor->SetInstanceCount(ParmInfo.instanceCount);
				HoudiniParameterRampColor->MultiParmInstanceLength = ParmInfo.instanceLength;
			}
		}
			break;
		case EHoudiniParameterType::FloatRamp:
		{
			UHoudiniParameterRampFloat* HoudiniParameterRampFloat = Cast<UHoudiniParameterRampFloat>(HoudiniParameter);
			if (IsValid(HoudiniParameterRampFloat))
			{
				HoudiniParameterRampFloat->SetInstanceCount(ParmInfo.instanceCount);
				HoudiniParameterRampFloat->MultiParmInstanceLength = ParmInfo.instanceLength;
			}	
		}
		break;

		case EHoudiniParameterType::File:
		case EHoudiniParameterType::FileDir:
		case EHoudiniParameterType::FileGeo:
		case EHoudiniParameterType::FileImage:
		{
			UHoudiniParameterFile* HoudiniParameterFile = Cast<UHoudiniParameterFile>(HoudiniParameter);
			if (IsValid(HoudiniParameterFile))
			{
				// Set the valueIndex
				HoudiniParameterFile->SetValueIndex(ParmInfo.stringValuesIndex);

				// Update the file filter and read only tag only for full updates
				if (bFullUpdate)
				{
					// Check if we are read-only
					bool bIsReadOnly = false;
					FString FileChooserTag;
					if (bHasValidNodeId && FHoudiniParameterTranslator::HapiGetParameterTagValue(InNodeId, ParmInfo.id, HAPI_PARAM_TAG_FILE_READONLY, FileChooserTag))
					{
						if (FileChooserTag.Equals(TEXT("read"), ESearchCase::IgnoreCase))
							bIsReadOnly = true;
					}
					HoudiniParameterFile->SetReadOnly(bIsReadOnly);

					// Update the file type using the typeInfo string.
					if (ParmInfo.typeInfoSH > 0)
					{
						FString Filters;
						FHoudiniEngineString HoudiniEngineString(ParmInfo.typeInfoSH);
						if (HoudiniEngineString.ToFString(Filters))
						{
							if (!Filters.IsEmpty())
								HoudiniParameterFile->SetFileFilters(Filters);
						}
					}
				}

				if (bUpdateValue)
				{
					// Get the actual values for this property.
					TArray< HAPI_StringHandle > StringHandles;

					if (bHasValidNodeId)
					{
						StringHandles.SetNumZeroed(ParmInfo.size);
						if (FHoudiniApi::GetParmStringValues(
							FHoudiniEngine::Get().GetSession(), InNodeId, false,
							&StringHandles[0], ParmInfo.stringValuesIndex, ParmInfo.size) != HAPI_RESULT_SUCCESS)
						{
							return false;
						}
					}
					else if (DefaultStringValues && DefaultStringValues->IsValidIndex(ParmInfo.stringValuesIndex) &&
						DefaultStringValues->IsValidIndex(ParmInfo.stringValuesIndex + ParmInfo.size - 1))
					{
						StringHandles.SetNumZeroed(ParmInfo.size);
						FPlatformMemory::Memcpy(
							&StringHandles[0],
							DefaultStringValues->GetData() + ParmInfo.stringValuesIndex,
							sizeof(HAPI_StringHandle) * ParmInfo.size);
					}
					else
					{
						return false;
					}

					// Convert HAPI string handles to Unreal strings.
					HoudiniParameterFile->SetNumberOfValues(ParmInfo.size);
					for (int32 Idx = 0; Idx < StringHandles.Num(); ++Idx)
					{
						FString ValueString = TEXT("");
						FHoudiniEngineString HoudiniEngineString(StringHandles[Idx]);
						HoudiniEngineString.ToFString(ValueString);

						// Update the parameter value
						HoudiniParameterFile->SetValueAt(ValueString, Idx);
					}
				}

				if (bFullUpdate) 
				{
					HoudiniParameterFile->SetDefaultValues();
				}
			}
		}
		break;

		case EHoudiniParameterType::Float:
		{
			UHoudiniParameterFloat* HoudiniParameterFloat = Cast<UHoudiniParameterFloat>(HoudiniParameter);
			if (IsValid(HoudiniParameterFloat))
			{
				// Set the valueIndex
				HoudiniParameterFloat->SetValueIndex(ParmInfo.floatValuesIndex);
				
				if (bUpdateValue)
				{
					// Update the parameter's value
					HoudiniParameterFloat->SetNumberOfValues(ParmInfo.size);

					if (bHasValidNodeId)
					{
						if (FHoudiniApi::GetParmFloatValues(
								FHoudiniEngine::Get().GetSession(), InNodeId,
								HoudiniParameterFloat->GetValuesPtr(),
								ParmInfo.floatValuesIndex, ParmInfo.size) != HAPI_RESULT_SUCCESS)
						{
							return false;
						}
					}
					else if (DefaultFloatValues && DefaultFloatValues->IsValidIndex(ParmInfo.floatValuesIndex) &&
						DefaultFloatValues->IsValidIndex(ParmInfo.floatValuesIndex + ParmInfo.size - 1))
					{
						FPlatformMemory::Memcpy(
							HoudiniParameterFloat->GetValuesPtr(),
							DefaultFloatValues->GetData() + ParmInfo.floatValuesIndex,
							sizeof(float) * ParmInfo.size);
					}
					else
					{
						return false;
					}
				}

				if (bFullUpdate)
				{
					if (ParmInfo.scriptType == HAPI_PrmScriptType::HAPI_PRM_SCRIPT_TYPE_FLOAT_LOG) 
					{
						HoudiniParameterFloat->SetIsLogarithmic(true);
					}

					// set the default float values.
					HoudiniParameterFloat->SetDefaultValues();

					// Only update Unit, no swap, and Min/Max values when doing a full update

					// Get the parameter's unit from the "unit" tag
					FString ParamUnit;
					if (bHasValidNodeId)
					{
						FHoudiniParameterTranslator::HapiGetParameterUnit(InNodeId, ParmInfo.id, ParamUnit);
						HoudiniParameterFloat->SetUnit(ParamUnit);
						// Get the parameter's no swap tag (hengine_noswap)
						HoudiniParameterFloat->SetNoSwap(HapiGetParameterHasTag(InNodeId, ParmInfo.id, HAPI_PARAM_TAG_NOSWAP));
					}

					// Set the min and max for this parameter
					if (ParmInfo.hasMin)
					{
						HoudiniParameterFloat->SetHasMin(true);
						HoudiniParameterFloat->SetMin(ParmInfo.min);
					}
					else
					{
						HoudiniParameterFloat->SetHasMin(false);
						HoudiniParameterFloat->SetMin(TNumericLimits<float>::Lowest());
					}

					if (ParmInfo.hasMax)
					{
						HoudiniParameterFloat->SetHasMax(true);
						HoudiniParameterFloat->SetMax(ParmInfo.max);
					}
					else
					{
						HoudiniParameterFloat->SetHasMax(false);
						HoudiniParameterFloat->SetMax(TNumericLimits<float>::Max());
					}

					// Set min and max for UI for this property.
					bool bUsesDefaultMin = false;
					if (ParmInfo.hasUIMin)
					{
						HoudiniParameterFloat->SetHasUIMin(true);
						HoudiniParameterFloat->SetUIMin(ParmInfo.UIMin);
					}
					else if (ParmInfo.hasMin)
					{
						// If it is not set, use supplied min.
						HoudiniParameterFloat->SetUIMin(ParmInfo.min);
					}
					else
					{
						// Min value Houdini uses by default.
						HoudiniParameterFloat->SetUIMin(HAPI_UNREAL_PARAM_FLOAT_UI_MIN);
						bUsesDefaultMin = true;
					}

					bool bUsesDefaultMax = false;
					if (ParmInfo.hasUIMax)
					{
						HoudiniParameterFloat->SetHasUIMax(true);
						HoudiniParameterFloat->SetUIMax(ParmInfo.UIMax);
					}
					else if (ParmInfo.hasMax)
					{
						// If it is not set, use supplied max.
						HoudiniParameterFloat->SetUIMax(ParmInfo.max);
					}
					else
					{
						// Max value Houdini uses by default.
						HoudiniParameterFloat->SetUIMax(HAPI_UNREAL_PARAM_FLOAT_UI_MAX);
						bUsesDefaultMax = true;
					}

					if (bUsesDefaultMin || bUsesDefaultMax)
					{
						// If we are using defaults, we can detect some most common parameter names and alter defaults.
						FString LocalParameterName = HoudiniParameterFloat->GetParameterName();
						FHoudiniEngineString HoudiniEngineString(ParmInfo.nameSH);
						HoudiniEngineString.ToFString(LocalParameterName);

						static const FString ParameterNameTranslate(TEXT(HAPI_UNREAL_PARAM_TRANSLATE));
						static const FString ParameterNameRotate(TEXT(HAPI_UNREAL_PARAM_ROTATE));
						static const FString ParameterNameScale(TEXT(HAPI_UNREAL_PARAM_SCALE));
						static const FString ParameterNamePivot(TEXT(HAPI_UNREAL_PARAM_PIVOT));

						if (!LocalParameterName.IsEmpty())
						{
							if (LocalParameterName.Equals(ParameterNameTranslate)
								|| LocalParameterName.Equals(ParameterNameScale)
								|| LocalParameterName.Equals(ParameterNamePivot))
							{
								if (bUsesDefaultMin)
								{
									HoudiniParameterFloat->SetUIMin(-1.0f);
								}
								if (bUsesDefaultMax)
								{
									HoudiniParameterFloat->SetUIMax(1.0f);
								}
							}
							else if (LocalParameterName.Equals(ParameterNameRotate))
							{
								if (bUsesDefaultMin)
								{
									HoudiniParameterFloat->SetUIMin(0.0f);
								}
								if (bUsesDefaultMax)
								{
									HoudiniParameterFloat->SetUIMax(360.0f);
								}
							}
						}
					}
				}
			}	
		}
		break;

		case EHoudiniParameterType::Folder:
		{
			UHoudiniParameterFolder* HoudiniParameterFolder = Cast<UHoudiniParameterFolder>(HoudiniParameter);
			if (IsValid(HoudiniParameterFolder))
			{
				// Set the valueIndex
				HoudiniParameterFolder->SetValueIndex(ParmInfo.intValuesIndex);
				HoudiniParameterFolder->SetFolderType(GetFolderTypeFromParamInfo(&ParmInfo));
			}
		}
		break;

		case EHoudiniParameterType::FolderList:
		{
			UHoudiniParameterFolderList* HoudiniParameterFolderList = Cast<UHoudiniParameterFolderList>(HoudiniParameter);
			if (IsValid(HoudiniParameterFolderList))
			{
				// Set the valueIndex
				HoudiniParameterFolderList->SetValueIndex(ParmInfo.intValuesIndex);
			}
		}
		break;

		case EHoudiniParameterType::Input:
		{
			// Inputs parameters are just stored, and handled separately by UHoudiniInputs
			UHoudiniParameterOperatorPath* HoudiniParameterOperatorPath = Cast<UHoudiniParameterOperatorPath>(HoudiniParameter);
			if (IsValid(HoudiniParameterOperatorPath))
			{
				/*
				// DO NOT CREATE A DUPLICATE INPUT HERE!
				// Inputs are created by the input translator, and will be tied to this parameter there
				UHoudiniInput * NewInput = NewObject< UHoudiniInput >(
				HoudiniParameterOperatorPath,
				UHoudiniInput::StaticClass());

				UHoudiniAssetComponent *ParentHAC = Cast<UHoudiniAssetComponent>(HoudiniParameterOperatorPath->GetOuter());

				if (!ParentHAC)
					return false;

				if (!IsValid(NewInput))
					return false;
				
				// Set the nodeId
				NewInput->SetAssetNodeId(ParentHAC->GetAssetId());
				NewInput->SetInputType(EHoudiniInputType::Geometry);
				HoudiniParameterOperatorPath->HoudiniInputs.Add(NewInput);
				*/
				// Set the valueIndex
				HoudiniParameterOperatorPath->SetValueIndex(ParmInfo.stringValuesIndex);
			}
		}
		break;

		case EHoudiniParameterType::Int:
		{
			UHoudiniParameterInt* HoudiniParameterInt = Cast<UHoudiniParameterInt>(HoudiniParameter);
			if (IsValid(HoudiniParameterInt))
			{
				// Set the valueIndex
				HoudiniParameterInt->SetValueIndex(ParmInfo.intValuesIndex);

				if (bUpdateValue)
				{
					// Get the actual values for this property.
					HoudiniParameterInt->SetNumberOfValues(ParmInfo.size);

					if (bHasValidNodeId)
					{
						if (FHoudiniApi::GetParmIntValues(
							FHoudiniEngine::Get().GetSession(), InNodeId,
							HoudiniParameterInt->GetValuesPtr(),
							ParmInfo.intValuesIndex, ParmInfo.size) != HAPI_RESULT_SUCCESS)
						{
							return false;
						}
					}
					else if (DefaultIntValues && DefaultIntValues->IsValidIndex(ParmInfo.intValuesIndex) &&
						DefaultIntValues->IsValidIndex(ParmInfo.intValuesIndex + ParmInfo.size - 1))
					{
						for (int32 Index = 0; Index < ParmInfo.size; ++Index)
						{
							// TODO: cannot use SetValueAt: Min/Max has not yet been configured and defaults to 0,0
							// so the value is clamped to 0
							// HoudiniParameterInt->SetValueAt(
							// 	(*DefaultIntValues)[ParmInfo.intValuesIndex + Index], Index);
							*(HoudiniParameterInt->GetValuesPtr() + Index) = (*DefaultIntValues)[ParmInfo.intValuesIndex + Index];
						}
					}
					else
					{
						return false;
					}
				}

				if (bFullUpdate)
				{
					if (ParmInfo.scriptType == HAPI_PrmScriptType::HAPI_PRM_SCRIPT_TYPE_INT_LOG)
					{
						HoudiniParameterInt->SetIsLogarithmic(true);
					}

					// Set the default int values at created
					HoudiniParameterInt->SetDefaultValues();
					// Only update unit and Min/Max values for a full update

					// Get the parameter's unit from the "unit" tag
					FString ParamUnit;
					if (bHasValidNodeId)
					{
						FHoudiniParameterTranslator::HapiGetParameterUnit(InNodeId, ParmInfo.id, ParamUnit);
						HoudiniParameterInt->SetUnit(ParamUnit);
					}

					// Set the min and max for this parameter
					if (ParmInfo.hasMin)
					{
						HoudiniParameterInt->SetHasMin(true);
						HoudiniParameterInt->SetMin((int32)ParmInfo.min);
					}
					else
					{
						HoudiniParameterInt->SetHasMin(false);
						HoudiniParameterInt->SetMin(TNumericLimits<int32>::Lowest());
					}

					if (ParmInfo.hasMax)
					{
						HoudiniParameterInt->SetHasMax(true);
						HoudiniParameterInt->SetMax((int32)ParmInfo.max);
					}
					else
					{
						HoudiniParameterInt->SetHasMax(false);
						HoudiniParameterInt->SetMax(TNumericLimits<int32>::Max());
					}

					// Set min and max for UI for this property.
					if (ParmInfo.hasUIMin)
					{
						HoudiniParameterInt->SetHasUIMin(true);
						HoudiniParameterInt->SetUIMin((int32)ParmInfo.UIMin);
					}
					else if (ParmInfo.hasMin)
					{
						// If it is not set, use supplied min.
						HoudiniParameterInt->SetUIMin((int32)ParmInfo.min);
					}
					else
					{
						// Min value Houdini uses by default.
						HoudiniParameterInt->SetUIMin(HAPI_UNREAL_PARAM_INT_UI_MIN);
					}

					if (ParmInfo.hasUIMax)
					{
						HoudiniParameterInt->SetHasUIMax(true);
						HoudiniParameterInt->SetUIMax((int32)ParmInfo.UIMax);
					}
					else if (ParmInfo.hasMax)
					{
						// If it is not set, use supplied max.
						HoudiniParameterInt->SetUIMax((int32)ParmInfo.max);
					}
					else
					{
						// Max value Houdini uses by default.
						HoudiniParameterInt->SetUIMax(HAPI_UNREAL_PARAM_INT_UI_MAX);
					}
				}
			}
		}
		break;

		case EHoudiniParameterType::IntChoice:
		{
			UHoudiniParameterChoice* HoudiniParameterIntChoice = Cast<UHoudiniParameterChoice>(HoudiniParameter);
			if (IsValid(HoudiniParameterIntChoice))
			{
				// Set the valueIndex
				HoudiniParameterIntChoice->SetValueIndex(ParmInfo.intValuesIndex);

				if (bUpdateValue)
				{
					// Get the actual values for this property.
					int32 CurrentIntValue = 0;

					if (bHasValidNodeId)
					{
						HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIntValues(
							FHoudiniEngine::Get().GetSession(),
							InNodeId, &CurrentIntValue,
							ParmInfo.intValuesIndex, 1/*ParmInfo.size*/), false);
					}
					else if (DefaultIntValues && DefaultIntValues->IsValidIndex(ParmInfo.intValuesIndex))
					{
						CurrentIntValue = (*DefaultIntValues)[ParmInfo.intValuesIndex];
					}
					else
					{
						return false;
					}

					// Get the value from the index array, if applicable.
					if (CurrentIntValue < HoudiniParameterIntChoice->GetNumChoices())
						CurrentIntValue = HoudiniParameterIntChoice->GetIndexFromValueArray(CurrentIntValue);

					// Check the value is valid
					if (CurrentIntValue >= ParmInfo.choiceCount)
					{
						HOUDINI_LOG_WARNING(TEXT("parm '%s' has an invalid value %d, menu tokens are not supported for choice menus"),
							*HoudiniParameterIntChoice->GetParameterName(), CurrentIntValue);
						CurrentIntValue = 0;
					}

					HoudiniParameterIntChoice->SetIntValue(CurrentIntValue);
				}

				// Get the choice descriptors
				if (bFullUpdate)
				{
					// Set the default value at created
					HoudiniParameterIntChoice->SetDefaultIntValue();
					// Get the choice descriptors.
					TArray< HAPI_ParmChoiceInfo > ParmChoices;

					ParmChoices.SetNum(ParmInfo.choiceCount);
					for (int32 Idx = 0; Idx < ParmChoices.Num(); Idx++)
						FHoudiniApi::ParmChoiceInfo_Init(&(ParmChoices[Idx]));

					if (bHasValidNodeId)
					{
						HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmChoiceLists(
							FHoudiniEngine::Get().GetSession(), 
							InNodeId, &ParmChoices[0],
							ParmInfo.choiceIndex, ParmInfo.choiceCount), false);
					}
					else if (DefaultChoiceValues && DefaultChoiceValues->IsValidIndex(ParmInfo.choiceIndex) &&
						DefaultChoiceValues->IsValidIndex(ParmInfo.choiceIndex + ParmInfo.choiceCount - 1))
					{
						FPlatformMemory::Memcpy(
							ParmChoices.GetData(),
							DefaultChoiceValues->GetData() + ParmInfo.choiceIndex,
							sizeof(HAPI_ParmChoiceInfo) * ParmInfo.choiceCount);
					}
					else
					{
						return false;
					}

					// Set the array sizes
					HoudiniParameterIntChoice->SetNumChoices(ParmInfo.choiceCount);

					bool bMatchedSelectionLabel = false;
					int32 CurrentIntValue = HoudiniParameterIntChoice->GetIntValueIndex();
					for (int32 ChoiceIdx = 0; ChoiceIdx < ParmChoices.Num(); ++ChoiceIdx)
					{
						FString * ChoiceLabel = HoudiniParameterIntChoice->GetStringChoiceLabelAt(ChoiceIdx);
						if (ChoiceLabel)
						{
							FHoudiniEngineString HoudiniEngineString(ParmChoices[ChoiceIdx].labelSH);
							if (!HoudiniEngineString.ToFString(*ChoiceLabel))
								return false;
							//StringChoiceLabels.Add(TSharedPtr< FString >(ChoiceLabel));
						}

						// Match our string value to the corresponding selection label.
						if (ChoiceIdx == CurrentIntValue)
						{
							HoudiniParameterIntChoice->SetStringValue(*ChoiceLabel);
						}

						int32 IntValue = ChoiceIdx;

						// If useMenuItemTokenAsValue is set, then the value is not the index. Find the value using the token, if possible.
						if (ParmInfo.useMenuItemTokenAsValue)
						{
							if (ChoiceIdx < ParmChoices.Num())
							{
								FHoudiniEngineString HoudiniEngineString(ParmChoices[ChoiceIdx].labelSH);
								FString Token;

								if (HoudiniEngineString.ToFString(Token))
								{
									int32 Value = FCString::Atoi(*Token);
									IntValue = Value;
								}
							}
						}

						HoudiniParameterIntChoice->SetIntValueArray(ChoiceIdx, IntValue);
					}
				}
				else if (bUpdateValue)
				{
					// We still need to match the string value to the label
					HoudiniParameterIntChoice->UpdateStringValueFromInt();
				}
			}
		}
		break;

		case EHoudiniParameterType::StringChoice:
		{
			UHoudiniParameterChoice* HoudiniParameterStringChoice = Cast<UHoudiniParameterChoice>(HoudiniParameter);
			if (IsValid(HoudiniParameterStringChoice))
			{
				// Set the valueIndex
				HoudiniParameterStringChoice->SetValueIndex(ParmInfo.stringValuesIndex);

				if (bUpdateValue)
				{
					// Get the actual values for this property.
					HAPI_StringHandle StringHandle;

					if (bHasValidNodeId)
					{
						HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmStringValues(
							FHoudiniEngine::Get().GetSession(),
							InNodeId, false, &StringHandle,
							ParmInfo.stringValuesIndex, 1/*ParmInfo.size*/), false);
					}
					else if (DefaultStringValues && DefaultStringValues->IsValidIndex(ParmInfo.stringValuesIndex))
					{
						StringHandle = (*DefaultStringValues)[ParmInfo.stringValuesIndex];
					}
					else
					{
						return false;
					}

					// Get the string value
					FString StringValue;
					FHoudiniEngineString HoudiniEngineString(StringHandle);
					HoudiniEngineString.ToFString(StringValue);

					HoudiniParameterStringChoice->SetStringValue(StringValue);
				}

				// Get the choice descriptors
				if (bFullUpdate)
				{
					// Set default value at created.
					HoudiniParameterStringChoice->SetDefaultStringValue();
					// Get the choice descriptors.
					TArray< HAPI_ParmChoiceInfo > ParmChoices;

					ParmChoices.SetNum(ParmInfo.choiceCount);
					for (int32 Idx = 0; Idx < ParmChoices.Num(); Idx++)
						FHoudiniApi::ParmChoiceInfo_Init(&(ParmChoices[Idx]));

					if (bHasValidNodeId)
					{
						HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmChoiceLists(
							FHoudiniEngine::Get().GetSession(),
							InNodeId, &ParmChoices[0],
							ParmInfo.choiceIndex, ParmInfo.choiceCount), false);
					}
					else if (DefaultChoiceValues && DefaultChoiceValues->IsValidIndex(ParmInfo.choiceIndex) &&
						DefaultChoiceValues->IsValidIndex(ParmInfo.choiceIndex + ParmInfo.choiceCount - 1))
					{
						FPlatformMemory::Memcpy(
							ParmChoices.GetData(),
							DefaultChoiceValues->GetData() + ParmInfo.choiceIndex,
							sizeof(HAPI_ParmChoiceInfo) * ParmInfo.choiceCount);
					}
					else
					{
						return false;
					}

					// Set the array sizes
					HoudiniParameterStringChoice->SetNumChoices(ParmInfo.choiceCount);

					bool bMatchedSelectionLabel = false;
					FString CurrentStringValue = HoudiniParameterStringChoice->GetStringValue();
					for (int32 ChoiceIdx = 0; ChoiceIdx < ParmChoices.Num(); ++ChoiceIdx)
					{
						FString * ChoiceValue = HoudiniParameterStringChoice->GetStringChoiceValueAt(ChoiceIdx);
						if (ChoiceValue)
						{
							FHoudiniEngineString HoudiniEngineString(ParmChoices[ChoiceIdx].valueSH);
							if (!HoudiniEngineString.ToFString(*ChoiceValue))
								return false;
							//StringChoiceValues.Add(TSharedPtr< FString >(ChoiceValue));
						}

						FString * ChoiceLabel = HoudiniParameterStringChoice->GetStringChoiceLabelAt(ChoiceIdx);
						if (ChoiceLabel)
						{
							FHoudiniEngineString HoudiniEngineString(ParmChoices[ChoiceIdx].labelSH);
							if (!HoudiniEngineString.ToFString(*ChoiceLabel))
								return false;
							//StringChoiceLabels.Add(TSharedPtr< FString >(ChoiceLabel));
						}

						// If this is a string choice list, we need to match name with corresponding selection label.
						if (!bMatchedSelectionLabel && ChoiceValue->Equals(CurrentStringValue))
						{
							bMatchedSelectionLabel = true;
							HoudiniParameterStringChoice->SetIntValue(ChoiceIdx);
						}
					}
				}
				else if (bUpdateValue)
				{
					// We still need to match the string value to the label
					HoudiniParameterStringChoice->UpdateIntValueFromString();
				}
			}
		}
		break;

		case EHoudiniParameterType::Label:
		{
			UHoudiniParameterLabel* HoudiniParameterLabel = Cast<UHoudiniParameterLabel>(HoudiniParameter);
			if (IsValid(HoudiniParameterLabel))
			{
				if (ParmInfo.type != HAPI_PARMTYPE_LABEL)
					return false;

				// Set the valueIndex
				HoudiniParameterLabel->SetValueIndex(ParmInfo.stringValuesIndex);

				// Get the actual value for this property.
				TArray<HAPI_StringHandle> StringHandles;

				if (bHasValidNodeId)
				{
					StringHandles.SetNumZeroed(ParmInfo.size);
					FHoudiniApi::GetParmStringValues(
						FHoudiniEngine::Get().GetSession(),
						InNodeId, false, &StringHandles[0],
						ParmInfo.stringValuesIndex, ParmInfo.size);
				}
				else if (DefaultStringValues && DefaultStringValues->IsValidIndex(ParmInfo.stringValuesIndex) &&
						DefaultStringValues->IsValidIndex(ParmInfo.stringValuesIndex + ParmInfo.size - 1))
				{
					StringHandles.SetNumZeroed(ParmInfo.size);
					FPlatformMemory::Memcpy(
						StringHandles.GetData(),
						DefaultStringValues->GetData() + ParmInfo.stringValuesIndex,
						sizeof(HAPI_StringHandle) * ParmInfo.size);
				}
				else
				{
					return false;
				}
				
				HoudiniParameterLabel->EmptyLabelString();

				// Convert HAPI string handles to Unreal strings.
				for (int32 Idx = 0; Idx < StringHandles.Num(); ++Idx)
				{
					FString ValueString = TEXT("");
					FHoudiniEngineString HoudiniEngineString(StringHandles[Idx]);
					HoudiniEngineString.ToFString(ValueString);
					HoudiniParameterLabel->AddLabelString(ValueString);
				}
			}
		}
		break;

		case EHoudiniParameterType::MultiParm:
		{
			UHoudiniParameterMultiParm* HoudiniParameterMulti = Cast<UHoudiniParameterMultiParm>(HoudiniParameter);
			if (IsValid(HoudiniParameterMulti))
			{
				if (ParmInfo.type != HAPI_PARMTYPE_MULTIPARMLIST)
					return false;

				// Set the valueIndex
				HoudiniParameterMulti->SetValueIndex(ParmInfo.intValuesIndex);

				// Set the multiparm value
				int32 MultiParmValue = 0;

				if (bHasValidNodeId)
				{
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIntValues(
						FHoudiniEngine::Get().GetSession(),
						InNodeId, &MultiParmValue, ParmInfo.intValuesIndex, 1), false);
				}
				else if (DefaultIntValues && DefaultIntValues->IsValidIndex(ParmInfo.intValuesIndex))
				{
					MultiParmValue = (*DefaultIntValues)[ParmInfo.intValuesIndex];
				}
				else
				{
					return false;
				}

				HoudiniParameterMulti->SetValue(MultiParmValue);
				HoudiniParameterMulti->MultiParmInstanceCount = ParmInfo.instanceCount;
				HoudiniParameterMulti->MultiParmInstanceLength = ParmInfo.instanceLength;

			}

			if (bFullUpdate) 
			{
				HoudiniParameterMulti->SetDefaultInstanceCount(ParmInfo.instanceCount);
			}
		}
		break;

		case EHoudiniParameterType::Separator:
		{
			UHoudiniParameterSeparator* HoudiniParameterSeparator = Cast<UHoudiniParameterSeparator>(HoudiniParameter);
			if (IsValid(HoudiniParameterSeparator))
			{
				// We can only handle separator type.
				if (ParmInfo.type != HAPI_PARMTYPE_SEPARATOR)
					return false;

				// Set the valueIndex
				HoudiniParameterSeparator->SetValueIndex(ParmInfo.stringValuesIndex);
			}
		}
		break;

		case EHoudiniParameterType::String:
		case EHoudiniParameterType::StringAssetRef:
		{
			UHoudiniParameterString* HoudiniParameterString = Cast<UHoudiniParameterString>(HoudiniParameter);
			if (IsValid(HoudiniParameterString))
			{
				// We can only handle string type.
				if (ParmInfo.type != HAPI_PARMTYPE_STRING && ParmInfo.type != HAPI_PARMTYPE_NODE)
					return false;

				// Set the valueIndex
				HoudiniParameterString->SetValueIndex(ParmInfo.stringValuesIndex);

				// Stop if we don't want to update the value
				if (bUpdateValue)
				{
					// Get the actual value for this property.
					TArray< HAPI_StringHandle > StringHandles;

					if (bHasValidNodeId)
					{
						StringHandles.SetNumZeroed(ParmInfo.size);
						if (FHoudiniApi::GetParmStringValues(
							FHoudiniEngine::Get().GetSession(),
							InNodeId, false, &StringHandles[0],
							ParmInfo.stringValuesIndex, ParmInfo.size) != HAPI_RESULT_SUCCESS)
						{
							return false;
						}
					}
					else if (DefaultStringValues && DefaultStringValues->IsValidIndex(ParmInfo.stringValuesIndex) &&
						DefaultStringValues->IsValidIndex(ParmInfo.stringValuesIndex + ParmInfo.size - 1))
					{
						StringHandles.SetNumZeroed(ParmInfo.size);
						FPlatformMemory::Memcpy(
							StringHandles.GetData(),
							DefaultStringValues->GetData() + ParmInfo.stringValuesIndex,
							sizeof(HAPI_StringHandle) * ParmInfo.size);
					}
					else
					{
						return false;
					}

					// Convert HAPI string handles to Unreal strings.
					HoudiniParameterString->SetNumberOfValues(ParmInfo.size);
					for (int32 Idx = 0; Idx < StringHandles.Num(); ++Idx)
					{
						FString ValueString = TEXT("");
						FHoudiniEngineString HoudiniEngineString(StringHandles[Idx]);
						HoudiniEngineString.ToFString(ValueString);
						HoudiniParameterString->SetValueAt(ValueString, Idx);
					}
				}

				if (bFullUpdate)
				{
					// Set default string values on created
					HoudiniParameterString->SetDefaultValues();
					// Check if the parameter has the "asset_ref" tag
					if (bHasValidNodeId)
					{
						HoudiniParameterString->SetIsAssetRef(
							FHoudiniParameterTranslator::HapiGetParameterHasTag(InNodeId, ParmInfo.id, HOUDINI_PARAMETER_STRING_REF_TAG));
					}
				}
			}
		}
		break;

		case EHoudiniParameterType::Toggle:
		{
			UHoudiniParameterToggle* HoudiniParameterToggle = Cast<UHoudiniParameterToggle>(HoudiniParameter);
			if (IsValid(HoudiniParameterToggle))
			{
				if (ParmInfo.type != HAPI_PARMTYPE_TOGGLE)
					return false;

				// Set the valueIndex
				HoudiniParameterToggle->SetValueIndex(ParmInfo.intValuesIndex);

				// Stop if we don't want to update the value
				if (bUpdateValue)
				{
					// Get the actual values for this property.
					HoudiniParameterToggle->SetNumberOfValues(ParmInfo.size);

					if (bHasValidNodeId)
					{
						if (FHoudiniApi::GetParmIntValues(
							FHoudiniEngine::Get().GetSession(), InNodeId,
							HoudiniParameterToggle->GetValuesPtr(),
							ParmInfo.intValuesIndex, ParmInfo.size) != HAPI_RESULT_SUCCESS)
						{
							return false;
						}
					}
					else if (DefaultIntValues && DefaultIntValues->IsValidIndex(ParmInfo.intValuesIndex) &&
						DefaultIntValues->IsValidIndex(ParmInfo.intValuesIndex + ParmInfo.size - 1))
					{
						for (int32 Index = 0; Index < ParmInfo.size; ++Index)
						{
							HoudiniParameterToggle->SetValueAt(
								(*DefaultIntValues)[ParmInfo.intValuesIndex + Index] != 0, Index);
						}
					}
					else
					{
						return false;
					}
				}
			}

			if (bFullUpdate) 
			{
				HoudiniParameterToggle->SetDefaultValues();
			}
		}
		break;

		case EHoudiniParameterType::Invalid:
		{
			// TODO
		}
		break;
	}

	return true;
}

bool
FHoudiniParameterTranslator::HapiGetParameterTagValue(const HAPI_NodeId& NodeId, const HAPI_ParmId& ParmId, const FString& Tag, FString& TagValue)
{
	// Default
	TagValue = FString();

	// Does the parameter has the tag?
	bool HasTag = false;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ParmHasTag(
		FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
		TCHAR_TO_ANSI(*Tag), &HasTag), false);

	if (!HasTag)
		return false;

	// Get the tag string value
	HAPI_StringHandle StringHandle;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmTagValue(
		FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
		TCHAR_TO_ANSI(*Tag), &StringHandle), false);

	FHoudiniEngineString HoudiniEngineString(StringHandle);
	if (HoudiniEngineString.ToFString(TagValue))
	{
		return true;
	}

	return false;
}


bool
FHoudiniParameterTranslator::HapiGetParameterUnit(const HAPI_NodeId& NodeId, const HAPI_ParmId& ParmId, FString& OutUnitString)
{
	//
	OutUnitString = TEXT("");

	// We're looking for the parameter unit tag
	//FString UnitTag = "units";	

	// Get the actual string value.
	FString UnitString = TEXT("");
	if (!FHoudiniParameterTranslator::HapiGetParameterTagValue(NodeId, ParmId, "units", UnitString))
		return false;
	
	// We need to do some replacement in the string here in order to be able to get the
	// proper unit type when calling FUnitConversion::UnitFromString(...) after.

	// Per second and per hour are the only "per" unit that unreal recognize
	UnitString.ReplaceInline(TEXT("s-1"), TEXT("/s"));
	UnitString.ReplaceInline(TEXT("h-1"), TEXT("/h"));

	// Houdini likes to add '1' on all the unit, so we'll remove all of them
	// except the '-1' that still needs to remain.
	UnitString.ReplaceInline(TEXT("-1"), TEXT("--"));
	UnitString.ReplaceInline(TEXT("1"), TEXT(""));
	UnitString.ReplaceInline(TEXT("--"), TEXT("-1"));

	OutUnitString = UnitString;

	return true;
}

bool
FHoudiniParameterTranslator::HapiGetParameterHasTag(const HAPI_NodeId& NodeId, const HAPI_ParmId& ParmId, const FString& Tag)
{
	// Does the parameter has the tag we're looking for?
	bool HasTag = false;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ParmHasTag(
		FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
		TCHAR_TO_ANSI(*Tag), &HasTag), false);

	return HasTag;
}


bool
FHoudiniParameterTranslator::UploadChangedParameters( UHoudiniAssetComponent * HAC )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniParameterTranslator::UploadChangedParameters);

	if (!IsValid(HAC))
		return false;

	TMap<FString, UHoudiniParameter*> RampsToRevert;
	// First upload all parameters, including the current child parameters/points of ramps, and then process
	// the ramp parameters themselves (delete and insert operations of ramp points)
	// This is so that the initial upload of parameter values use the correct parameter value/tuple array indices
	// (which will change after potential insert/delete operations). Insert operations will upload their new
	// parameter values after the insert.
	TArray<UHoudiniParameter*> RampsToUpload;

	for (int32 ParmIdx = 0; ParmIdx < HAC->GetNumParameters(); ParmIdx++)
	{
		UHoudiniParameter*& CurrentParm = HAC->Parameters[ParmIdx];
		if (!IsValid(CurrentParm) || !CurrentParm->HasChanged())
			continue;

		bool bSuccess = false;

		const EHoudiniParameterType CurrentParmType = CurrentParm->GetParameterType();
		if (CurrentParm->IsPendingRevertToDefault())
		{
			bSuccess = RevertParameterToDefault(CurrentParm);

			if (CurrentParmType == EHoudiniParameterType::FloatRamp ||
				CurrentParmType == EHoudiniParameterType::ColorRamp) 
			{
				RampsToRevert.Add(CurrentParm->GetParameterName(), CurrentParm);
			}
		}
		else
		{
			if (CurrentParmType == EHoudiniParameterType::FloatRamp ||
				CurrentParmType == EHoudiniParameterType::ColorRamp)
			{
				RampsToUpload.Add(CurrentParm);
			}
			else
			{
				bSuccess = UploadParameterValue(CurrentParm);
			}
		}


		if (bSuccess)
		{
			CurrentParm->MarkChanged(false);
			//CurrentParm->SetNeedsToTriggerUpdate(false);
		}
		else
		{
			// Keep this param marked as changed but prevent it from generating updates
			CurrentParm->SetNeedsToTriggerUpdate(false);
		}
	}

	FHoudiniParameterTranslator::RevertRampParameters(RampsToRevert, HAC->GetAssetId());

	for (UHoudiniParameter* const RampParam : RampsToUpload)
	{
		if (!IsValid(RampParam))
			continue;

		if (UploadParameterValue(RampParam))
			RampParam->MarkChanged(false);
	}

	return true;
}

bool
FHoudiniParameterTranslator::UploadParameterValue(UHoudiniParameter* InParam)
{
	if (!IsValid(InParam))
		return false;

	switch (InParam->GetParameterType())
	{
		case EHoudiniParameterType::Float:
		{
			UHoudiniParameterFloat* FloatParam = Cast<UHoudiniParameterFloat>(InParam);
			if (!IsValid(FloatParam))
			{
				return false;
			}

			float* DataPtr = FloatParam->GetValuesPtr();
			if (!DataPtr)
			{
				return false;
			}

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValues(
				FHoudiniEngine::Get().GetSession(),
				FloatParam->GetNodeId(), DataPtr, FloatParam->GetValueIndex(), FloatParam->GetTupleSize()), false);
		}
		break;

		case EHoudiniParameterType::Int:
		{
			UHoudiniParameterInt* IntParam = Cast<UHoudiniParameterInt>(InParam);
			if (!IsValid(IntParam))
			{
				return false;
			}

			int32* DataPtr = IntParam->GetValuesPtr();
			if (!DataPtr)
			{
				return false;
			}

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValues(
				FHoudiniEngine::Get().GetSession(),
				IntParam->GetNodeId(), DataPtr, IntParam->GetValueIndex(), IntParam->GetTupleSize()), false);
		}
		break;

		case EHoudiniParameterType::String:
		{
			UHoudiniParameterString* StringParam = Cast<UHoudiniParameterString>(InParam);
			if (!IsValid(StringParam))
			{
				return false;
			}

			int32 NumValues = StringParam->GetNumberOfValues();
			if (NumValues <= 0)
			{
				return false;
			}

			for (int32 Idx = 0; Idx < NumValues; Idx++)
			{
				std::string ConvertedString = TCHAR_TO_UTF8(*(StringParam->GetValueAt(Idx)));
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
					FHoudiniEngine::Get().GetSession(),
					StringParam->GetNodeId(), ConvertedString.c_str(), StringParam->GetParmId(), Idx), false);
			}
		}
		break;

		case EHoudiniParameterType::IntChoice:
		{
			UHoudiniParameterChoice* ChoiceParam = Cast<UHoudiniParameterChoice>(InParam);
			if (!IsValid(ChoiceParam))
				return false;

			// Set the parameter's int value.
			const int32 IntValueIndex = ChoiceParam->GetIntValueIndex();
			const int32 IntValue = ChoiceParam->GetIntValue(IntValueIndex);
				
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValues(
				FHoudiniEngine::Get().GetSession(),
				ChoiceParam->GetNodeId(), &IntValue, ChoiceParam->GetValueIndex(), ChoiceParam->GetTupleSize()), false);
		}
		break;
		case EHoudiniParameterType::StringChoice:
		{
			UHoudiniParameterChoice* ChoiceParam = Cast<UHoudiniParameterChoice>(InParam);
			if (!IsValid(ChoiceParam))
			{
				return false;
			}

			if (ChoiceParam->IsStringChoice())
			{
				// Set the parameter's string value.
				std::string ConvertedString = TCHAR_TO_UTF8(*(ChoiceParam->GetStringValue()));
				HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetParmStringValue(
					FHoudiniEngine::Get().GetSession(),
					ChoiceParam->GetNodeId(), ConvertedString.c_str(), ChoiceParam->GetParmId(), 0), false);
			}
			else
			{
				// Set the parameter's int value.
				int32 IntValue = ChoiceParam->GetIntValueIndex();
				HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetParmIntValues(
					FHoudiniEngine::Get().GetSession(),
					ChoiceParam->GetNodeId(), &IntValue, ChoiceParam->GetValueIndex(), ChoiceParam->GetTupleSize()), false);
			}
		}
		break;

		case EHoudiniParameterType::Color:
		{	
			UHoudiniParameterColor* ColorParam = Cast<UHoudiniParameterColor>(InParam);
			if (!IsValid(ColorParam))
				return false;

			bool bHasAlpha = ColorParam->GetTupleSize() == 4 ? true : false;
			FLinearColor Color = ColorParam->GetColorValue();
			
			// Set the color value
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValues(
				FHoudiniEngine::Get().GetSession(),
				ColorParam->GetNodeId(),
				(float*)(&Color.R), ColorParam->GetValueIndex(), bHasAlpha ? 4 : 3), false);
		
		}
		break;

		case EHoudiniParameterType::Button:
		{
			UHoudiniParameterButton* ButtonParam = Cast<UHoudiniParameterButton>(InParam);
			if (!ButtonParam)
				return false;

			TArray<int32> DataArray;
			DataArray.Add(1);

			// Set the button parameter value to 1, (setting button param to any value will call the callback function.)
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValues(
				FHoudiniEngine::Get().GetSession(),
				ButtonParam->GetNodeId(),
				DataArray.GetData(),
				ButtonParam->GetValueIndex(), 1), false);
		}
		break;

		case EHoudiniParameterType::ButtonStrip: 
		{
			UHoudiniParameterButtonStrip* ButtonStripParam = Cast<UHoudiniParameterButtonStrip>(InParam);
			if (!ButtonStripParam)
				return false;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValues(
				FHoudiniEngine::Get().GetSession(),
				ButtonStripParam->GetNodeId(),
				ButtonStripParam->Values.GetData(),
				ButtonStripParam->GetValueIndex(), ButtonStripParam->Count), false);
		}
		break;

		case EHoudiniParameterType::Toggle: 
		{
			UHoudiniParameterToggle* ToggleParam = Cast<UHoudiniParameterToggle>(InParam);
			if (!ToggleParam)
				return false;

			// Set the toggle parameter values.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValues(
				FHoudiniEngine::Get().GetSession(),
				ToggleParam->GetNodeId(),
				ToggleParam->GetValuesPtr(),
				ToggleParam->GetValueIndex(), ToggleParam->GetTupleSize()), false);
		}
		break;

		case EHoudiniParameterType::File:
		case EHoudiniParameterType::FileDir:
		case EHoudiniParameterType::FileGeo:
		case EHoudiniParameterType::FileImage:
		{
			UHoudiniParameterFile* FileParam = Cast<UHoudiniParameterFile>(InParam);

			if (!UploadDirectoryPath(FileParam))
				return false;
		}
		break;

		case EHoudiniParameterType::MultiParm: 
		{
			if (!UploadMultiParmValues(InParam))
				return false;
		}

		break;

		case EHoudiniParameterType::FloatRamp:
		{
			if (!UploadRampParameter(InParam))
				return false;
		}
		break;

		case EHoudiniParameterType::ColorRamp:
		{
			if (!UploadRampParameter(InParam))
				return false;
		}
		break;

		default:
		{
			// TODO: implement other parameter types!
			return false;
		}
		break;
	}

	// The parameter is no longer considered as changed
	InParam->MarkChanged(false);

	return true;
}

bool
FHoudiniParameterTranslator::RevertParameterToDefault(UHoudiniParameter* InParam)
{
	if (!IsValid(InParam))
		return false;

	if (!InParam->IsPendingRevertToDefault())
		return false;

	TArray<int32> TupleToRevert;
	InParam->GetTuplePendingRevertToDefault(TupleToRevert);
	if (TupleToRevert.Num() <= 0)
		return false;

	FString ParameterName = InParam->GetParameterName();

	bool bReverted = true;
	for (auto CurrentIdx : TupleToRevert )
	{
		if (!TupleToRevert.IsValidIndex(CurrentIdx))
		{
			// revert the whole parameter to its default value
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::RevertParmToDefaults(
				FHoudiniEngine::Get().GetSession(),
				InParam->GetNodeId(), TCHAR_TO_UTF8(*ParameterName)))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to revert parameter %s to its default value."), *ParameterName);
				bReverted = false;
			}
		}
		else
		{
			// revert a tuple to its default value
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::RevertParmToDefault(
				FHoudiniEngine::Get().GetSession(),
				InParam->GetNodeId(), TCHAR_TO_UTF8(*ParameterName), CurrentIdx))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to revert parameter %s - %d to its default value."), *ParameterName, CurrentIdx);
				bReverted = false;
			}
		}
	}

	if (!bReverted)
		return false;

	// The parameter no longer needs to be reverted
	InParam->MarkDefault(true);

	return true;
}

EHoudiniFolderParameterType 
FHoudiniParameterTranslator::GetFolderTypeFromParamInfo(const HAPI_ParmInfo* ParamInfo) 
{
	EHoudiniFolderParameterType Type = EHoudiniFolderParameterType::Invalid;

	switch (ParamInfo->scriptType) 
	{
	case HAPI_PrmScriptType::HAPI_PRM_SCRIPT_TYPE_GROUPSIMPLE:
		Type = EHoudiniFolderParameterType::Simple;
		break;

	case HAPI_PrmScriptType::HAPI_PRM_SCRIPT_TYPE_GROUPCOLLAPSIBLE:
		Type = EHoudiniFolderParameterType::Collapsible;
		break;

	case HAPI_PrmScriptType::HAPI_PRM_SCRIPT_TYPE_GROUP:
		Type = EHoudiniFolderParameterType::Tabs;
		break;

	// Treat Radio folders as tabs for now
	case HAPI_PrmScriptType::HAPI_PRM_SCRIPT_TYPE_GROUPRADIO:
		Type = EHoudiniFolderParameterType::Radio;
		break;

	default:
		Type = EHoudiniFolderParameterType::Other;
		break;
	
	}
	
	return Type;

}

bool
FHoudiniParameterTranslator::SyncMultiParmValuesAtLoad(
	UHoudiniParameter* InParam, TArray<UHoudiniParameter*>& OldParams, const int32& InAssetId, const HAPI_AssetInfo& AssetInfo)
{
	UHoudiniParameterMultiParm* MultiParam = Cast<UHoudiniParameterMultiParm>(InParam);
	if (!IsValid(MultiParam))
		return false;

	UHoudiniParameterRampFloat* FloatRampParameter = nullptr;
	UHoudiniParameterRampColor* ColorRampParameter = nullptr;

	if (MultiParam->GetParameterType() == EHoudiniParameterType::FloatRamp)
		FloatRampParameter = Cast<UHoudiniParameterRampFloat>(MultiParam);
	else if (MultiParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
		ColorRampParameter = Cast<UHoudiniParameterRampColor>(MultiParam);

	HAPI_NodeId NodeId = AssetInfo.nodeId;

	int32 Idx = 0;
	int32 InstanceCount = -1;
	HAPI_ParmId ParmId = -1;
	TArray<HAPI_ParmInfo> ParmInfos;
	if (!GetMultiParmInstanceStartIdx(AssetInfo, MultiParam->GetParameterName(), Idx, InstanceCount, ParmId, ParmInfos))
		return false;

	const int32 InstanceCountInUnreal = FMath::Max(MultiParam->GetInstanceCount(), 0);
	if (InstanceCount > InstanceCountInUnreal)
	{
		// The multiparm has more instances on the Houdini side, remove instances from the end until it has the same
		// number as in Unreal.
		// NOTE: Initially this code always removed the first instance. But that causes an issue if HAPI/Houdini does
		//		 not immediately update the parameter names (param1, param2, param3, when param1 is removed, 2 -> 1,
		//		 3 -> 2) so that could result in GetParameters returning parameters with unique IDs, but where the names
		//		 are not up to date, so in the above example, the last param could still be named param3 when it should
		//		 be named param2.
		const int32 Delta = InstanceCount - InstanceCountInUnreal;
		for (int32 n = 0; n < Delta; ++n)
		{
			FHoudiniApi::RemoveMultiparmInstance(
				FHoudiniEngine::Get().GetSession(), NodeId,
				ParmId, MultiParam->InstanceStartOffset + InstanceCount - 1 - n);
		}
	}
	else if (InstanceCountInUnreal > InstanceCount)
	{
		// The multiparm has fewer instances on the Houdini side, add instances at the end until it has the same
		// number as in Unreal.
		// NOTE: Initially this code always inserted before the first instance. But that causes an issue if HAPI/Houdini
		//		 does not immediately update the parameter names (param1, param2, param3, when a param is inserted
		//		 before 1, then 1->2, 2->3, 3->4 so that could result in GetParameters returning parameters with unique
		//		 IDs, but where the names are not up to date, so in the above example, the now second param could still
		//		 be named param1 when it should be named param2.
		const int32 Delta = InstanceCountInUnreal - InstanceCount;
		for (int32 n = 0; n < Delta; ++n)
		{
			FHoudiniApi::InsertMultiparmInstance(
				FHoudiniEngine::Get().GetSession(), NodeId,
				ParmId, MultiParam->InstanceStartOffset + InstanceCount + n);
		}
	}

	// We are going to Sync nested multi-params recursively
	int32 MyParmId = InParam->GetParmId();
	// First, we need to manually look for our index in the old map
	// Since there is a possibility that the parameter interface has changed since our previous cook
	int32 MyIndex = -1;
	for (int32 ParamIdx = 0; ParamIdx < OldParams.Num(); ParamIdx++)
	{
		UHoudiniParameter* CurrentOldParm = OldParams[ParamIdx];
		if (!IsValid(CurrentOldParm))
			continue;

		if (CurrentOldParm->GetParmId() != MyParmId)
			continue;

		// We found ourself, exit now
		MyIndex = ParamIdx;
		break;
	}

	if (MyIndex >= 0)
	{
		// Now Sync nested multi-params recursively
		for (int32 ParamIdx = MyIndex + 1; ParamIdx < OldParams.Num(); ParamIdx++)
		{
			UHoudiniParameter* NextParm = OldParams[ParamIdx];
			if (!IsValid(NextParm))
				continue;

			if (NextParm->GetParentParmId() != MyParmId)
				continue;

			if (NextParm->GetParameterType() != EHoudiniParameterType::MultiParm)
				continue;

			// Always make sure to NOT recurse on ourselves!
			// This could happen if parms have been deleted...
			if (NextParm->GetParmId() == MyParmId)
				continue;

			SyncMultiParmValuesAtLoad(NextParm, OldParams, InAssetId, AssetInfo);
		}
	}

	// The multiparm is a ramp, Get the param infos again, since the number of param instances is changed
	if (!GetMultiParmInstanceStartIdx(AssetInfo, InParam->GetParameterName(), Idx, InstanceCount, ParmId, ParmInfos))
		return false;

	// Step 3:  Set values of the inserted points
	if (FloatRampParameter)
	{
		for (auto & Point : FloatRampParameter->Points)
		{
			// 1: update position float at param Idx
			FHoudiniApi::SetParmFloatValues(
				FHoudiniEngine::Get().GetSession(),
				NodeId, &(Point->Position), ParmInfos[Idx].floatValuesIndex, 1);

			// 2: update float value at param Idx + 1
			// float value
			FHoudiniApi::SetParmFloatValues(
				FHoudiniEngine::Get().GetSession(),
				NodeId, &(Point->Value), ParmInfos[Idx + 1].floatValuesIndex, 1);

			// 3: update interpolation type at param Idx + 2
			int32 IntValue = (int32)(Point->Interpolation);
			FHoudiniApi::SetParmIntValues(
				FHoudiniEngine::Get().GetSession(),
				NodeId, &IntValue, ParmInfos[Idx + 2].intValuesIndex, 1);

			Idx += 3;
		}
	}
	else if (ColorRampParameter)
	{
		for (auto& Point : ColorRampParameter->Points)
		{
			// 1: update position float at param Idx
			FHoudiniApi::SetParmFloatValues(
				FHoudiniEngine::Get().GetSession(),
				NodeId, &(Point->Position), ParmInfos[Idx].floatValuesIndex, 1);

			// 2: update color value at param Idx + 1
			// float value
			FHoudiniApi::SetParmFloatValues(
				FHoudiniEngine::Get().GetSession(),
				NodeId, (float*)(&Point->Value.R), ParmInfos[Idx + 1].floatValuesIndex, 3);

			// 3: update interpolation type at param Idx + 2
			int32 IntValue = (int32)(Point->Interpolation);
			FHoudiniApi::SetParmIntValues(
				FHoudiniEngine::Get().GetSession(),
				NodeId, &IntValue, ParmInfos[Idx + 2].intValuesIndex, 1);

			Idx += 3;
		}
	}

	return true;
}


bool FHoudiniParameterTranslator::UploadRampParameter(UHoudiniParameter* InParam) 
{
	UHoudiniParameterMultiParm* MultiParam = Cast<UHoudiniParameterMultiParm>(InParam);
	if (!IsValid(MultiParam))
		return false;

	UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InParam->GetOuter());
	if (!HoudiniAssetComponent)
		return false;

	int32 InsertIndexStart = -1;
	UHoudiniParameterRampFloat* RampFloatParam = Cast<UHoudiniParameterRampFloat>(InParam);
	UHoudiniParameterRampColor* RampColorParam = Cast<UHoudiniParameterRampColor>(InParam);

	TArray<UHoudiniParameterRampModificationEvent*> *Events = nullptr;
	if (RampFloatParam)
	{
		Events = &(RampFloatParam->ModificationEvents);
		InsertIndexStart = RampFloatParam->GetInstanceCount();
	}
	else if (RampColorParam)
	{
		Events = &(RampColorParam->ModificationEvents);
		InsertIndexStart = RampColorParam->GetInstanceCount();
	}
	else
		return false;

	// Handle All Events
	Events->Sort([](const UHoudiniParameterRampModificationEvent& A, const UHoudiniParameterRampModificationEvent& B) 
	{
		return A.DeleteInstanceIndex > B.DeleteInstanceIndex;
	});
	

	// Step 1:  Handle all delete events first
	for (auto& Event : *Events) 
	{
		if (!Event)
			continue;

		if (!Event->IsDeleteEvent())
			continue;
		
		FHoudiniApi::RemoveMultiparmInstance(
			FHoudiniEngine::Get().GetSession(), MultiParam->GetNodeId(),
			MultiParam->GetParmId(), Event->DeleteInstanceIndex + MultiParam->InstanceStartOffset);

		InsertIndexStart -= 1;
	}

	int32 InsertIndex = InsertIndexStart;


	// Step 2:  Handle all insert events
	for (auto& Event : *Events) 
	{
		if (!Event)
			continue;

		if (!Event->IsInsertEvent())
			continue;
		
		FHoudiniApi::InsertMultiparmInstance(
			FHoudiniEngine::Get().GetSession(), MultiParam->GetNodeId(),
			MultiParam->GetParmId(), InsertIndex + MultiParam->InstanceStartOffset);

		InsertIndex += 1;
	}
	
	// Step 3:  Set inserted parameter values (only if there are instances inserted)
	if (InsertIndex > InsertIndexStart)
	{
		if (HoudiniAssetComponent) 
		{
			// Get the asset's info
			HAPI_AssetInfo AssetInfo;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
				FHoudiniEngine::Get().GetSession(), HoudiniAssetComponent->AssetId, &AssetInfo), false);

			int32 Idx = 0;
			int32 InstanceCount = -1;
			HAPI_ParmId ParmId = -1;
			TArray<HAPI_ParmInfo> ParmInfos;
			if (!FHoudiniParameterTranslator::GetMultiParmInstanceStartIdx(AssetInfo, InParam->GetParameterName(),
				Idx, InstanceCount, ParmId, ParmInfos))
				return false;

			if (InstanceCount < 0)
				return false;

			// Instance count doesn't match, 
			if (InsertIndex != InstanceCount)
				return false;

			// Starting index of parameters which we just inserted
			Idx += 3 * InsertIndexStart;

			if (!ParmInfos.IsValidIndex(Idx + 2))
				return false;

			for (auto & Event : *Events)
			{
				if (!Event)
					continue;
				
				if (!Event->IsInsertEvent())
					continue;

				// 1: update position float at param Idx
				FHoudiniApi::SetParmFloatValues(
					FHoudiniEngine::Get().GetSession(),
					AssetInfo.nodeId, &(Event->InsertPosition), ParmInfos[Idx].floatValuesIndex, 1);

				// step 2: update value at param Idx + 1
				if (Event->IsFloatRampEvent())
				{
					// float value
					FHoudiniApi::SetParmFloatValues(
						FHoudiniEngine::Get().GetSession(),
						AssetInfo.nodeId, &(Event->InsertFloat), ParmInfos[Idx + 1].floatValuesIndex, 1);
				}
				else
				{
					// color value
					FHoudiniApi::SetParmFloatValues(
						FHoudiniEngine::Get().GetSession(),
						AssetInfo.nodeId, (float*)(&Event->InsertColor.R), ParmInfos[Idx + 1].floatValuesIndex, 3);
				}

				// step 3: update interpolation type at param Idx + 2
				int32 IntValue = (int32)(Event->InsertInterpolation);
				FHoudiniApi::SetParmIntValues(
					FHoudiniEngine::Get().GetSession(),
					AssetInfo.nodeId, &IntValue, ParmInfos[Idx + 2].intValuesIndex, 1);
				
				Idx += 3;
			}
		}
	}

	// Step 4: clear all events
	Events->Empty();

	return true;
}

bool FHoudiniParameterTranslator::UploadMultiParmValues(UHoudiniParameter* InParam) 
{
	UHoudiniParameterMultiParm* MultiParam = Cast<UHoudiniParameterMultiParm>(InParam);
	if (!MultiParam)
		return false;

	TArray<EHoudiniMultiParmModificationType> &LastModificationArray = MultiParam->MultiParmInstanceLastModifyArray;

	int32 Size = MultiParam->MultiParmInstanceLastModifyArray.Num();

	for (int32 Index = 0; Index < Size; ++Index)
	{
		if (LastModificationArray[Index] == EHoudiniMultiParmModificationType::Inserted)
		{
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::InsertMultiparmInstance(
				FHoudiniEngine::Get().GetSession(), MultiParam->GetNodeId(),
				MultiParam->GetParmId(), Index + MultiParam->InstanceStartOffset))
				return false;	
			
		}
	}

	for (int32 Index = Size - 1; Index >= 0; --Index)
	{
		if (LastModificationArray[Index] == EHoudiniMultiParmModificationType::Removed)
		{
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::RemoveMultiparmInstance(
				FHoudiniEngine::Get().GetSession(), MultiParam->GetNodeId(),
				MultiParam->GetParmId(), Index + MultiParam->InstanceStartOffset))
				return false;
		}
	}

	// Remove all removal events.
	for (int32 Index = Size - 1; Index >= 0; --Index) 
	{
		if (LastModificationArray[Index] == EHoudiniMultiParmModificationType::Removed)
			LastModificationArray.RemoveAt(Index);
	}
	
	// The last modification array is resized.
	Size = LastModificationArray.Num();

	// Reset the last modification array
	for (int32 Itr =Size - 1; Itr >= 0; --Itr)
	{
		LastModificationArray[Itr] = EHoudiniMultiParmModificationType::None;
	}

	MultiParam->MultiParmInstanceCount = Size;

	return true;
}

bool
FHoudiniParameterTranslator::UploadDirectoryPath(UHoudiniParameterFile* InParam) 
{
	if(!InParam)
		return false;

	for (int32 Index = 0; Index < InParam->GetNumValues(); ++Index)
	{
		std::string ConvertedString = TCHAR_TO_UTF8(*(InParam->GetValueAt(Index)));
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(),
			InParam->GetNodeId(), ConvertedString.c_str(), InParam->GetParmId(), Index), false);
	}

	return true;
}

bool
FHoudiniParameterTranslator::GetMultiParmInstanceStartIdx(const HAPI_AssetInfo& InAssetInfo, const FString InParmName,
	int32& OutStartIdx, int32& OutInstanceCount, HAPI_ParmId& OutParmId, TArray<HAPI_ParmInfo> &OutParmInfos)
{
	// TODO: FIX/IMPROVE THIS!
	// This is bad, that function can be called recursively, fetches all parameters,
	// iterates on them, and fetches their name!! WTF!
	// TODO: Slightly better now, at least we dont fetch every parameter's name!

	// Reset outputs
	OutStartIdx = -1;
	OutInstanceCount = -1;
	OutParmId = -1;
	OutParmInfos.Empty();

	// Try to find the parameter by its name
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(), InAssetInfo.nodeId, TCHAR_TO_UTF8(*InParmName), &OutParmId), false);

	if (OutParmId < 0)
		return false;

	// Get the asset's node info
	HAPI_NodeInfo NodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InAssetInfo.nodeId, &NodeInfo), false);

	// Get all parameters
	OutParmInfos.SetNumUninitialized(NodeInfo.parmCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParameters(
		FHoudiniEngine::Get().GetSession(), InAssetInfo.nodeId, &OutParmInfos[0], 0, NodeInfo.parmCount), false);

	OutStartIdx = 0;
	for (const auto& CurrentParmInfo : OutParmInfos)
	{
		if (OutParmId == CurrentParmInfo.id)
		{
			OutInstanceCount = OutParmInfos[OutStartIdx].instanceCount;

			// Increment, to get the Start index of the ramp children parameters
			OutStartIdx++;
			return true;
		}

		OutStartIdx++;
	}

	// We failed to find the parm
	OutStartIdx = -1;

	return false;
}

bool 
FHoudiniParameterTranslator::RevertRampParameters(TMap<FString, UHoudiniParameter*> & InRampParams, const int32 & AssetId) 
{
	if (InRampParams.Num() <= 0)
		return true;

	// Get the asset's info
	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

	// .. the asset's node info
	HAPI_NodeInfo NodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &NodeInfo), false);

	// Retrieve all the parameter infos.
	TArray< HAPI_ParmInfo > ParmInfos;
	ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParameters(
		FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &ParmInfos[0], 0, NodeInfo.parmCount), false);

	int32 ParamIdx = 0;
	while (ParamIdx < ParmInfos.Num())
	{
		const HAPI_ParmInfo & ParmInfo = ParmInfos[ParamIdx];
		FString ParmName;
		FHoudiniEngineString(ParmInfo.nameSH).ToFString(ParmName);

		if (InRampParams.Contains(ParmName)) 
		{
			if (!InRampParams[ParmName])
			{
				ParamIdx += 1;
				continue;
			}
			
			if (InRampParams[ParmName]->GetParameterType() == EHoudiniParameterType::FloatRamp)
			{
				UHoudiniParameterRampFloat * FloatRamp = Cast<UHoudiniParameterRampFloat>(InRampParams[ParmName]);
				if (!FloatRamp)
				{
					ParamIdx += 1;
					continue;
				}

				if (ParmInfo.instanceCount != FloatRamp->NumDefaultPoints)
				{
					ParamIdx += 1;
					continue;
				}

				for (int32 PtIdx = 0; PtIdx < FloatRamp->NumDefaultPoints; ++PtIdx) 
				{
					const HAPI_ParmInfo & PositionParmInfo = ParmInfos[ParamIdx + 1];
					const HAPI_ParmInfo & ValueParmInfo = ParmInfos[ParamIdx + 2];
					const HAPI_ParmInfo & InterpolationParmInfo = ParmInfos[ParamIdx + 3];

					FHoudiniApi::SetParmFloatValues(
						FHoudiniEngine::Get().GetSession(),
						NodeInfo.id, FloatRamp->DefaultPositions.GetData() + PtIdx, PositionParmInfo.floatValuesIndex, 1);

					FHoudiniApi::SetParmFloatValues(
						FHoudiniEngine::Get().GetSession(),
						NodeInfo.id, FloatRamp->DefaultValues.GetData() + PtIdx, ValueParmInfo.floatValuesIndex, 1);

					FHoudiniApi::SetParmIntValues(
						FHoudiniEngine::Get().GetSession(),
						NodeInfo.id, FloatRamp->DefaultChoices.GetData() + PtIdx, InterpolationParmInfo.intValuesIndex, 1);

					ParamIdx += 3;
				}
			}

			if (InRampParams[ParmName]->GetParameterType() == EHoudiniParameterType::ColorRamp)
			{
				UHoudiniParameterRampColor * ColorRamp = Cast<UHoudiniParameterRampColor>(InRampParams[ParmName]);
				if (!ColorRamp)
				{
					ParamIdx += 1;
					continue;
				}

				if (ParmInfo.instanceCount != ColorRamp->NumDefaultPoints)
				{
					ParamIdx += 1;
					continue;
				}

				for (int32 PtIdx = 0; PtIdx < ColorRamp->NumDefaultPoints; ++PtIdx)
				{
					const HAPI_ParmInfo & PositionParmInfo = ParmInfos[ParamIdx + 1];
					const HAPI_ParmInfo & ValueParmInfo = ParmInfos[ParamIdx + 2];
					const HAPI_ParmInfo & InterpolationParmInfo = ParmInfos[ParamIdx + 3];

					FHoudiniApi::SetParmFloatValues(
						FHoudiniEngine::Get().GetSession(),
						NodeInfo.id, ColorRamp->DefaultPositions.GetData() + PtIdx, PositionParmInfo.floatValuesIndex, 1);

					FHoudiniApi::SetParmFloatValues(
						FHoudiniEngine::Get().GetSession(),
						NodeInfo.id, (float*)(&ColorRamp->DefaultValues[PtIdx].R), ValueParmInfo.floatValuesIndex, 3);

					FHoudiniApi::SetParmIntValues(
						FHoudiniEngine::Get().GetSession(),
						NodeInfo.id, ColorRamp->DefaultChoices.GetData() + PtIdx, InterpolationParmInfo.intValuesIndex, 1);

					ParamIdx += 3;
				}
			}
		}

		ParamIdx += 1;
	}

	return true;
}
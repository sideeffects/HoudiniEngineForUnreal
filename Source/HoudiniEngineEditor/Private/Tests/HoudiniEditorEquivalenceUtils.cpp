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

#include "HoudiniEditorEquivalenceUtils.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniParameter.h"
#include "HoudiniOutput.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"
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
#include "HoudiniParameterOperatorPath.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterSeparator.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniStaticMeshComponent.h"

#include "Containers/Set.h"
#include "Misc/AutomationTest.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodyInstance.h"

// #define WITH_DEV_AUTOMATION_TESTS 0

#define FLOAT_TOLERANCE 0.001F // Mac tests need higher float tolerance

#if WITH_DEV_AUTOMATION_TESTS

FAutomationTestBase* FHoudiniEditorEquivalenceUtils::TestBase = nullptr;
#endif

bool FHoudiniEditorEquivalenceUtils::TestExpressionErrorEnabled = true;

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniAssetComponent * A, const UHoudiniAssetComponent * B)
{
	const FString Header = "UHoudiniAssetComponent";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
	   return true;
	}

	Result &= TestExpressionError(IsEquivalent(A->HoudiniAsset, B->HoudiniAsset), Header, "HoudiniAsset");
	Result &= TestExpressionError(A->bCookOnParameterChange == B->bCookOnParameterChange, Header, "bCookOnParameterChange");
	Result &= TestExpressionError(A->bUploadTransformsToHoudiniEngine == B->bUploadTransformsToHoudiniEngine, Header, "bUploadTransformsToHoudiniEngine");
	Result &= TestExpressionError(A->bCookOnTransformChange == B->bCookOnTransformChange, Header, "bCookOnTransformChange");
	Result &= TestExpressionError(A->bCookOnAssetInputCook == B->bCookOnAssetInputCook, Header, "bCookOnAssetInputCook");
	Result &= TestExpressionError(A->bOutputless == B->bOutputless, Header, "bOutputless");
	Result &= TestExpressionError(A->bOutputTemplateGeos == B->bOutputTemplateGeos, Header, "bOutputTemplateGeos");
	Result &= TestExpressionError(A->bCookOnParameterChange == B->bCookOnParameterChange, Header, "bCookOnParameterChange");
	// We change the temporary cook folder to organize the tests. Do not test this.
	// Result &= TestExpressionError(IsEquivalent(A->TemporaryCookFolder, B->TemporaryCookFolder), Header, "TemporaryCookFolder");
	// Result &= TestExpressionError(IsEquivalent(A->BakeFolder, B->BakeFolder), Header, "BakeFolder");
	Result &= TestExpressionError(IsEquivalent(A->StaticMeshGenerationProperties, B->StaticMeshGenerationProperties), Header, "StaticMeshGenerationProperties");
	Result &= TestExpressionError(IsEquivalent(A->StaticMeshBuildSettings, B->StaticMeshBuildSettings), Header, "StaticMeshBuildSettings");
	Result &= TestExpressionError(A->bOverrideGlobalProxyStaticMeshSettings == B->bOverrideGlobalProxyStaticMeshSettings, Header, "bOverrideGlobalProxyStaticMeshSettings");
	Result &= TestExpressionError(A->bEnableProxyStaticMeshOverride == B->bEnableProxyStaticMeshOverride, Header, "bEnableProxyStaticMeshOverride");
	Result &= TestExpressionError(A->bEnableProxyStaticMeshRefinementByTimerOverride == B->bEnableProxyStaticMeshRefinementByTimerOverride, Header, "bEnableProxyStaticMeshRefinementByTimerOverride");
	Result &= TestExpressionError(A->ProxyMeshAutoRefineTimeoutSecondsOverride == B->ProxyMeshAutoRefineTimeoutSecondsOverride, Header, "ProxyMeshAutoRefineTimeoutSecondsOverride");
	Result &= TestExpressionError(A->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride == B->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride, Header, "bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride");
	Result &= TestExpressionError(A->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride == B->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride, Header, "bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride");

	// Skip UI data
	// #if WITH_EDITORONLY_DATA
	// 	Result &= TestExpressionError(A->bGenerateMenuExpanded == B->bGenerateMenuExpanded, Header, "bGenerateMenuExpanded");
	// 	Result &= TestExpressionError(A->bBakeMenuExpanded == B->bBakeMenuExpanded, Header, "bBakeMenuExpanded");
	// 	Result &= TestExpressionError(A->bAssetOptionMenuExpanded == B->bAssetOptionMenuExpanded, Header, "bAssetOptionMenuExpanded");
	// 	Result &= TestExpressionError(A->bHelpAndDebugMenuExpanded == B->bHelpAndDebugMenuExpanded, Header, "bHelpAndDebugMenuExpanded");
	// 	Result &= TestExpressionError(A->HoudiniEngineBakeOption == B->HoudiniEngineBakeOption, Header, "HoudiniEngineBakeOption");
	// 	Result &= TestExpressionError(A->bRemoveOutputAfterBake == B->bRemoveOutputAfterBake, Header, "bRemoveOutputAfterBake");
	// 	Result &= TestExpressionError(A->bRecenterBakedActors == B->bRecenterBakedActors, Header, "bRecenterBakedActor");
	// 	Result &= TestExpressionError(A->bReplacePreviousBake == B->bReplacePreviousBake, Header, "bReplacePreviousBake");
	// #endif

	// Skip AssetId

	// Note: Despite DownstreamHoudiniAssets being transient, want to test
	Result &= TestExpressionError(A->DownstreamHoudiniAssets.Num() == B->DownstreamHoudiniAssets.Num(), Header, "DownstreamHoudiniAssets.Num");
	for (int i = 0; i < FMath::Min(A->DownstreamHoudiniAssets.Num(), B->DownstreamHoudiniAssets.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->DownstreamHoudiniAssets.Array()[i], B->DownstreamHoudiniAssets.Array()[i]), Header, "DownstreamHoudiniAssets");	
	}

	// Skip ComponentGUID
	// Skip HapiGUID

	// SKip DebugLastAssetState

	// Skip HapiAssetName
	// Skip AssetState
	// Skip DebugLastAssetState
	// Skip AssetStateResult

	// Skip SubAssetIndex
	// Skip AssetCookCount
	// Skip DuplicateTransient cook flags:  bHasBeenLoaded, HasBeenDuplicated, bPendingDelete

	Result &= TestExpressionError(A->Parameters.Num() == B->Parameters.Num(), Header, "Parameters.Num");
	for (int i = 0; i < FMath::Min(A->Parameters.Num(), B->Parameters.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->Parameters[i], B->Parameters[i]), Header, "Parameters");
	}

	Result &= TestExpressionError(A->Inputs.Num() == B->Inputs.Num(), Header, "Inputs.Num");
	for (int i = 0; i < FMath::Min(A->Inputs.Num(), B->Inputs.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->Inputs[i], B->Inputs[i]), Header, "Inputs");
	}

	Result &= TestExpressionError(A->Outputs.Num() == B->Outputs.Num(), Header, "Outputs.Num");
	for (int i = 0; i < FMath::Min(A->Outputs.Num(), B->Outputs.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->Outputs[i], B->Outputs[i]), Header, "Outputs");
	}

	Result &= TestExpressionError(A->BakedOutputs.Num() == B->BakedOutputs.Num(), Header, "BakedOutputs.Num");
	for (int i = 0; i < FMath::Min(A->BakedOutputs.Num(), B->BakedOutputs.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->BakedOutputs[i], B->BakedOutputs[i]), Header, "BakedOutputs");
	}

	// Result &= TestExpressionError(A->UntrackedOutputs.Num() == B->UntrackedOutputs.Num(), Header, "UntrackedOutputs.Num");
	// for (int i = 0; i < FMath::Min(A->UntrackedOutputs.Num(), B->UntrackedOutputs.Num()); i++)
	// {
	// 	Result &= TestExpressionError(IsEquivalent(A->UntrackedOutputs[i].Get(), B->UntrackedOutputs[i].Get()), Header, "UntrackedOutputs");
	// }

	Result &= TestExpressionError(A->HandleComponents.Num() == B->HandleComponents.Num(), Header, "HandleComponents.Num");
	for (int i = 0; i < FMath::Min(A->HandleComponents.Num(), A->HandleComponents.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->HandleComponents[i], B->HandleComponents[i]), Header, "HandleComponents");
	}

	// Skip bHasComponentTransformChanged, bFullyLoaded
	Result &= TestExpressionError(IsEquivalent(A->PDGAssetLink, B->PDGAssetLink), Header, "PDGAssetLink");

	// Not sure if these are necessary
	// Skip refine meshes timer/delegate

	// Skip bNoProxyMeshNextCookRequested

	// Skip bBakeAfterNextCook

	// Skip delegates
	// Skip bCachedIsPreview
	// Skip LastTickTime
	// Skip Version1CompatibilityHAC (Just for simplicity)
	// Skip LastTIckTime
	// Skip remaining delegates

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniAsset* A, const UHoudiniAsset* B)
{
	const FString Header = "UHoudiniAsset";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->AssetFileName.Equals(B->AssetFileName), Header, "AssetFileName");
	// Skip AssetImportData
	// Skip AssetBytes/AssetBytesCount?

	Result &= TestExpressionError(A->bAssetLimitedCommercial == B->bAssetLimitedCommercial, Header, "bAssetLimitedCommercial");
	Result &= TestExpressionError(A->bAssetNonCommercial == B->bAssetNonCommercial, Header, "bAssetLimitedCommercial");
	Result &= TestExpressionError(A->bAssetExpanded == B->bAssetExpanded, Header, "bAssetExpanded");

	return Result;
}



bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameter* A, const UHoudiniParameter* B)
{
	const FString Header = "UHoudiniParameter";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->Name.Equals(B->Name), Header, "Name");
	Result &= TestExpressionError(A->Label.Equals(B->Label), Header, "Label");
	Result &= TestExpressionError(A->ParmType == B->ParmType, Header, "ParmType");
	Result &= TestExpressionError(A->TupleSize == B->TupleSize, Header, "TupleSize");

	// Skip NodeId
	// Skip ParmId
	// Skip ParentParmId

	Result &= TestExpressionError(A->ChildIndex == B->ChildIndex, Header, "ChildIndex");
	Result &= TestExpressionError(A->bIsVisible == B->bIsVisible, Header, "bVisible");
	Result &= TestExpressionError(A->bIsDisabled == B->bIsDisabled, Header, "bIsDisabled");
	// We set bHasChanged on scene change, so don't compare this
	// Result &= TestExpressionError(A->bHasChanged == B->bHasChanged, Header, "bHasChanged");
	// Result &= TestExpressionError(A->bNeedsToTriggerUpdate == B->bNeedsToTriggerUpdate, Header, "bNeedsToTriggerUpdate");
	Result &= TestExpressionError(A->bIsDefault == B->bIsDefault, Header, "bIsDefault");
	Result &= TestExpressionError(A->bIsSpare == B->bIsSpare, Header, "bIsSpare");
	Result &= TestExpressionError(A->bJoinNext == B->bJoinNext, Header, "bJoinNext");
	Result &= TestExpressionError(A->bIsChildOfMultiParm == B->bIsChildOfMultiParm, Header, "bIsChildOfMultiParm");
	Result &= TestExpressionError(A->bIsDirectChildOfMultiParm == B->bIsDirectChildOfMultiParm, Header, "bIsDirectChildOfMultiParm");

	// Skip bPendingRevertToDefault
	// Skip TuplePendingRevertToDefault
	
	Result &= TestExpressionError(A->Help.Equals(B->Help), Header, "Help");
	Result &= TestExpressionError(A->TagCount == B->TagCount, Header, "TagCount");
	//Result &= TestExpressionError(A->ValueIndex == B->ValueIndex, Header, "ValueIndex");
	Result &= TestExpressionError(A->bHasExpression == B->bHasExpression, Header, "bHasExpression");
	Result &= TestExpressionError(A->ParamExpression.Equals(B->ParamExpression), Header, "ParamExpression");
	
	Result &= TestExpressionError(A->Tags.Num() == B->Tags.Num(), Header, "Tags.Num");
	for (auto PairA : A->Tags)
	{
		SetTestExpressionError(false);

		bool HasEquivalent = false;
		for (auto PairB : B->Tags)
		{
			if (PairA.Value.Equals(PairB.Value))
			{
				HasEquivalent = true;
				break;
			}
		}
		SetTestExpressionError(true);
		Result &= TestExpressionError(HasEquivalent, Header, "Tags");
	}

	Result &= TestExpressionError(A->bAutoUpdate == B->bAutoUpdate, Header, "bAutoUpdate");

	if (A->IsA(UHoudiniParameterButton::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterButton>(A), Cast<UHoudiniParameterButton>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterButtonStrip::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterButtonStrip>(A), Cast<UHoudiniParameterButtonStrip>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterChoice::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterChoice>(A), Cast<UHoudiniParameterChoice>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterColor::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterColor>(A), Cast<UHoudiniParameterColor>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterFile::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterFile>(A), Cast<UHoudiniParameterFile>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterFloat::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterFloat>(A), Cast<UHoudiniParameterFloat>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterFolder::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterFolder>(A), Cast<UHoudiniParameterFolder>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterFolderList::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterFolderList>(A), Cast<UHoudiniParameterFolderList>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterInt::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterInt>(A), Cast<UHoudiniParameterInt>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterLabel::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterLabel>(A), Cast<UHoudiniParameterLabel>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterMultiParm::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterMultiParm>(A), Cast<UHoudiniParameterMultiParm>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterOperatorPath::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterOperatorPath>(A), Cast<UHoudiniParameterOperatorPath>(B)), Header, "Object cast");
	/*
	else if (A->IsA(UHoudiniParameterRampFloatPoint::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterRampFloatPoint>(A), Cast<UHoudiniParameterRampFloatPoint>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterRampColorPoint::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterRampColorPoint>(A), Cast<UHoudiniParameterRampColorPoint>(B)), Header, "Object cast");
	*/
	else if (A->IsA(UHoudiniParameterRampFloat::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterRampFloat>(A), Cast<UHoudiniParameterRampFloat>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterRampColor::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterRampColor>(A), Cast<UHoudiniParameterRampColor>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterSeparator::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterSeparator>(A), Cast<UHoudiniParameterSeparator>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterString::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterString>(A), Cast<UHoudiniParameterString>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterToggle::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterToggle>(A), Cast<UHoudiniParameterToggle>(B)), Header, "Object cast");
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniInput* A, const UHoudiniInput* B)
{
	const FString Header = "UHoudiniInput";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->Name.Equals(B->Name), Header, "Name");
	Result &= TestExpressionError(A->Label.Equals(B->Label), Header, "Label");
	Result &= TestExpressionError(A->Type == B->Type, Header, "Type");

	// Skip PreviousType
	// Skip AssetNodeId
	// Skip InputNodeId

	Result &= TestExpressionError(A->InputIndex == B->InputIndex, Header, "InputIndex");
	Result &= TestExpressionError(A->bIsObjectPathParameter == B->bIsObjectPathParameter, Header, "bIsObjectPathParameter");
	// Skip bDataUploadNeeded
	//Result &= TestExpressionError(A->bDataUploadNeeded == B->bDataUploadNeeded, Header, "bDataUploadNeeded");

	const FHoudiniInputObjectSettings& InputSettingsA = A->GetInputSettings();
	const FHoudiniInputObjectSettings& InputSettingsB = B->GetInputSettings();
	
	Result &= TestExpressionError(A->Help.Equals(B->Help), Header, "Help");
	Result &= TestExpressionError(InputSettingsA.KeepWorldTransform == InputSettingsB.KeepWorldTransform, Header, "KeepWorldTransform");
	Result &= TestExpressionError(A->bPackBeforeMerge == B->bPackBeforeMerge, Header, "bPackBeforeMerge");
	Result &= TestExpressionError(InputSettingsA.bImportAsReference == InputSettingsB.bImportAsReference, Header, "bImportAsReference");
	Result &= TestExpressionError(InputSettingsA.bImportAsReferenceRotScaleEnabled == InputSettingsB.bImportAsReferenceRotScaleEnabled, Header, "bImportAsReferenceRotScaleEnabled");
	Result &= TestExpressionError(InputSettingsA.bExportLODs == InputSettingsB.bExportLODs, Header, "bExportLODs");
	Result &= TestExpressionError(InputSettingsA.bExportSockets == InputSettingsB.bExportSockets, Header, "bExportSockets");
	Result &= TestExpressionError(InputSettingsA.bPreferNaniteFallbackMesh == InputSettingsB.bPreferNaniteFallbackMesh, Header, "bPreferNaniteFallbackMesh");
	Result &= TestExpressionError(InputSettingsA.bExportColliders == InputSettingsB.bExportColliders, Header, "bExportColliders");
	// Result &= TestExpressionError(A->bCookOnCurveChanged == B->bCookOnCurveChanged, Header, "bCookOnCurveChanged");

	Result &= TestExpressionError(A->GeometryInputObjects.Num() == B->GeometryInputObjects.Num(), Header, "GeometryInputObjects.Num");
	for (int i = 0; i < FMath::Min(A->GeometryInputObjects.Num(), B->GeometryInputObjects.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->GeometryInputObjects[i], B->GeometryInputObjects[i]), Header, "GeometryInputObjects");
	}

	// Result &= TestExpressionError(A->bStaticMeshChanged == B->bStaticMeshChanged, Header, "bStaticMeshChanged");

	// Skip EDITORONLYDATA (for simplicity)
	Result &= TestExpressionError(A->bInputAssetConnectedInHoudini == B->bInputAssetConnectedInHoudini, Header, "bInputAssetConnectedInHoudini");
	
	Result &= TestExpressionError(A->CurveInputObjects.Num() == B->CurveInputObjects.Num(), Header, "CurveInputObjects.Num");
	for (int i = 0; i < FMath::Min(A->CurveInputObjects.Num(), B->CurveInputObjects.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->CurveInputObjects[i], B->CurveInputObjects[i]), Header, "CurveInputObjects");
	}

	Result &= TestExpressionError(FMath::IsNearlyEqual(A->DefaultCurveOffset, B->DefaultCurveOffset, FLOAT_TOLERANCE), Header, "DefaultCurveOffset");
	
	Result &= TestExpressionError(InputSettingsA.bAddRotAndScaleAttributesOnCurves == InputSettingsB.bAddRotAndScaleAttributesOnCurves, Header, "bAddRotAndScaleAttributeOnCurves");

	Result &= TestExpressionError(InputSettingsA.bUseLegacyInputCurves == InputSettingsB.bUseLegacyInputCurves, Header, "bUseLegacyInputCurves");
	
	// Result &= TestExpressionError(A->bLandscapeHasExportTypeChanged == B->bLandscapeHasExportTypeChanged, Header, "bLandscapeHasExportTypeChanged");

	Result &= TestExpressionError(A->WorldInputObjects.Num() == B->WorldInputObjects.Num(), Header, "WorldInputObjects.Num");
	for (int i = 0; i < FMath::Min(A->WorldInputObjects.Num(), B->WorldInputObjects.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->WorldInputObjects[i], B->WorldInputObjects[i]), Header, "WorldInputObjects");
	}

	Result &= TestExpressionError(A->WorldInputBoundSelectorObjects.Num() == B->WorldInputBoundSelectorObjects.Num(), Header, "WorldInputBoundSelectorObjects.Num");
	for (int i = 0; i < FMath::Min(A->WorldInputBoundSelectorObjects.Num(), B->WorldInputBoundSelectorObjects.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->WorldInputBoundSelectorObjects[i], B->WorldInputBoundSelectorObjects[i]), Header, "WorldInputBoundSelectorObjects");
	}

	Result &= TestExpressionError(A->bIsWorldInputBoundSelector == B->bIsWorldInputBoundSelector, Header, "bIsWorldInputBoundSelector");
	Result &= TestExpressionError(A->bWorldInputBoundSelectorAutoUpdate == B->bWorldInputBoundSelectorAutoUpdate, Header, "bWorldInputBoundSelectorAutoUpdate");
	Result &= TestExpressionError(FMath::IsNearlyEqual(InputSettingsA.UnrealSplineResolution, InputSettingsB.UnrealSplineResolution, FLOAT_TOLERANCE), Header, "UnrealSplineResolution");

	// Skip LastInsertedInputs
	// Skip LastUndoDeletedInputs

	Result &= TestExpressionError(InputSettingsA.LandscapeExportType == InputSettingsB.LandscapeExportType, Header, "LandscapeExportType");
	Result &= TestExpressionError(InputSettingsA.bLandscapeExportSelectionOnly == InputSettingsB.bLandscapeExportSelectionOnly, Header, "bLandscapeExportSelectionOnly");
	Result &= TestExpressionError(InputSettingsA.bLandscapeAutoSelectComponent == InputSettingsB.bLandscapeAutoSelectComponent, Header, "bLandscapeAutoSelectComponent");
	Result &= TestExpressionError(InputSettingsA.bLandscapeExportMaterials == InputSettingsB.bLandscapeExportMaterials, Header, "bLandscapeExportMaterials");
	Result &= TestExpressionError(InputSettingsA.bLandscapeExportLighting == InputSettingsB.bLandscapeExportLighting, Header, "bLandscapeExportLighting");
	Result &= TestExpressionError(InputSettingsA.bLandscapeExportNormalizedUVs == InputSettingsB.bLandscapeExportNormalizedUVs, Header, "bLandscapeExportNormalizedUVs");
	Result &= TestExpressionError(InputSettingsA.bLandscapeExportTileUVs == InputSettingsB.bLandscapeExportTileUVs, Header, "bLandscapeExportTileUVs");
	Result &= TestExpressionError(A->bCanDeleteHoudiniNodes == B->bCanDeleteHoudiniNodes, Header, "bCanDeleteHoudiniNodes");
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniInputObject* A, const UHoudiniInputObject* B)
{
	const FString Header = "UHoudiniInputObject";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(IsEquivalent(A->InputObject.Get(), B->InputObject.Get()), Header, "InputObject");
	Result &= TestExpressionError(IsEquivalent(A->Transform, B->Transform), Header, "Transform");
	Result &= TestExpressionError(A->Type == B->Type, Header, "Type");
	Result &= TestExpressionError(A->GetImportAsReference() == B->GetImportAsReference(), Header, "bImportAsReference");

	// Skip EditorOnly data

	Result &= TestExpressionError(A->bCanDeleteHoudiniNodes == B->bCanDeleteHoudiniNodes, Header, "bCanDeleteHoudiniNodes");
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniOutput* A, const UHoudiniOutput* B)
{
	const FString Header = "UHoudiniOutput";

	bool Result = true;

	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->Type == B->Type, Header, "Type");

	Result &= TestExpressionError(A->HoudiniGeoPartObjects.Num() == B->HoudiniGeoPartObjects.Num(), Header, "HoudiniGeoPartObjects.Num");
	for (int i = 0; i < FMath::Min(A->HoudiniGeoPartObjects.Num(), B->HoudiniGeoPartObjects.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->HoudiniGeoPartObjects[i], B->HoudiniGeoPartObjects[i]), Header, "HoudiniGeoPartObjects");	
	}
	Result &= TestExpressionError(A->OutputObjects.Num() == B->OutputObjects.Num(), Header, "OutputObjects.Num");
	for (auto PairA : A->OutputObjects)
	{
		SetTestExpressionError(false);
		bool HasEquivalent = false;
		for (auto PairB : B->OutputObjects)
		{
			if (IsEquivalent(PairA.Value, PairB.Value))
			{
				HasEquivalent = true;
				break;
			}
		}
		SetTestExpressionError(true);
		Result &= TestExpressionError(HasEquivalent, Header, "OutputObjects");
	}
	Result &= TestExpressionError(A->InstancedOutputs.Num() == B->InstancedOutputs.Num(), Header, "InstancedOutputs.Num");
	for (auto PairA : A->InstancedOutputs)
	{
		SetTestExpressionError(false);

		bool HasEquivalent = false;
		for (auto PairB : B->InstancedOutputs)
		{
			if (IsEquivalent(PairA.Value, PairB.Value))
			{
				HasEquivalent = true;
				break;
			}
		}
		SetTestExpressionError(true);
		Result &= TestExpressionError(HasEquivalent, Header, "InstancedOutputs");
	}
	Result &= TestExpressionError(A->AssignmentMaterialsById.Num() == B->AssignmentMaterialsById.Num(), Header, "AssignmentMaterialsById.Num");
	for (auto PairA : A->AssignmentMaterialsById)
	{
		SetTestExpressionError(false);

		bool HasEquivalent = false;
		for (auto PairB : B->AssignmentMaterialsById)
		{
			if (IsEquivalent(PairA.Value, PairB.Value))
			{
				HasEquivalent = true;
				break;
			}
		}
		SetTestExpressionError(true);
		Result &= TestExpressionError(HasEquivalent, Header, "AssignmentMaterialsById");
	}
	Result &= TestExpressionError(A->ReplacementMaterialsById.Num() == B->ReplacementMaterialsById.Num(), Header, "ReplacementMaterialsById.Num");

	for (auto PairA : A->ReplacementMaterialsById)
	{
		SetTestExpressionError(false);
		bool HasEquivalent = false;
		for (auto PairB : B->ReplacementMaterialsById)
		{
			if (IsEquivalent(PairA.Value, PairB.Value))
			{
				HasEquivalent = true;
				break;
			}
		}
		SetTestExpressionError(true);
		Result &= TestExpressionError(HasEquivalent, Header, "ReplacementMaterialsById");
	}


	// Result &= TestExpressionError(A->StaleCount == B->StaleCount, Header, "StaleCount");
	Result &= TestExpressionError(A->bLandscapeWorldComposition == B->bLandscapeWorldComposition, Header, "bLandscapeWorldComposition");
	Result &= TestExpressionError(A->HoudiniCreatedSocketActors.Num() == B->HoudiniCreatedSocketActors.Num(), Header, "HoudiniCreatedSocketActors.Num");
	for (int i = 0; i < FMath::Min(A->HoudiniCreatedSocketActors.Num(), B->HoudiniCreatedSocketActors.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->HoudiniCreatedSocketActors[i], B->HoudiniCreatedSocketActors[i]), Header, "HoudiniCreatedSocketActors");	
	}
	Result &= TestExpressionError(A->HoudiniAttachedSocketActors.Num() == B->HoudiniAttachedSocketActors.Num(), Header, "HoudiniAttachedSocketActors.Num");
	for (int i = 0; i < FMath::Min(A->HoudiniAttachedSocketActors.Num(), B->HoudiniAttachedSocketActors.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->HoudiniAttachedSocketActors[i], B->HoudiniAttachedSocketActors[i]), Header, "HoudiniAttachedSocketActors");	
	}
	Result &= TestExpressionError(A->bIsEditableNode == B->bIsEditableNode, Header, "bIsEditableNode");
	// Result &= TestExpressionError(A->bIsUpdating == B->bIsUpdating, Header, "bIsUpdating");
	Result &= TestExpressionError(A->bCanDeleteHoudiniNodes == B->bCanDeleteHoudiniNodes, Header, "bCanDeleteHoudiniNodes");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniHandleComponent* A, const UHoudiniHandleComponent* B)
{
	const FString Header = "UHoudiniHandleComponent";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->XformParms.Num() == B->XformParms.Num(), Header, "XformParms.Num");
	for (int i = 0; i < FMath::Min(A->XformParms.Num(), B->XformParms.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->XformParms[i], B->XformParms[i]), Header, "XformParms");	
	}
	Result &= TestExpressionError(IsEquivalent(A->RSTParm, B->RSTParm), Header, "RSTParm");
	Result &= TestExpressionError(IsEquivalent(A->RotOrderParm, B->RotOrderParm), Header, "RotOrderParm");
	Result &= TestExpressionError(A->HandleType == B->HandleType, Header, "HandleType");
	Result &= TestExpressionError(A->HandleName.Equals(B->HandleName), Header, "HandleName");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniPDGAssetLink* A, const UHoudiniPDGAssetLink* B)
{
	const FString Header = "UHoudiniPDGAssetLink";
	bool Result = true;

	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}
	
	Result &= TestExpressionError(A->AllTOPNetworks.Num() == B->AllTOPNetworks.Num(), Header, "AllTOPNetworks.Num");
	for (int i = 0; i < FMath::Min(A->AllTOPNetworks.Num(), B->AllTOPNetworks.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->AllTOPNetworks[i], B->AllTOPNetworks[i]), Header, "AllTOPNetworks");	
	}
	
	Result &= TestExpressionError(A->SelectedTOPNetworkIndex == B->SelectedTOPNetworkIndex, Header, "SelectedTOPNetworkIndex");
	Result &= TestExpressionError(A->bAutoCook == B->bAutoCook, Header, "bAutoCook");
	Result &= TestExpressionError(A->bUseTOPNodeFilter == B->bUseTOPNodeFilter, Header, "bUseTOPNodeFilter");
	Result &= TestExpressionError(A->bUseTOPOutputFilter == B->bUseTOPOutputFilter, Header, "bUseTOPOutputFilter");
	Result &= TestExpressionError(A->TOPNodeFilter.Equals(B->TOPNodeFilter), Header, "TOPNodeFilter");
	Result &= TestExpressionError(A->TOPOutputFilter.Equals(B->TOPOutputFilter), Header, "TOPOutputFilter");
	Result &= TestExpressionError(A->NumWorkItems == B->NumWorkItems, Header, "NumWorkItems");
	Result &= TestExpressionError(A->OutputCachePath.Equals(B->OutputCachePath), Header, "OutputCachePath");
	Result &= TestExpressionError(IsEquivalent(A->OutputParentActor, B->OutputParentActor), Header, "OutputParentActor");
	Result &= TestExpressionError(IsEquivalent(A->BakeFolder, B->BakeFolder), Header, "BakeFolder");

	// Skip editorOnly data

	return Result;
}

// Houdini struct equivalence functions ===

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniStaticMeshGenerationProperties& A,
	const FHoudiniStaticMeshGenerationProperties& B)
{
	const FString Header = "FHoudiniStaticMeshGenerationProperties";
	bool Result = true;
	
	Result &= TestExpressionError(A.bGeneratedDoubleSidedGeometry == B.bGeneratedDoubleSidedGeometry, Header, "bGeneratedDoubleSidedGeometry");
	Result &= TestExpressionError(IsEquivalent(A.GeneratedPhysMaterial, B.GeneratedPhysMaterial), Header, "GeneratedPhysMaterial");
	Result &= TestExpressionError(IsEquivalent(A.DefaultBodyInstance, B.DefaultBodyInstance), Header, "DefaultBodyInstance");
	Result &= TestExpressionError(A.GeneratedCollisionTraceFlag == B.GeneratedCollisionTraceFlag, Header, "GeneratedCollisionTraceFlag");
	Result &= TestExpressionError(A.GeneratedLightMapResolution == B.GeneratedLightMapResolution, Header, "GeneratedLightMapResolution");
	//Result &= TestExpressionError(FMath::IsNearlyEqual(A.GeneratedLpvBiasMultiplier, B.GeneratedLpvBiasMultiplier), Header, "GeneratedLpvBiasMultiplier");
	Result &= TestExpressionError(IsEquivalent(A.GeneratedWalkableSlopeOverride, B.GeneratedWalkableSlopeOverride), Header, "GeneratedWalkableSlopOverride");
	Result &= TestExpressionError(A.GeneratedLightMapCoordinateIndex == B.GeneratedLightMapCoordinateIndex, Header, "GeneratedLightMapCoordinateIndex");
	Result &= TestExpressionError(A.bGeneratedUseMaximumStreamingTexelRatio == B.bGeneratedUseMaximumStreamingTexelRatio, Header, "bGeneratedUseMaximumStreamingTexelRatio");
	Result &= TestExpressionError(A.GeneratedFoliageDefaultSettings == B.GeneratedFoliageDefaultSettings, Header, "GeneratedFoliageDefaultSettings");

	Result &= TestExpressionError(A.GeneratedAssetUserData.Num() == B.GeneratedAssetUserData.Num(), Header, "GeneratedAssetUserData.Num");
	for (int i = 0; i < FMath::Min(A.GeneratedAssetUserData.Num(), B.GeneratedAssetUserData.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A.GeneratedAssetUserData[i], B.GeneratedAssetUserData[i]), Header, "GeneratedAssetUserData");
	}
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniBakedOutput& A, const FHoudiniBakedOutput& B)
{
	const FString Header = "FHoudiniBakedOutput";
	bool Result = true;

	Result &= TestExpressionError(A.BakedOutputObjects.Num() == B.BakedOutputObjects.Num(), Header, "BakedOutputObjects.Num");

	for (auto PairA : A.BakedOutputObjects)
	{
		SetTestExpressionError(false);
		bool HasEquivalent = false;
		for (auto PairB : B.BakedOutputObjects)
		{
			if (IsEquivalent(PairA.Value, PairB.Value))
			{
				HasEquivalent = true;
				break;
			}
		}
		SetTestExpressionError(true);
		Result &= TestExpressionError(HasEquivalent, Header, "BakedOutputObjects");
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniGeoPartObject& A, const FHoudiniGeoPartObject& B)
{
	const FString Header = "FHoudiniGeoPartObject";
	bool Result = true;

	// Skip AssetId, ObjectId, AssetName, ObjectName, GeoId
	Result &= TestExpressionError(A.PartId == B.PartId, Header, "PartId");
	Result &= TestExpressionError(A.PartName.Equals(B.PartName), Header, "PartName");
	Result &= TestExpressionError(A.bHasCustomPartName == B.bHasCustomPartName, Header, "bHasCustomPartName");
	Result &= TestExpressionError(A.SplitGroups.Num() == B.SplitGroups.Num(), Header, "SplitGroups.Num");
	for (int i = 0; i < FMath::Min(A.SplitGroups.Num(), B.SplitGroups.Num()); i++)
	{
		Result &= TestExpressionError(A.SplitGroups[i].Equals(B.SplitGroups[i]), Header, "SplitGroups");	
	}
	Result &= TestExpressionError(IsEquivalent(A.TransformMatrix, B.TransformMatrix), Header, "TransformMatrix");
	// Result &= TestExpressionError(A.NodePath.Equals(B.NodePath), Header, "NodePath");
	Result &= TestExpressionError(A.Type == B.Type, Header, "Type");
	Result &= TestExpressionError(A.InstancerType == B.InstancerType, Header, "InstancerType");
	Result &= TestExpressionError(A.VolumeName.Equals(B.VolumeName), Header, "VolumeName");
	Result &= TestExpressionError(A.VolumeTileIndex == B.VolumeTileIndex, Header, "VolumeTileIndex");
	Result &= TestExpressionError(A.bIsVisible == B.bIsVisible, Header, "bIsVisible");
	Result &= TestExpressionError(A.bIsEditable == B.bIsEditable, Header, "bIsEditable");
	Result &= TestExpressionError(A.bIsTemplated == B.bIsTemplated, Header, "bIsTemplated");
	Result &= TestExpressionError(A.bIsInstanced == B.bIsInstanced, Header, "bIsInstanced");
	//Result &= TestExpressionError(A.bHasGeoChanged == B.bHasGeoChanged, Header, "bHasGeoChanged");
	//Result &= TestExpressionError(A.bHasPartChanged == B.bHasPartChanged, Header, "bHasPartChanged");
	//Result &= TestExpressionError(A.bHasTransformChanged == B.bHasTransformChanged, Header, "bHasTransformChanged");
	//Result &= TestExpressionError(A.bHasMaterialsChanged == B.bHasMaterialsChanged, Header, "bHasMaterialsChanged");
	Result &= TestExpressionError(A.bLoaded == B.bLoaded, Header, "bLoaded");
	//Result &= TestExpressionError(IsEquivalent(A.ObjectInfo, B.ObjectInfo), Header, "ObjectInfo");
	//Result &= TestExpressionError(IsEquivalent(A.GeoInfo, B.GeoInfo), Header, "GeoInfo");
	//Result &= TestExpressionError(IsEquivalent(A.PartInfo, B.PartInfo), Header, "PartInfo");
	//Result &= TestExpressionError(IsEquivalent(A.VolumeInfo, B.VolumeInfo), Header, "VolumeInfo");
	//Result &= TestExpressionError(IsEquivalent(A.CurveInfo, B.CurveInfo), Header, "CurveInfo");
	Result &= TestExpressionError(A.AllMeshSockets.Num() == B.AllMeshSockets.Num(), Header, "AllMeshSockets.Num");
	for (int i = 0; i < FMath::Min(A.AllMeshSockets.Num(), B.AllMeshSockets.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A.AllMeshSockets[i], B.AllMeshSockets[i]), Header, "AllMeshSockets");	
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniInstancedOutput& A, const FHoudiniInstancedOutput& B)
{
	const FString Header = "FHoudiniInstancedOutput";
	bool Result = true;

	//Result &= TestExpressionError(IsEquivalent(A.OriginalObject.Get(), B.OriginalObject.Get()), Header, "OriginalObject");
	Result &= TestExpressionError(A.OriginalObjectIndex == B.OriginalObjectIndex, Header, "OriginalObjectIndex");
	Result &= TestExpressionError(A.OriginalTransforms.Num() == B.OriginalTransforms.Num(), Header, "OriginalTransforms.Num");
	for (int i = 0; i < FMath::Min(A.OriginalTransforms.Num(), B.OriginalTransforms.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A.OriginalTransforms[i], B.OriginalTransforms[i]), Header, "OriginalTransforms");	
	}
	Result &= TestExpressionError(A.VariationObjects.Num() == B.VariationObjects.Num(), Header, "VariationObjects.Num");
	for (int i = 0; i < FMath::Min(A.VariationObjects.Num(), B.VariationObjects.Num()); i++)
	{
	//	Result &= TestExpressionError(IsEquivalent(A.VariationObjects[i].Get(), B.VariationObjects[i].Get()), Header, "VariationObjects");	
	}
	Result &= TestExpressionError(A.VariationTransformOffsets.Num() == B.VariationTransformOffsets.Num(), Header, "VariationTransformOffsets.Num");
	for (int i = 0; i < FMath::Min(A.VariationTransformOffsets.Num(), B.VariationTransformOffsets.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A.VariationTransformOffsets[i], B.VariationTransformOffsets[i]), Header, "VariationTransformOffsets");	
	}
	Result &= TestExpressionError(A.TransformVariationIndices.Num() == B.TransformVariationIndices.Num(), Header, "TransformVariationIndices.Num");
	for (int i = 0; i < FMath::Min(A.TransformVariationIndices.Num(), B.TransformVariationIndices.Num()); i++)
	{
		Result &= TestExpressionError(A.TransformVariationIndices[i] == B.TransformVariationIndices[i], Header, "TransformVariationIndices");	
	}
	// Result &= TestExpressionError(A.bChanged == B.bChanged, Header, "bChanged");
	// Result &= TestExpressionError(A.bStale == B.bStale, Header, "bStale");

	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniBakedOutputObject& A, const FHoudiniBakedOutputObject& B)
{
	const FString Header = "FHoudiniBakedOutputObject";
	bool Result = true;
	
	// Result &= TestExpressionError(A.Actor.Equals(B.Actor), Header, "Actor");
	// Result &= TestExpressionError(A.Blueprint.Equals(B.Blueprint), Header, "Blueprint");
	Result &= TestExpressionError(A.ActorBakeName.IsEqual(B.ActorBakeName), Header, "ActorBakeName");
	Result &= TestExpressionError(A.BakedObject.Equals(B.BakedObject), Header, "BakedObject");
	Result &= TestExpressionError(A.BakedComponent.Equals(B.BakedComponent), Header, "BakedComponent");
	Result &= TestExpressionError(A.InstancedActors.Num() == B.InstancedActors.Num(), Header, "InstancedActors.Num");
	for (int i = 0; i < FMath::Min(A.InstancedActors.Num(), B.InstancedActors.Num()); i++)
	{
		Result &= TestExpressionError(A.InstancedActors[i].Equals(B.InstancedActors[i]), Header, "InstancedActors");	
	}
	Result &= TestExpressionError(A.InstancedComponents.Num() == B.InstancedComponents.Num(), Header, "InstancedComponents.Num");
	for (int i = 0; i < FMath::Min(A.InstancedComponents.Num(), B.InstancedComponents.Num()); i++)
	{
		Result &= TestExpressionError(A.InstancedComponents[i].Equals(B.InstancedComponents[i]), Header, "InstancedComponents");	
	}
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniObjectInfo& A, const FHoudiniObjectInfo& B)
{
	const FString Header = "FHoudiniObjectInfo";
	bool Result = true;

	// Skip Name
	// Result &= TestExpressionError(A.bHasTransformChanged == B.bHasTransformChanged, Header, "bHasTransformChanged");
	// Result &= TestExpressionError(A.bHaveGeosChanged == B.bHaveGeosChanged, Header, "bHaveGeosChanged");
	Result &= TestExpressionError(A.bIsVisible == B.bIsVisible, Header, "bIsVisible");
	Result &= TestExpressionError(A.bIsInstancer == B.bIsInstancer, Header, "bIsInstancer");
	Result &= TestExpressionError(A.bIsInstanced == B.bIsInstanced, Header, "bIsInstanced");
	Result &= TestExpressionError(A.GeoCount == B.GeoCount, Header, "GeoCount");
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniGeoInfo& A, const FHoudiniGeoInfo& B)
{
	const FString Header = "FHoudiniGeoInfo";
	bool Result = true;
	
	Result &= TestExpressionError(A.Type == B.Type, Header, "Type");
	Result &= TestExpressionError(A.Name.Equals(B.Name), Header, "Name");
	Result &= TestExpressionError(A.bIsEditable == B.bIsEditable, Header, "bIsEditable");
	Result &= TestExpressionError(A.bIsTemplated == B.bIsTemplated, Header, "bIsTemplated");
	Result &= TestExpressionError(A.bIsDisplayGeo == B.bIsDisplayGeo, Header, "bIsDisplayGeo");
	// Result &= TestExpressionError(A.bHasGeoChanged == B.bHasGeoChanged, Header, "bHasGeoChanged");
	// Result &= TestExpressionError(A.bHasMaterialChanged == B.bHasMaterialChanged, Header, "bHasMaterialChanged");
	Result &= TestExpressionError(A.PartCount == B.PartCount, Header, "PartCount");
	Result &= TestExpressionError(A.PointGroupCount == B.PointGroupCount, Header, "PointGroupCount");
	Result &= TestExpressionError(A.PrimitiveGroupCount == B.PrimitiveGroupCount, Header, "PrimitiveGroupCount");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniPartInfo& A, const FHoudiniPartInfo& B)
{
	const FString Header = "FHoudiniPartInfo";
	bool Result = true;

	Result &= TestExpressionError(A.PartId == B.PartId, Header, "PartId");
	Result &= TestExpressionError(A.Name.Equals(B.Name), Header, "Name");
	Result &= TestExpressionError(A.Type == B.Type, Header, "Type");
	Result &= TestExpressionError(A.FaceCount == B.FaceCount, Header, "FaceCount");
	Result &= TestExpressionError(A.VertexCount == B.VertexCount, Header, "VertexCount");
	Result &= TestExpressionError(A.PointCount == B.PointCount, Header, "PointCount");
	Result &= TestExpressionError(A.PointAttributeCounts == B.PointAttributeCounts, Header, "PointAttributeCounts");
	Result &= TestExpressionError(A.VertexAttributeCounts == B.VertexAttributeCounts, Header, "VertexAttributeCounts");
	Result &= TestExpressionError(A.PrimitiveAttributeCounts == B.PrimitiveAttributeCounts, Header, "PrimitiveAttributeCounts");
	Result &= TestExpressionError(A.DetailAttributeCounts == B.DetailAttributeCounts, Header, "DetailAttributeCounts");
	Result &= TestExpressionError(A.bIsInstanced == B.bIsInstanced, Header, "bIsInstanced");
	Result &= TestExpressionError(A.InstancedPartCount == B.InstancedPartCount, Header, "InstancedPartCount");
	Result &= TestExpressionError(A.InstanceCount == B.InstanceCount, Header, "InstanceCount");
	// Result &= TestExpressionError(A.bHasChanged == B.bHasChanged, Header, "bHasChanged");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniVolumeInfo& A, const FHoudiniVolumeInfo& B)
{
	const FString Header = "FHoudiniVolumeInfo";
	bool Result = true;
	
	Result &= TestExpressionError(A.Name.Equals(B.Name), Header, "Name");
	Result &= TestExpressionError(A.bIsVDB == B.bIsVDB, Header, "bIsVDB");
	Result &= TestExpressionError(A.TupleSize == B.TupleSize, Header, "TupleSize");
	Result &= TestExpressionError(A.bIsFloat == B.bIsFloat, Header, "bIsFloat");
	Result &= TestExpressionError(A.TileSize == B.TileSize, Header, "TileSize");
	Result &= TestExpressionError(IsEquivalent(A.Transform, B.Transform), Header, "Transform");
	Result &= TestExpressionError(A.bHasTaper == B.bHasTaper, Header, "bHasTaper");
	Result &= TestExpressionError(A.XLength == B.XLength, Header, "XLength");
	Result &= TestExpressionError(A.YLength == B.YLength, Header, "YLength");
	Result &= TestExpressionError(A.ZLength == B.ZLength, Header, "ZLength");
	Result &= TestExpressionError(A.MinX == B.MinX, Header, "MinX");
	Result &= TestExpressionError(A.MinY == B.MinY, Header, "MinY");
	Result &= TestExpressionError(A.MinZ == B.MinZ, Header, "MinZ");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.XTaper, B.XTaper, FLOAT_TOLERANCE), Header, "XTaper");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.YTaper, B.YTaper, FLOAT_TOLERANCE), Header, "YTaper");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniCurveInfo& A, const FHoudiniCurveInfo& B)
{
	const FString Header = "FHoudiniCurveInfo";
	bool Result = true;
	
	Result &= TestExpressionError(A.Type == B.Type, Header, "Type");
	Result &= TestExpressionError(A.CurveCount == B.CurveCount, Header, "CurveCount");
	Result &= TestExpressionError(A.VertexCount == B.VertexCount, Header, "VertexCount");
	Result &= TestExpressionError(A.KnotCount == B.KnotCount, Header, "KnotCount");
	Result &= TestExpressionError(A.bIsPeriodic == B.bIsPeriodic, Header, "bIsPeriodic");
	Result &= TestExpressionError(A.bIsRational == B.bIsRational, Header, "bIsRational");
	Result &= TestExpressionError(A.Order == B.Order, Header, "Order");
	Result &= TestExpressionError(A.bHasKnots == B.bHasKnots, Header, "bHasKnots");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniMeshSocket& A, const FHoudiniMeshSocket& B)
{
	const FString Header = "FHoudiniMeshSocket";
	bool Result = true;
	
	Result &= TestExpressionError(A == B, Header, "Equals");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniOutputObject& A, const FHoudiniOutputObject& B)
{
	const FString Header = "FHoudiniOutputObject";
	bool Result = true;

	Result &= TestExpressionError(IsEquivalent(A.OutputObject, B.OutputObject), Header, "OutputObject");

	if (A.OutputComponents.Num() != B.OutputComponents.Num())
	    return false;

	for(int Index = 0; Index < A.OutputComponents.Num(); Index++)
	{
	    Result &= TestExpressionError(IsEquivalent(A.OutputComponents[Index], B.OutputComponents[Index]), Header, "OutputComponent");
	}

	// Proxies can't be saved, so comparing them will be difficult (saving refines the proxies to SM)
	// Result &= TestExpressionError(IsEquivalent(A.ProxyObject, B.ProxyObject), Header, "ProxyObject");
	// Result &= TestExpressionError(IsEquivalent(A.ProxyComponent, B.ProxyComponent), Header, "ProxyComponent");
	Result &= TestExpressionError(A.bProxyIsCurrent == B.bProxyIsCurrent, Header, "bProxyIsCurrent");
	Result &= TestExpressionError(A.bIsImplicit == B.bIsImplicit, Header, "bIsImplicit");
	Result &= TestExpressionError(A.BakeName.Equals(B.BakeName), Header, "BakeName");
	Result &= TestExpressionError(IsEquivalent(A.CurveOutputProperty, B.CurveOutputProperty), Header, "CurveOutputProperty");
	// Comparing CachedAttributes/CachedTokens will be specific to certain advanced tests
	// Result &= TestExpressionError(A.CachedAttributes.Num() == B.CachedAttributes.Num(), Header, "CachedAttributes.Num");
	// for (auto PairA : A.CachedAttributes)
	// {
	// 	SetTestExpressionError(false);
	// 	bool HasEquivalent = false;
	// 	for (auto PairB : B.CachedAttributes)
	// 	{
	// 		if (PairA.Value.Equals(PairB.Value))
	// 		{
	// 			HasEquivalent = true;
	// 			break;
	// 		}
	// 	}
	// 	SetTestExpressionError(true);
	// 	Result &= TestExpressionError(HasEquivalent, Header, "CachedAttributes");
	// }
	// Skip CachedTokens as they can be different.
	//Result &= TestExpressionError(A.CachedTokens.Num() == B.CachedTokens.Num(), Header, "CachedTokens.Num");
//
	//for (auto PairA : A.CachedTokens)
	//{
	//	SetTestExpressionError(false);
	//	bool HasEquivalent = false;
	//	for (auto PairB : B.CachedTokens)
	//	{
	//		if (PairA.Value.Equals(PairB.Value))
	//		{
	//			HasEquivalent = true;
	//			break;
	//		}
	//	}
	//	SetTestExpressionError(true);
	//	Result &= TestExpressionError(HasEquivalent, Header, "CachedTokens");
	//}


	return Result;
}


// Unreal Equivalence functions ===
bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FDirectoryPath & A, const FDirectoryPath & B)
{
	const FString Header = "FDirectoryPath";
	bool Result = true;
	
	Result &= TestExpressionError(A.Path.Equals(B.Path), Header, "Path");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FMeshBuildSettings& A, const FMeshBuildSettings& B)
{
	const FString Header = "FMeshBuildSettings";
	bool Result = true;

	// Skip everything (for simplicity)
	Result &= TestExpressionError(A == B, Header, "Equality");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FTransform& A, const FTransform& B)
{
	const FString Header = "FTransform";
	bool Result = true;

	Result &= TestExpressionError(IsEquivalent(A.GetTranslation(), B.GetTranslation()), Header, "Translation");
	Result &= TestExpressionError(IsEquivalent(A.GetRotation(), B.GetRotation()), Header, "Rotation");
	Result &= TestExpressionError(IsEquivalent(A.GetScale3D(), B.GetScale3D()), Header, "Scale3D");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FBodyInstance& A, const FBodyInstance& B)
{
	const FString Header = "FBodyInstance";
	bool Result = true;

	// Nothing interesting to test
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FWalkableSlopeOverride& A, const FWalkableSlopeOverride& B)
{
	const FString Header = "FWalkableSlopeOverride";
	bool Result = true;

	// Nothing interesting to test
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FHoudiniCurveOutputProperties& A, const FHoudiniCurveOutputProperties& B)
{
	const FString Header = "FHoudiniCurveOutputProperties";
	bool Result = true;

	Result &= TestExpressionError(A.CurveOutputType == B.CurveOutputType, Header, "CurveOutputType");
	Result &= TestExpressionError(A.NumPoints == B.NumPoints, Header, "NumPoints");
	Result &= TestExpressionError(A.bClosed == B.bClosed, Header, "bClosed");
	Result &= TestExpressionError(A.CurveType == B.CurveType, Header, "CurveType");
	Result &= TestExpressionError(A.CurveMethod == B.CurveMethod, Header, "CurveMethod");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FColor& A, const FColor& B)
{
	const FString Header = "FColor";
	bool Result = true;
	
	Result &= TestExpressionError(A.R == B.R, Header, "R");
	Result &= TestExpressionError(A.G == B.G, Header, "G");
	Result &= TestExpressionError(A.B == B.B, Header, "B");
	Result &= TestExpressionError(A.A == B.A, Header, "A");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FLinearColor& A, const FLinearColor& B)
{
	const FString Header = "FLinearColor";
	bool Result = true;
	
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.R, B.R, FLOAT_TOLERANCE), Header, "R");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.G, B.G, FLOAT_TOLERANCE), Header, "G");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.B, B.B, FLOAT_TOLERANCE), Header, "B");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.A, B.A, FLOAT_TOLERANCE), Header, "A");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FVector& A, const FVector& B)
{
	const FString Header = "FVector";
	bool Result = true;

	Result &= TestExpressionError(FMath::IsNearlyEqual(A.X, B.X, FLOAT_TOLERANCE), Header, "X");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.Y, B.Y, FLOAT_TOLERANCE), Header, "Y");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.Z, B.Z, FLOAT_TOLERANCE), Header, "Z");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FQuat& A, const FQuat& B)
{
	const FString Header = "FQuat";
	
	bool Result = true;
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.X, B.X, FLOAT_TOLERANCE), Header, "X");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.Y, B.Y, FLOAT_TOLERANCE), Header, "Y");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.Z, B.Z, FLOAT_TOLERANCE), Header, "Z");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.W, B.W, FLOAT_TOLERANCE), Header, "W");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FIntVector& A, const FIntVector& B)
{
	const FString Header = "FIntVector";
	bool Result = true;

	Result &= TestExpressionError(A.X == B.X, Header, "X");
	Result &= TestExpressionError(A.Y == B.Y, Header, "Y");
	Result &= TestExpressionError(A.Z == B.Z, Header, "Z");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FVector2D& A, const FVector2D& B)
{
	const FString Header = "FVector2D";
	bool Result = true;

	Result &= TestExpressionError(FMath::IsNearlyEqual(A.X, B.X, FLOAT_TOLERANCE), Header, "X");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.Y, B.Y, FLOAT_TOLERANCE), Header, "Y");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FVector2f& A, const FVector2f& B)
{
	const FString Header = "FVector2f";
	bool Result = true;

	Result &= TestExpressionError(FMath::IsNearlyEqual(A.X, B.X, FLOAT_TOLERANCE), Header, "X");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A.Y, B.Y, FLOAT_TOLERANCE), Header, "Y");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FStaticMaterial& A, const FStaticMaterial& B)
{
	const FString Header = "FStaticMaterial";
	bool Result = true;

	Result &= TestExpressionError(A == B, Header, "Equality");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const FBox& A, const FBox& B)
{
	const FString Header = "FBox";
	bool Result = true;

	Result &= TestExpressionError(IsEquivalent(A.Min, B.Min), Header, "Min");
	Result &= TestExpressionError(IsEquivalent(A.Max, B.Max), Header, "Max");
	Result &= TestExpressionError(A.IsValid == B.IsValid, Header, "IsValid");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UObject* A, const UObject* B)
{
	const FString Header = "UObject";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	// Compare class
	Result &= TestExpressionError(A->StaticClass() == B->StaticClass(), Header, "StaticClass");

	// Test all possible Runtime UObjects in Houdini
	// NOTE: MAKE SURE TO IMPLEMENT THE FUNCTIONS OTHERWISE INFINITE RECURSION WILL OCCUR

	// Misc
	if (A->IsA(UHoudiniAsset::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniAsset>(A), Cast<UHoudiniAsset>(B)), Header, "Object cast");
	else if (A->IsA(AHoudiniAssetActor::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<AHoudiniAssetActor>(A), Cast<AHoudiniAssetActor>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniHandleComponent::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniHandleComponent>(A), Cast<UHoudiniHandleComponent>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniInput::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniInput>(A), Cast<UHoudiniInput>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniInputObject::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniInputObject>(A), Cast<UHoudiniInputObject>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniInstancedActorComponent::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniInstancedActorComponent>(A), Cast<UHoudiniInstancedActorComponent>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniMeshSplitInstancerComponent::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniMeshSplitInstancerComponent>(A), Cast<UHoudiniMeshSplitInstancerComponent>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniOutput::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniOutput>(A), Cast<UHoudiniOutput>(B)), Header, "Object cast");
	// Parameters
	else if (A->IsA(UHoudiniParameterButton::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterButton>(A), Cast<UHoudiniParameterButton>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterButtonStrip::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterButtonStrip>(A), Cast<UHoudiniParameterButtonStrip>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterChoice::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterChoice>(A), Cast<UHoudiniParameterChoice>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterColor::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterColor>(A), Cast<UHoudiniParameterColor>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterFile::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterFile>(A), Cast<UHoudiniParameterFile>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterFloat::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterFloat>(A), Cast<UHoudiniParameterFloat>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterFolder::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterFolder>(A), Cast<UHoudiniParameterFolder>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterFolderList::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterFolderList>(A), Cast<UHoudiniParameterFolderList>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterInt::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterInt>(A), Cast<UHoudiniParameterInt>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterLabel::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterLabel>(A), Cast<UHoudiniParameterLabel>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterMultiParm::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterMultiParm>(A), Cast<UHoudiniParameterMultiParm>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterOperatorPath::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterOperatorPath>(A), Cast<UHoudiniParameterOperatorPath>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterRampFloatPoint::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterRampFloatPoint>(A), Cast<UHoudiniParameterRampFloatPoint>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterRampColorPoint::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterRampColorPoint>(A), Cast<UHoudiniParameterRampColorPoint>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterRampFloat::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterRampFloat>(A), Cast<UHoudiniParameterRampFloat>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterRampColor::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterRampColor>(A), Cast<UHoudiniParameterRampColor>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterSeparator::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterSeparator>(A), Cast<UHoudiniParameterSeparator>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterString::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterString>(A), Cast<UHoudiniParameterString>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniParameterToggle::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniParameterToggle>(A), Cast<UHoudiniParameterToggle>(B)), Header, "Object cast");
	// Outputs
	else if (A->IsA(UHoudiniSplineComponent::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniSplineComponent>(A), Cast<UHoudiniSplineComponent>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniStaticMesh::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniStaticMesh>(A), Cast<UHoudiniStaticMesh>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniStaticMeshComponent::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniStaticMeshComponent>(A), Cast<UHoudiniStaticMeshComponent>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniLandscapePtr::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniLandscapePtr>(A), Cast<UHoudiniLandscapePtr>(B)), Header, "Object cast");
	else if (A->IsA(UHoudiniLandscapeTargetLayerOutput::StaticClass()))
		Result &= TestExpressionError(IsEquivalent(Cast<UHoudiniLandscapeTargetLayerOutput>(A), Cast<UHoudiniLandscapeTargetLayerOutput>(B)), Header, "Object cast");
		
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UPhysicalMaterial * A, const UPhysicalMaterial * B)
{
	const FString Header = "UPhysicalMaterial";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->GetName().Equals(B->GetName()), Header, "Name");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const AActor* A, const AActor* B)
{
	const FString Header = "AActor";

	bool Result = true;
	
	Result &= TestExpressionError((A != nullptr) == (B != nullptr), Header, "Null check");

	if (A == nullptr || B == nullptr)
	{
		return true;
	}

	// Compare the actors class
	Result &= TestExpressionError(IsEquivalent(A->GetClass(), B->GetClass()), Header, "Cast");

	// Just compare UObject
	Result &= TestExpressionError(IsEquivalent(Cast<UObject>(A), Cast<UObject>(B)), Header, "Cast");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const ALandscapeProxy* A, const ALandscapeProxy* B)
{
	const FString Header = "ALandscapeProxy";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	// TODO: Not sure what's important to test here

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const AHoudiniAssetActor* A, const AHoudiniAssetActor* B)
{
	const FString Header = "AHoudiniAssetActor";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(IsEquivalent(A->HoudiniAssetComponent, B->HoudiniAssetComponent), Header, "HoudiniAssetComponent");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniInstancedActorComponent* A,
	const UHoudiniInstancedActorComponent* B)
{
	const FString Header = "UHoudiniInstancedActorComponent";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(IsEquivalent(A->InstancedObject, B->InstancedObject), Header, "InstancedObject");

	Result &= TestExpressionError(A->InstancedActors.Num() == B->InstancedActors.Num(), Header, "InstancedActors.Num");
	for (int i = 0; i < FMath::Min(A->InstancedActors.Num(), B->InstancedActors.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->InstancedActors[i], B->InstancedActors[i]), Header, "InstancedActors");
	}
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniMeshSplitInstancerComponent* A,
	const UHoudiniMeshSplitInstancerComponent* B)
{
	const FString Header = "UHoudiniMeshSplitInstancerComponent";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->Instances.Num() == B->Instances.Num(), Header, "Instances.Num");
	for (int i = 0; i < FMath::Min(A->Instances.Num(), B->Instances.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->Instances[i], B->Instances[i]), Header, "Instances");
	}
	Result &= TestExpressionError(A->OverrideMaterials.Num() == B->OverrideMaterials.Num(), Header, "OverrideMaterials.Num");
	for (int i = 0; i < FMath::Min(A->OverrideMaterials.Num(), B->OverrideMaterials.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->OverrideMaterials[i], B->OverrideMaterials[i]), Header, "OverrideMaterials");
	}
	Result &= TestExpressionError(IsEquivalent(A->InstancedMesh, B->InstancedMesh), Header, "InstancedMesh");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterButton* A, const UHoudiniParameterButton* B)
{
	const FString Header = "UHoudiniParameterButton";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterButtonStrip* A, const UHoudiniParameterButtonStrip* B)
{
	const FString Header = "UHoudiniParameterButtonStrip";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->GetNumValues() == B->GetNumValues(), Header, "NumValues");
	for (uint32 i = 0; i < FMath::Min(A->GetNumValues(), B->GetNumValues()); i++)
	{
		const FString* LabelA = A->GetStringLabelAt(i);
		const FString* LabelB = B->GetStringLabelAt(i);

		if (!LabelA || !LabelB)
		{
			Result = false;
			continue;
		}

		Result &= TestExpressionError(LabelA->Equals(*LabelB), Header, "Labels");
	}

	for (uint32 i = 0; i < FMath::Min(A->GetNumValues(), B->GetNumValues()); i++)
	{
		Result &= TestExpressionError(A->GetValueAt(i) == B->GetValueAt(i), Header, "Values");
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterChoice* A, const UHoudiniParameterChoice* B)
{
	const FString Header = "UHoudiniParameterChoice";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->IntValue == B->IntValue, Header, "IntValue");
	Result &= TestExpressionError(A->DefaultIntValue == B->DefaultIntValue, Header, "DefaultIntValue");
	Result &= TestExpressionError(A->StringValue.Equals(B->StringValue), Header, "StringValue");
	Result &= TestExpressionError(A->DefaultStringValue.Equals(B->DefaultStringValue), Header, "DefaultStringValue");
	Result &= TestExpressionError(A->StringChoiceValues.Num() == B->StringChoiceValues.Num(), Header, "StringChoiceValues.Num");
	for (int i = 0; i < FMath::Min(A->StringChoiceValues.Num(), B->StringChoiceValues.Num()); i++)
	{
		Result &= TestExpressionError(A->StringChoiceValues[i].Equals(B->StringChoiceValues[i]), Header, "StringChoiceValues");
	}
	Result &= TestExpressionError(A->StringChoiceLabels.Num() == B->StringChoiceLabels.Num(), Header, "StringChoiceLabels.Num");
	for (int i = 0; i < FMath::Min(A->StringChoiceLabels.Num(), B->StringChoiceLabels.Num()); i++)
	{
		Result &= TestExpressionError(A->StringChoiceLabels[i].Equals(B->StringChoiceLabels[i]), Header, "StringChoiceLabels");
	}
	// Result &= TestExpressionError(A->ChoiceLabelsPtr.Num() == B->ChoiceLabelsPtr.Num(), Header, "ChoiceLabelsPtr.Num");
	// for (int i = 0; i < A->ChoiceLabelsPtr.Num(); i++)
	// {
	// 	Result &= TestExpressionError(A->ChoiceLabelsPtr[i]->Equals(B->ChoiceLabelsPtr[i])), Header, "ChoiceLabelsPtr");
	// }
	Result &= TestExpressionError(A->bIsChildOfRamp == B->bIsChildOfRamp, Header, "bIsChildOfRamp");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterColor* A, const UHoudiniParameterColor* B)
{
	const FString Header = "UHoudiniParameterColor";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(IsEquivalent(A->Color, B->Color), Header, "Color");
	Result &= TestExpressionError(IsEquivalent(A->DefaultColor, B->DefaultColor), Header, "DefaultColor");
	Result &= TestExpressionError(A->bIsChildOfRamp == B->bIsChildOfRamp, Header, "bIsChildOfRamp");
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterFile* A, const UHoudiniParameterFile* B)
{
	const FString Header = "UHoudiniParameterColor";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->Values.Num() == B->Values.Num(), Header, "Values.Num");
	for (int i = 0; i < FMath::Min(A->Values.Num(), B->Values.Num()); i++)
	{
		Result &= TestExpressionError(A->Values[i].Equals(B->Values[i]), Header, "Values");
	}
	Result &= TestExpressionError(A->DefaultValues.Num() == B->DefaultValues.Num(), Header, "DefaultValues.Num");
	for (int i = 0; i < FMath::Min(A->DefaultValues.Num(), B->DefaultValues.Num()); i++)
	{
		Result &= TestExpressionError(A->DefaultValues[i].Equals(B->DefaultValues[i]), Header, "DefaultValues");
	}
	Result &= TestExpressionError(A->Filters.Equals(B->Filters), Header, "Filters");
	Result &= TestExpressionError(A->bIsReadOnly == B->bIsReadOnly, Header, "bIsReadOnly");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterFloat* A, const UHoudiniParameterFloat* B)
{
	const FString Header = "UHoudiniParameterFloat";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->Values.Num() == B->Values.Num(), Header, "Values.Num");
	for (int i = 0; i < FMath::Min(A->Values.Num(), B->Values.Num()); i++)
	{
		Result &= TestExpressionError(FMath::IsNearlyEqual(A->Values[i], B->Values[i], FLOAT_TOLERANCE), Header, "Values");
	}
	Result &= TestExpressionError(A->DefaultValues.Num() == B->DefaultValues.Num(), Header, "DefaultValues.Num");
	for (int i = 0; i < FMath::Min(A->DefaultValues.Num(), B->DefaultValues.Num()); i++)
	{
		Result &= TestExpressionError(FMath::IsNearlyEqual(A->DefaultValues[i], B->DefaultValues[i]), Header, "DefaultValues");
	}
	Result &= TestExpressionError(A->Unit.Equals(B->Unit), Header, "Unit");
	Result &= TestExpressionError(A->bNoSwap == B->bNoSwap, Header, "bNoSwap");
	Result &= TestExpressionError(A->bHasMin == B->bHasMin, Header, "bHasMin");
	Result &= TestExpressionError(A->bHasMax == B->bHasMax, Header, "bHasMax");
	Result &= TestExpressionError(A->bHasUIMin == B->bHasUIMin, Header, "bHasUIMin");
	Result &= TestExpressionError(A->bHasUIMax == B->bHasUIMax, Header, "bHasUIMax");
	Result &= TestExpressionError(A->bIsLogarithmic == B->bIsLogarithmic, Header, "bIsLogarithmic");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A->Min, B->Min, FLOAT_TOLERANCE), Header, "Min");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A->Max, B->Max, FLOAT_TOLERANCE), Header, "Max");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A->UIMin, B->UIMin, FLOAT_TOLERANCE), Header, "UIMin");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A->UIMax, B->UIMax, FLOAT_TOLERANCE), Header, "UIMax");
	Result &= TestExpressionError(A->bIsChildOfRamp == B->bIsChildOfRamp, Header, "bIsChildOfRamp");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterFolder* A, const UHoudiniParameterFolder* B)
{
	const FString Header = "UHoudiniParameterColor";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->FolderType == B->FolderType, Header, "FolderType");
	Result &= TestExpressionError(A->bExpanded == B->bExpanded, Header, "bExpanded");
	Result &= TestExpressionError(A->bChosen == B->bChosen, Header, "bChosen");
	Result &= TestExpressionError(A->ChildCounter == B->ChildCounter, Header, "ChildCounter");
	Result &= TestExpressionError(A->bIsContentShown == B->bIsContentShown, Header, "bIsContentShown");
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterFolderList* A, const UHoudiniParameterFolderList* B)
{
	const FString Header = "UHoudiniParameterFolderList";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->bIsTabMenu == B->bIsTabMenu, Header, "bIsTabMenu");
	Result &= TestExpressionError(A->bIsTabsShown == B->bIsTabsShown, Header, "bIsTabsShown");
	Result &= TestExpressionError(A->TabFolders.Num() == B->TabFolders.Num(), Header, "TabFolders.Num");
	for (int i = 0; i < FMath::Min(A->TabFolders.Num(), B->TabFolders.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->TabFolders[i], B->TabFolders[i]), Header, "TabFolders");
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterInt* A, const UHoudiniParameterInt* B)
{
	const FString Header = "UHoudiniParameterInt";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->Values.Num() == B->Values.Num(), Header, "Values.Num");
	for (int i = 0; i < FMath::Min(A->Values.Num(), B->Values.Num()); i++)
	{
		Result &= TestExpressionError(A->Values[i] == B->Values[i], Header, "Values");
	}
	Result &= TestExpressionError(A->DefaultValues.Num() == B->DefaultValues.Num(), Header, "DefaultValues.Num");
	for (int i = 0; i < FMath::Min(A->DefaultValues.Num(), B->DefaultValues.Num()); i++)
	{
		Result &= TestExpressionError(A->DefaultValues[i] == B->DefaultValues[i], Header, "DefaultValues");
	}
	Result &= TestExpressionError(A->Unit.Equals(B->Unit), Header, "Unit");
	Result &= TestExpressionError(A->bHasMin == B->bHasMin, Header, "bHasMin");
	Result &= TestExpressionError(A->bHasMax == B->bHasMax, Header, "bHasMax");
	Result &= TestExpressionError(A->bHasUIMin == B->bHasUIMin, Header, "bHasUIMin");
	Result &= TestExpressionError(A->bHasUIMax == B->bHasUIMax, Header, "bHasUIMax");
	Result &= TestExpressionError(A->bIsLogarithmic == B->bIsLogarithmic, Header, "bIsLogarithmic");
	Result &= TestExpressionError(A->Min == B->Min, Header, "Min");
	Result &= TestExpressionError(A->Max == B->Max, Header, "Max");
	Result &= TestExpressionError(A->UIMin == B->UIMin, Header, "UIMin");
	Result &= TestExpressionError(A->UIMax == B->UIMax, Header, "UIMax");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterLabel* A, const UHoudiniParameterLabel* B)
{
	const FString Header = "UHoudiniParameterLabel";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->LabelStrings.Num() == B->LabelStrings.Num(), Header, "LabelStrings.Num");
	for (int i = 0; i < FMath::Min(A->LabelStrings.Num(), B->LabelStrings.Num()); i++)
	{
		Result &= TestExpressionError(A->LabelStrings[i].Equals(B->LabelStrings[i]), Header, "LabelStrings");
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterMultiParm* A, const UHoudiniParameterMultiParm* B)
{
	const FString Header = "UHoudiniParameterMultiParm";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->bIsShown == B->bIsShown, Header, "bIsShown");
	Result &= TestExpressionError(A->Value == B->Value, Header, "Value");
	Result &= TestExpressionError(A->TemplateName.Equals(B->TemplateName), Header, "TemplateName");
	Result &= TestExpressionError(A->MultiparmValue == B->MultiparmValue, Header, "MultiparmValue");
	Result &= TestExpressionError(A->MultiParmInstanceNum == B->MultiParmInstanceNum, Header, "MultiParmInstanceNum");
	Result &= TestExpressionError(A->MultiParmInstanceLength == B->MultiParmInstanceLength, Header, "MultiParmInstanceLength");
	Result &= TestExpressionError(A->MultiParmInstanceCount == B->MultiParmInstanceCount, Header, "MultiParmInstanceCount");
	Result &= TestExpressionError(A->InstanceStartOffset == B->InstanceStartOffset, Header, "InstanceStartOffset");
	Result &= TestExpressionError(A->MultiParmInstanceLastModifyArray.Num() == B->MultiParmInstanceLastModifyArray.Num(), Header, "MultiParmInstanceLastModifyArray.Num");
	for (int i = 0; i < FMath::Min(A->MultiParmInstanceLastModifyArray.Num(), B->MultiParmInstanceLastModifyArray.Num()); i++)
	{
		Result &= TestExpressionError(A->MultiParmInstanceLastModifyArray[i] == B->MultiParmInstanceLastModifyArray[i], Header, "MultiParmInstanceLastModifyArray");
	}
	Result &= TestExpressionError(A->DefaultInstanceCount == B->DefaultInstanceCount, Header, "DefaultInstanceCount");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterOperatorPath* A, const UHoudiniParameterOperatorPath* B)
{
	const FString Header = "UHoudiniParameterOperatorPath";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	if (A->HoudiniInput != nullptr && B->HoudiniInput != nullptr)
	{
		Result &= TestExpressionError(IsEquivalent(A->HoudiniInput.Get(), B->HoudiniInput.Get()), Header, "UHoudiniParameter");
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterRampFloatPoint* A,
	const UHoudiniParameterRampFloatPoint* B)
{
	const FString Header = "UHoudiniParameterRampFloatPoint";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(FMath::IsNearlyEqual(A->Position, B->Position, FLOAT_TOLERANCE), Header, "Position");
	Result &= TestExpressionError(FMath::IsNearlyEqual(A->Value, B->Value, FLOAT_TOLERANCE), Header, "Value");
	Result &= TestExpressionError(A->Interpolation == B->Interpolation, Header, "Interpolation");
	Result &= TestExpressionError(A->InstanceIndex == B->InstanceIndex, Header, "InstanceIndex");
	Result &= TestExpressionError(IsEquivalent(A->PositionParentParm, B->PositionParentParm), Header, "PositionParentParm");
	Result &= TestExpressionError(IsEquivalent(A->ValueParentParm, B->ValueParentParm), Header, "ValueParentParm");
	Result &= TestExpressionError(IsEquivalent(A->InterpolationParentParm, B->InterpolationParentParm), Header, "InterpolationParentParm");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterRampColorPoint* A,
	const UHoudiniParameterRampColorPoint* B)
{
	const FString Header = "UHoudiniParameterRampColorPoint";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(FMath::IsNearlyEqual(A->Position, B->Position, FLOAT_TOLERANCE), Header, "Position");
	Result &= TestExpressionError(IsEquivalent(A->Value, B->Value), Header, "Value");
	Result &= TestExpressionError(A->Interpolation == B->Interpolation, Header, "Interpolation");
	Result &= TestExpressionError(A->InstanceIndex == B->InstanceIndex, Header, "InstanceIndex");
	Result &= TestExpressionError(IsEquivalent(A->PositionParentParm, B->PositionParentParm), Header, "PositionParentParm");
	Result &= TestExpressionError(IsEquivalent(A->ValueParentParm, B->ValueParentParm), Header, "ValueParentParm");
	Result &= TestExpressionError(IsEquivalent(A->InterpolationParentParm, B->InterpolationParentParm), Header, "InterpolationParentParm");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterRampFloat* A, const UHoudiniParameterRampFloat* B)
{
	const FString Header = "UHoudiniParameterRampFloat";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterRampColor* A, const UHoudiniParameterRampColor* B)
{
	const FString Header = "UHoudiniParameterRampFloat";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->Points.Num() == B->Points.Num(), Header, "Points.Num");
	for (int i = 0; i < FMath::Min(A->Points.Num(), B->Points.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->Points[i], B->Points[i]), Header, "Points");
	}
	Result &= TestExpressionError(A->CachedPoints.Num() == B->CachedPoints.Num(), Header, "CachedPoints.Num");
	for (int i = 0; i < FMath::Min(A->CachedPoints.Num(), B->CachedPoints.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->CachedPoints[i], B->CachedPoints[i]), Header, "CachedPoints");
	}
	Result &= TestExpressionError(A->DefaultPositions.Num() == B->DefaultPositions.Num(), Header, "DefaultPositions.Num");
	for (int i = 0; i < FMath::Min(A->DefaultPositions.Num(), B->DefaultPositions.Num()); i++)
	{
		Result &= TestExpressionError(FMath::IsNearlyEqual(A->DefaultPositions[i], B->DefaultPositions[i], FLOAT_TOLERANCE), Header, "DefaultPositions");
	}
	Result &= TestExpressionError(A->DefaultValues.Num() == B->DefaultValues.Num(), Header, "DefaultValues.Num");
	for (int i = 0; i < FMath::Min(A->DefaultValues.Num(), B->DefaultValues.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->DefaultValues[i], B->DefaultValues[i]), Header, "DefaultValues");
	}
	Result &= TestExpressionError(A->DefaultChoices.Num() == B->DefaultChoices.Num(), Header, "DefaultChoices.Num");
	for (int i = 0; i < FMath::Min(A->DefaultChoices.Num(), B->DefaultChoices.Num()); i++)
	{
		Result &= TestExpressionError(A->DefaultChoices[i] == B->DefaultChoices[i], Header, "DefaultChoices");
	}
	Result &= TestExpressionError(A->NumDefaultPoints == B->NumDefaultPoints, Header, "NumDefaultPoints");
	Result &= TestExpressionError(A->bCaching == B->bCaching, Header, "bCaching");

	// Skip ModificationEvents

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterSeparator* A, const UHoudiniParameterSeparator* B)
{
	const FString Header = "UHoudiniParameterSeparator";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterString* A, const UHoudiniParameterString* B)
{
	const FString Header = "UHoudiniParameterSeparator";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}
	
	Result &= TestExpressionError(A->Values.Num() == B->Values.Num(), Header, "Values.Num");
	for (int i = 0; i < FMath::Min(A->Values.Num(), B->Values.Num()); i++)
	{
		Result &= TestExpressionError(A->Values[i].Equals(B->Values[i]), Header, "Values");
	}
	Result &= TestExpressionError(A->DefaultValues.Num() == B->DefaultValues.Num(), Header, "DefaultValues.Num");
	for (int i = 0; i < FMath::Min(A->DefaultValues.Num(), B->DefaultValues.Num()); i++)
	{
		Result &= TestExpressionError(A->DefaultValues[i].Equals(B->DefaultValues[i]), Header, "DefaultValues");
	}
	Result &= TestExpressionError(A->ChosenAssets.Num() == B->ChosenAssets.Num(), Header, "ChosenAssets.Num");
	for (int i = 0; i < FMath::Min(A->ChosenAssets.Num(), B->ChosenAssets.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->ChosenAssets[i], B->ChosenAssets[i]), Header, "ChosenAssets");
	}
	Result &= TestExpressionError(A->bIsAssetRef == B->bIsAssetRef, Header, "bIsAssetRef");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniParameterToggle* A, const UHoudiniParameterToggle* B)
{
	const FString Header = "UHoudiniParameterToggle";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}
	
	Result &= TestExpressionError(A->Values.Num() == B->Values.Num(), Header, "Values.Num");
	for (int i = 0; i < FMath::Min(A->Values.Num(), B->Values.Num()); i++)
	{
		Result &= TestExpressionError(A->Values[i] == B->Values[i], Header, "Values");
	}
	Result &= TestExpressionError(A->DefaultValues.Num() == B->DefaultValues.Num(), Header, "DefaultValues.Num");
	for (int i = 0; i < FMath::Min(A->DefaultValues.Num(), B->DefaultValues.Num()); i++)
	{
		Result &= TestExpressionError(A->DefaultValues[i] == B->DefaultValues[i], Header, "DefaultValues");
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniSplineComponent* A, const UHoudiniSplineComponent* B)
{
	const FString Header = "UHoudiniSplineComponent";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->CurvePoints.Num() == B->CurvePoints.Num(), Header, "CurvePoints.Num");
	for (int i = 0; i < FMath::Min(A->CurvePoints.Num(), B->CurvePoints.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->CurvePoints[i], B->CurvePoints[i]), Header, "CurvePoints");
	}
	Result &= TestExpressionError(A->DisplayPoints.Num() == B->DisplayPoints.Num(), Header, "DisplayPoints.Num");
	for (int i = 0; i < FMath::Min(A->DisplayPoints.Num(), B->DisplayPoints.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->DisplayPoints[i], B->DisplayPoints[i]), Header, "DisplayPoints");
	}
	Result &= TestExpressionError(A->DisplayPointIndexDivider.Num() == B->DisplayPointIndexDivider.Num(), Header, "DisplayPointIndexDivider.Num");
	for (int i = 0; i < FMath::Min(A->DisplayPointIndexDivider.Num(), B->DisplayPointIndexDivider.Num()); i++)
	{
		Result &= TestExpressionError(A->DisplayPointIndexDivider[i] == B->DisplayPointIndexDivider[i], Header, "DisplayPointIndexDivider");
	}
	Result &= TestExpressionError(A->HoudiniSplineName.Equals(B->HoudiniSplineName), Header, "HoudiniSplineName");
	Result &= TestExpressionError(A->bClosed == B->bClosed, Header, "bClosed");
	Result &= TestExpressionError(A->bReversed == B->bReversed, Header, "bReversed");
	Result &= TestExpressionError(A->bIsHoudiniSplineVisible == B->bIsHoudiniSplineVisible, Header, "bIsHoudiniSplineVisible");
	Result &= TestExpressionError(A->CurveType == B->CurveType, Header, "CurveType");
	Result &= TestExpressionError(A->CurveMethod == B->CurveMethod, Header, "CurveMethod");
	Result &= TestExpressionError(A->bIsOutputCurve == B->bIsOutputCurve, Header, "bIsOutputCurve");
	Result &= TestExpressionError(A->bCookOnCurveChanged == B->bCookOnCurveChanged, Header, "bCookOnCurveChanged");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniStaticMesh* A, const UHoudiniStaticMesh* B)
{
	const FString Header = "UHoudiniStaticMesh";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->bHasNormals == B->bHasNormals, Header, "bHasNormals");
	Result &= TestExpressionError(A->bHasTangents == B->bHasTangents, Header, "bHasTangents");
	Result &= TestExpressionError(A->bHasColors == B->bHasColors, Header, "bHasColors");
	Result &= TestExpressionError(A->NumUVLayers == B->NumUVLayers, Header, "NumUVLayers");
	Result &= TestExpressionError(A->bHasPerFaceMaterials == B->bHasPerFaceMaterials, Header, "bHasPerFaceMaterials");
	Result &= TestExpressionError(A->VertexPositions.Num() == B->VertexPositions.Num(), Header, "VertexPositions.Num");
	for (int i = 0; i < FMath::Min(A->VertexPositions.Num(), B->VertexPositions.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->VertexPositions[i], B->VertexPositions[i]), Header, "VertexPositions");
	}
	Result &= TestExpressionError(A->TriangleIndices.Num() == B->TriangleIndices.Num(), Header, "TriangleIndices.Num");
	for (int i = 0; i < FMath::Min(A->TriangleIndices.Num(), B->TriangleIndices.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->TriangleIndices[i], B->TriangleIndices[i]), Header, "TriangleIndices");
	}
	Result &= TestExpressionError(A->VertexInstanceColors.Num() == B->VertexInstanceColors.Num(), Header, "VertexInstanceColors.Num");
	for (int i = 0; i < FMath::Min(A->VertexInstanceColors.Num(), B->VertexInstanceColors.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->VertexInstanceColors[i], B->VertexInstanceColors[i]), Header, "VertexInstanceColors");
	}
	Result &= TestExpressionError(A->VertexInstanceNormals.Num() == B->VertexInstanceNormals.Num(), Header, "VertexInstanceNormals.Num");
	for (int i = 0; i < FMath::Min(A->VertexInstanceNormals.Num(), B->VertexInstanceNormals.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->VertexInstanceNormals[i], B->VertexInstanceNormals[i]), Header, "VertexInstanceNormals");
	}
	Result &= TestExpressionError(A->VertexInstanceUTangents.Num() == B->VertexInstanceUTangents.Num(), Header, "VertexInstanceUTangents.Num");
	for (int i = 0; i < FMath::Min(A->VertexInstanceUTangents.Num(), B->VertexInstanceUTangents.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->VertexInstanceUTangents[i], B->VertexInstanceUTangents[i]), Header, "VertexInstanceUTangents");
	}
	Result &= TestExpressionError(A->VertexInstanceVTangents.Num() == B->VertexInstanceVTangents.Num(), Header, "VertexInstanceVTangents.Num");
	for (int i = 0; i < FMath::Min(A->VertexInstanceVTangents.Num(), B->VertexInstanceVTangents.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->VertexInstanceVTangents[i], B->VertexInstanceVTangents[i]), Header, "VertexInstanceVTangents");
	}
	Result &= TestExpressionError(A->VertexInstanceUVs.Num() == B->VertexInstanceUVs.Num(), Header, "VertexInstanceUVs.Num");
	for (int i = 0; i < FMath::Min(A->VertexInstanceUVs.Num(), B->VertexInstanceUVs.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->VertexInstanceUVs[i], B->VertexInstanceUVs[i]), Header, "VertexInstanceUVs");
	}
	Result &= TestExpressionError(A->MaterialIDsPerTriangle.Num() == B->MaterialIDsPerTriangle.Num(), Header, "MaterialIDsPerTriangle.Num");
	for (int i = 0; i < FMath::Min(A->MaterialIDsPerTriangle.Num(), B->MaterialIDsPerTriangle.Num()); i++)
	{
		Result &= TestExpressionError(A->MaterialIDsPerTriangle[i] == B->MaterialIDsPerTriangle[i], Header, "MaterialIDsPerTriangle");
	}
	Result &= TestExpressionError(A->StaticMaterials.Num() == B->StaticMaterials.Num(), Header, "StaticMaterials.Num");
	for (int i = 0; i < FMath::Min(A->StaticMaterials.Num(), B->StaticMaterials.Num()); i++)
	{
		Result &= TestExpressionError(IsEquivalent(A->StaticMaterials[i], B->StaticMaterials[i]), Header, "StaticMaterials");
	}

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniStaticMeshComponent* A, const UHoudiniStaticMeshComponent* B)
{
	const FString Header = "UHoudiniStaticMeshComponent";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(IsEquivalent(A->Mesh, B->Mesh), Header, "Mesh");
	Result &= TestExpressionError(IsEquivalent(A->LocalBounds, B->LocalBounds), Header, "LocalBounds");
	Result &= TestExpressionError(A->bHoudiniIconVisible == B->bHoudiniIconVisible, Header, "bHoudiniIconVisible");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniLandscapePtr* A, const UHoudiniLandscapePtr* B)
{
	const FString Header = "UHoudiniLandscapePtr";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(IsEquivalent(A->LandscapeSoftPtr.Get(), B->LandscapeSoftPtr.Get()), Header, "LandscapeSoftPtr");
	Result &= TestExpressionError(A->BakeType == B->BakeType, Header, "BakeType");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UHoudiniLandscapeTargetLayerOutput* A, const UHoudiniLandscapeTargetLayerOutput* B)
{
	const FString Header = "UHoudiniLandscapeTargetLayerOutput";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	// TODO: Add here


	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UStaticMeshComponent* A, const UStaticMeshComponent* B)
{
	const FString Header = "UStaticMeshComponent";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(IsEquivalent(A->GetStaticMesh(), B->GetStaticMesh()), Header, "StaticMesh");

	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UMaterialInterface* A, const UMaterialInterface* B)
{
	const FString Header = "UMaterialInterface";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	// Result &= TestExpressionError(A->GetHeight() == B->GetHeight(), Header, "Height");
	// Result &= TestExpressionError(A->GetWidth() == B->GetWidth(), Header, "Width");
	// TODO: Not sure what to test here.
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::IsEquivalent(const UStaticMesh* A, const UStaticMesh* B)
{
	const FString Header = "UStaticMesh";

	bool Result = true;
	
	Result &= TestExpressionError((IsValid(A)) == (IsValid(B)), Header, "Null check");

	if (!IsValid(A) || !IsValid(B))
	{
		return true;
	}

	Result &= TestExpressionError(A->GetNumLODs() == B->GetNumLODs(), Header, "LODs");
	for (int i = 0; i < FMath::Min(A->GetNumLODs(), B->GetNumLODs()); i++)
	{
		Result &= TestExpressionError(A->GetNumVertices(i) == B->GetNumVertices(i), Header, "Vertices");
	}

	// TODO: Actually check the vertices
	
	return Result;
}

bool FHoudiniEditorEquivalenceUtils::TestExpressionError(const bool Expression, const FString &Header, const FString &Subject)
{
	if (!Expression)
	{

		if (TestExpressionErrorEnabled == false)
		{
			// Andy: Comment out this message, it really spams the logs.
			//const FString OutputStr = FString::Printf(TEXT("%s: %s is not equivalent, but may just be a TMap comparison"), *Header, *Subject);
			//UE_LOG(LogTemp, Display, TEXT("%s"), *OutputStr);
			return false;
		}
		
		const FString OutputStr = FString::Printf(TEXT("%s: %s is not equivalent"), *Header, *Subject);

#if WITH_DEV_AUTOMATION_TESTS
		if (FHoudiniEditorEquivalenceUtils::TestBase != nullptr)
		{
			FHoudiniEditorEquivalenceUtils::TestBase->AddError(OutputStr);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("%s"), *OutputStr);
		}
#else
		UE_LOG(LogTemp, Error, TEXT("%s"), *OutputStr);
#endif
	}

	return Expression;
}

void FHoudiniEditorEquivalenceUtils::SetTestExpressionError(bool Enabled)
{
	TestExpressionErrorEnabled = Enabled;
	if (TestExpressionErrorEnabled)
	{
	//	UE_LOG(LogTemp, Display, TEXT("Enabling test expression errors."))
	}
	else
	{
	//	UE_LOG(LogTemp, Display, TEXT("Ignoring test expression errors (Usually we do this for TMaps)"))
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FHoudiniEditorEquivalenceUtils::SetAutomationBase(FAutomationTestBase* Test)
{
	FHoudiniEditorEquivalenceUtils::TestBase = Test;
}


#endif


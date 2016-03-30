/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk, Pavlo Penenko
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniAssetInput.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterButton.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterColor.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniAssetParameterFolder.h"
#include "HoudiniAssetParameterFolderList.h"
#include "HoudiniAssetParameterInt.h"
#include "HoudiniAssetParameterLabel.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniAssetParameterSeparator.h"
#include "HoudiniAssetParameterString.h"
#include "HoudiniAssetParameterFile.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniAssetParameterRamp.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniApi.h"
#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniAssetComponentMaterials.h"
#include "HoudiniAssetComponentVersion.h"
#include "HoudiniEngineString.h"


#if WITH_EDITOR

 /** Slate widget used to pick an asset to instantiate from an HDA with multiple assets inside. **/
class SAssetSelectionWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetSelectionWidget)
		: _WidgetWindow(), _AvailableAssetNames()
	{}

	SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(TArray<HAPI_StringHandle>, AvailableAssetNames)
		SLATE_END_ARGS()

public:

	SAssetSelectionWidget();

public:

	/** Widget construct. **/
	void Construct(const FArguments& InArgs);

	/** Return true if cancel button has been pressed. **/
	bool IsCancelled() const;

	/** Return true if constructed widget is valid. **/
	bool IsValidWidget() const;

	/** Return selected asset name. **/
	int32 GetSelectedAssetName() const;

protected:

	/** Called when Ok button is pressed. **/
	FReply OnButtonOk();

	/** Called when Cancel button is pressed. **/
	FReply OnButtonCancel();

	/** Called when user picks an asset. **/
	FReply OnButtonAssetPick(int32 AssetName);

protected:

	/** Parent widget window. **/
	TSharedPtr<SWindow> WidgetWindow;

	/** List of available Houdini Engine asset names. **/
	TArray<HAPI_StringHandle> AvailableAssetNames;

	/** Selected asset name. **/
	int32 SelectedAssetName;

	/** Is set to true if constructed widget is valid. **/
	bool bIsValidWidget;

	/** Is set to true if selection process has been cancelled. **/
	bool bIsCancelled;
};


SAssetSelectionWidget::SAssetSelectionWidget() :
	SelectedAssetName(-1),
	bIsValidWidget(false),
	bIsCancelled(false)
{

}


bool
SAssetSelectionWidget::IsCancelled() const
{
	return bIsCancelled;
}


bool
SAssetSelectionWidget::IsValidWidget() const
{
	return bIsValidWidget;
}


int32
SAssetSelectionWidget::GetSelectedAssetName() const
{
	return SelectedAssetName;
}


void
SAssetSelectionWidget::Construct(const FArguments& InArgs)
{
	WidgetWindow = InArgs._WidgetWindow;
	AvailableAssetNames = InArgs._AvailableAssetNames;

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
		.Content()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SAssignNew(VerticalBox, SVerticalBox)
			]
		]
	];

	for(int32 AssetNameIdx = 0, AssetNameNum = AvailableAssetNames.Num(); AssetNameIdx < AssetNameNum; ++AssetNameIdx)
	{
		FString AssetNameString;
		HAPI_StringHandle AssetName = AvailableAssetNames[AssetNameIdx];

		FHoudiniEngineString HoudiniEngineString(AssetName);
		if(HoudiniEngineString.ToFString(AssetNameString))
		{
			bIsValidWidget = true;
			FText AssetNameStringText = FText::FromString(AssetNameString);

			VerticalBox->AddSlot()
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.0f, 4.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.OnClicked(this, &SAssetSelectionWidget::OnButtonAssetPick, AssetName)
					.Text(AssetNameStringText)
					.ToolTipText(AssetNameStringText)
				]
			];
		}
	}
}


FReply
SAssetSelectionWidget::OnButtonAssetPick(int32 AssetName)
{
	SelectedAssetName = AssetName;

	WidgetWindow->HideWindow();
	WidgetWindow->RequestDestroyWindow();

	return FReply::Handled();
}


FReply
SAssetSelectionWidget::OnButtonOk()
{
	WidgetWindow->HideWindow();
	WidgetWindow->RequestDestroyWindow();

	return FReply::Handled();
}


FReply
SAssetSelectionWidget::OnButtonCancel()
{
	bIsCancelled = true;

	WidgetWindow->HideWindow();
	WidgetWindow->RequestDestroyWindow();

	return FReply::Handled();
}


#endif


// Macro to update given property on all components.
#define HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(COMPONENT_CLASS, PROPERTY)										\
	do																										\
	{																										\
		TArray<UActorComponent*> ReregisterComponents;														\
		for(TArray<USceneComponent*>::TIterator Iter(AttachChildren); Iter; ++Iter)							\
		{																									\
			COMPONENT_CLASS* Component = Cast<COMPONENT_CLASS>(*Iter);										\
			if(Component)																					\
			{																								\
				Component->PROPERTY = PROPERTY;																\
				ReregisterComponents.Add(Component);														\
			}																								\
		}																									\
																											\
		if(ReregisterComponents.Num() > 0)																	\
		{																									\
			FMultiComponentReregisterContext MultiComponentReregisterContext(ReregisterComponents);			\
		}																									\
	}																										\
	while(0)


bool
UHoudiniAssetComponent::bDisplayEngineNotInitialized = true;


bool
UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch = true;


UHoudiniAssetComponent::UHoudiniAssetComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAsset(nullptr),
	PreviousTransactionHoudiniAsset(nullptr),
	HoudiniAssetComponentMaterials(nullptr),
#if WITH_EDITOR

	CopiedHoudiniComponent(nullptr),

#endif
	AssetId(-1),
	GeneratedGeometryScaleFactor(HAPI_UNREAL_SCALE_FACTOR_POSITION),
	TransformScaleFactor(HAPI_UNREAL_SCALE_FACTOR_TRANSLATION),
	ImportAxis(HRSAI_Unreal),
	HapiNotificationStarted(0.0),
	AssetCookCount(0),
	/** Component flags. **/
	bEnableCooking(true),
	bUploadTransformsToHoudiniEngine(true),
	bTransformChangeTriggersCooks(false),
	bContainsHoudiniLogoGeometry(false),
	bIsNativeComponent(false),
	bIsPreviewComponent(false),
	bLoadedComponent(false),
	bIsPlayModeActive(false),
	bTimeCookInPlaymode(false),
	bUseHoudiniMaterials(true),
	bCookingTriggersDownstreamCooks(true),
	/** Component transient flags. **/
	bComponentCopyImported(false),
	bTransactionAssetChange(false),
	bWaitingForUpstreamAssetsToInstantiate(false),
	bParametersChanged(false),
	bComponentTransformHasChanged(false),
	bLoadedComponentRequiresInstantiation(false),
	HoudiniAssetComponentVersion(VER_HOUDINI_ENGINE_COMPONENT_BASE)
{
	UObject* Object = ObjectInitializer.GetObj();
	UObject* ObjectOuter = Object->GetOuter();

	if(ObjectOuter->IsA(AHoudiniAssetActor::StaticClass()))
	{
		bIsNativeComponent = true;
	}

	// Set scaling information.
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if(HoudiniRuntimeSettings)
	{
		GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
		TransformScaleFactor = HoudiniRuntimeSettings->TransformScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	// Set component properties.
	Mobility = EComponentMobility::Movable;
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bGenerateOverlapEvents = false;

	// Similar to UMeshComponent.
	CastShadow = true;
	bUseAsOccluder = true;
	bCanEverAffectNavigation = true;

	// This component requires render update.
	bNeverNeedsRenderUpdate = false;

	// Initialize static mesh generation parameters.
	bGeneratedDoubleSidedGeometry = false;
	GeneratedPhysMaterial = nullptr;
	GeneratedCollisionTraceFlag = CTF_UseDefault;
	GeneratedLpvBiasMultiplier = 1.0f;
	GeneratedLightMapResolution = 32;
	GeneratedLightMapCoordinateIndex = 1;
	bGeneratedUseMaximumStreamingTexelRatio = false;
	GeneratedStreamingDistanceMultiplier = 1.0f;

	// Make an invalid GUID, since we do not have any cooking requests.
	HapiGUID.Invalidate();

	// Create unique component GUID.
	ComponentGUID = FGuid::NewGuid();
}


UHoudiniAssetComponent::~UHoudiniAssetComponent()
{

}


void
UHoudiniAssetComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InThis);

	if(HoudiniAssetComponent && !HoudiniAssetComponent->IsPendingKill())
	{
		// Add references for all parameters.
		for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator
			IterParams(HoudiniAssetComponent->Parameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
			Collector.AddReferencedObject(HoudiniAssetParameter, InThis);
		}

		// Add references to all inputs.
		for(TArray<UHoudiniAssetInput*>::TIterator
			IterInputs(HoudiniAssetComponent->Inputs); IterInputs; ++IterInputs)
		{
			UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;
			Collector.AddReferencedObject(HoudiniAssetInput, InThis);
		}

		// Add references to all instance inputs.
		for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator
			Iter(HoudiniAssetComponent->InstanceInputs); Iter; ++Iter)
		{
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Iter.Value();
			Collector.AddReferencedObject(HoudiniAssetInstanceInput, InThis);
		}

		// Add references to all handles.
		for(TMap<FString, UHoudiniHandleComponent*>::TIterator
			IterHandles(HoudiniAssetComponent->HandleComponents); IterHandles; ++IterHandles)
		{
			UHoudiniHandleComponent* HandleComponent = IterHandles.Value();
			Collector.AddReferencedObject(HandleComponent, InThis);
		}

		// Add references to all static meshes and corresponding geo parts.
		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator
			Iter(HoudiniAssetComponent->StaticMeshes); Iter; ++Iter)
		{
			UStaticMesh* StaticMesh = Iter.Value();
			if(StaticMesh)
			{
				Collector.AddReferencedObject(StaticMesh, InThis);
			}
		}

		// Add references to all static meshes and their static mesh components.
		for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator
			Iter(HoudiniAssetComponent->StaticMeshComponents); Iter; ++Iter)
		{
			UStaticMesh* StaticMesh = Iter.Key();
			UStaticMeshComponent* StaticMeshComponent = Iter.Value();

			Collector.AddReferencedObject(StaticMesh, InThis);
			Collector.AddReferencedObject(StaticMeshComponent, InThis);
		}

		// Add references to all spline components.
		for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator
			Iter(HoudiniAssetComponent->SplineComponents); Iter; ++Iter)
		{
			UHoudiniSplineComponent* HoudiniSplineComponent = Iter.Value();
			Collector.AddReferencedObject(HoudiniSplineComponent, InThis);
		}

		// Retrieve asset associated with this component and add reference to it.
		// Also do not add reference if it is being referenced by preview component.
		UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
		if(HoudiniAsset && !HoudiniAssetComponent->bIsPreviewComponent)
		{
			// Manually add a reference to Houdini asset from this component.
			Collector.AddReferencedObject(HoudiniAsset, InThis);
		}

		// Add reference to material replacements.
		if(HoudiniAssetComponent->HoudiniAssetComponentMaterials)
		{
			Collector.AddReferencedObject(HoudiniAssetComponent->HoudiniAssetComponentMaterials, InThis);
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetComponent::SetNative(bool InbIsNativeComponent)
{
	bIsNativeComponent = InbIsNativeComponent;
}


HAPI_AssetId
UHoudiniAssetComponent::GetAssetId() const
{
	return AssetId;
}


void
UHoudiniAssetComponent::SetAssetId(HAPI_AssetId InAssetId)
{
	AssetId = InAssetId;
}


bool
UHoudiniAssetComponent::HasValidAssetId() const
{
	return FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId);
}


UHoudiniAsset*
UHoudiniAssetComponent::GetHoudiniAsset() const
{
	return HoudiniAsset;
}


AHoudiniAssetActor*
UHoudiniAssetComponent::GetHoudiniAssetActorOwner() const
{
	return Cast<AHoudiniAssetActor>(GetOwner());
}


void
UHoudiniAssetComponent::SetHoudiniAsset(UHoudiniAsset* InHoudiniAsset)
{
	// If it is the same asset, do nothing.
	if(InHoudiniAsset == HoudiniAsset)
	{
		return;
	}

	UHoudiniAsset* Asset = nullptr;
	AHoudiniAssetActor* HoudiniAssetActor = Cast<AHoudiniAssetActor>(GetOwner());

	HoudiniAsset = InHoudiniAsset;

	// Reset material tracking.
	if(HoudiniAssetComponentMaterials)
	{
		HoudiniAssetComponentMaterials->ResetMaterialInfo();
	}

	if(!bIsNativeComponent)
	{
		return;
	}

#if WITH_EDITOR

	// Houdini Asset has been changed, we need to reset corresponding HDA and relevant resources.
	ResetHoudiniResources();

#endif

	// Clear all created parameters.
	ClearParameters();

	// Clear all inputs.
	ClearInputs();

	// Clear all instance inputs.
	ClearInstanceInputs();

	// Release all curve related resources.
	ClearCurves();

	// Clear all handles.
	ClearHandles();

	// Set Houdini logo to be default geometry.
	ReleaseObjectGeoPartResources(StaticMeshes);
	StaticMeshes.Empty();
	StaticMeshComponents.Empty();
	CreateStaticMeshHoudiniLogoResource(StaticMeshes);

	bIsPreviewComponent = false;
	if(!InHoudiniAsset)
	{
#if WITH_EDITOR
		UpdateEditorProperties(false);
#endif
		return;
	}

	if(HoudiniAssetActor)
	{
		bIsPreviewComponent = HoudiniAssetActor->IsUsedForPreview();
	}

	if(!bIsNativeComponent)
	{
		bLoadedComponent = false;
	}

	// Get instance of Houdini Engine.
	FHoudiniEngine& HoudiniEngine = FHoudiniEngine::Get();

#if WITH_EDITOR

	if(!bIsPreviewComponent)
	{
		if(HoudiniEngine.IsInitialized())
		{
			if(!bLoadedComponent)
			{
				// If component is not a loaded component, instantiate and start ticking.
				StartTaskAssetInstantiation(false, true);
			}
			else if(bTransactionAssetChange)
			{
				// If assigned asset is transacted asset, instantiate and start ticking. Also treat as loaded component.
				StartTaskAssetInstantiation(true, true);
			}
		}
		else
		{
			if(UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch && HoudiniEngine.CheckHapiVersionMismatch())
			{
				// We have mismatch in defined and running versions.
				int32 RunningEngineMajor = 0;
				int32 RunningEngineMinor = 0;
				int32 RunningEngineApi = 0;

				const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

				// Retrieve version numbers for running Houdini Engine.
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi);

				// Get platform specific name of libHAPI.
				FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();

				// Some sanity checks.
				int32 BuiltEngineMajor = FMath::Max(0, HAPI_VERSION_HOUDINI_ENGINE_MAJOR);
				int32 BuiltEngineMinor = FMath::Max(0, HAPI_VERSION_HOUDINI_ENGINE_MINOR);
				int32 BuiltEngineApi = FMath::Max(0, HAPI_VERSION_HOUDINI_ENGINE_API);

				FString WarningMessage = FString::Printf(
					TEXT("Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d mismatch. ")
					TEXT("%s was loaded, but has wrong version. "),
					TEXT("No cooking / instantiation will take place."),
					BuiltEngineMajor,
					BuiltEngineMinor,
					BuiltEngineApi,
					RunningEngineMajor,
					RunningEngineMinor,
					RunningEngineApi,
					*LibHAPIName);

				FString WarningTitle = TEXT("Houdini Engine Plugin Warning");
				FText WarningTitleText = FText::FromString(WarningTitle);
				FMessageDialog::Debugf(FText::FromString(WarningMessage), &WarningTitleText);

				UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch = false;
			}
			else if(UHoudiniAssetComponent::bDisplayEngineNotInitialized)
			{
				// Get platform specific name of libHAPI.
				FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();

				// If this is first time component is instantiated and we do not have Houdini Engine initialized.
				FString WarningTitle = TEXT("Houdini Engine Plugin Warning");
				FText WarningTitleText = FText::FromString(WarningTitle);
				FString WarningMessage = FString::Printf(TEXT("Houdini Installation was not detected. ")
														 TEXT("Failed to locate or load %s. ")
														 TEXT("No cooking / instantiation will take place."),
														 *LibHAPIName);

				FMessageDialog::Debugf(FText::FromString(WarningMessage), &WarningTitleText);

				UHoudiniAssetComponent::bDisplayEngineNotInitialized = false;
			}
		}
	}

#endif
}


void
UHoudiniAssetComponent::AddDownstreamAsset(UHoudiniAssetComponent* InDownstreamAssetComponent, int32 InInputIndex)
{
	if(InDownstreamAssetComponent)
	{
		if(DownstreamAssetConnections.Contains(InDownstreamAssetComponent))
		{
			TSet<int32>& InputIndicesSet = DownstreamAssetConnections[InDownstreamAssetComponent];
			InputIndicesSet.Add(InInputIndex);
		}
		else
		{
			TSet<int32> InputIndicesSet;
			InputIndicesSet.Add(InInputIndex);
			DownstreamAssetConnections.Add(InDownstreamAssetComponent, InputIndicesSet);
		}
	}
}


void
UHoudiniAssetComponent::RemoveDownstreamAsset(UHoudiniAssetComponent* InDownstreamAssetComponent, int32 InInputIndex)
{
	if(DownstreamAssetConnections.Contains(InDownstreamAssetComponent))
	{
		TSet<int32>& InputIndicesSet = DownstreamAssetConnections[InDownstreamAssetComponent];
		if(InputIndicesSet.Contains(InInputIndex))
		{
			InputIndicesSet.Remove(InInputIndex);
		}
	}
}


void
UHoudiniAssetComponent::CreateObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap)
{
	// Reset Houdini logo flag.
	bContainsHoudiniLogoGeometry = false;

	// We need to store instancers as they need to be processed after all other meshes.
	TArray<FHoudiniGeoPartObject> FoundInstancers;
	TArray<FHoudiniGeoPartObject> FoundCurves;

	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshMap); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
		UStaticMesh* StaticMesh = Iter.Value();

		if(HoudiniGeoPartObject.IsInstancer())
		{
			// This geo part is an instancer and has no mesh assigned.
			check(!StaticMesh);
			FoundInstancers.Add(HoudiniGeoPartObject);
		}
		else if(HoudiniGeoPartObject.IsCurve())
		{
			// This geo part is a curve and has no mesh assigned.
			check(!StaticMesh);
			FoundCurves.Add(HoudiniGeoPartObject);
		}
		else if(HoudiniGeoPartObject.IsVisible())
		{
			// This geo part is visible and not an instancer and must have static mesh assigned.
			if(!StaticMesh)
			{
				continue;
			}

			UStaticMeshComponent* StaticMeshComponent = nullptr;
			UStaticMeshComponent* const* FoundStaticMeshComponent = StaticMeshComponents.Find(StaticMesh);

			if(FoundStaticMeshComponent)
			{
				StaticMeshComponent = *FoundStaticMeshComponent;
			}
			else
			{
				// Create necessary component.
				StaticMeshComponent = NewObject<UStaticMeshComponent>(GetOwner(), UStaticMeshComponent::StaticClass(),
					NAME_None, RF_Transactional);

				// Add to map of components.
				StaticMeshComponents.Add(StaticMesh, StaticMeshComponent);

				// Attach created static mesh component to our Houdini component.
				StaticMeshComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);

				StaticMeshComponent->SetStaticMesh(StaticMesh);
				StaticMeshComponent->SetVisibility(true);
				StaticMeshComponent->SetMobility(Mobility);
				StaticMeshComponent->RegisterComponent();
			}

			// If this is a collision geo, we need to make it invisible.
			if(HoudiniGeoPartObject.IsCollidable())
			{
				StaticMeshComponent->SetVisibility(false);
			}

			// Transform the component by transformation provided by HAPI.
			StaticMeshComponent->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);
		}
	}

	// Skip self assignment.
	if(&StaticMeshes != &StaticMeshMap)
	{
		StaticMeshes = StaticMeshMap;
	}

#if WITH_EDITOR

	if(FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId))
	{
		// Create necessary instance inputs.
		CreateInstanceInputs(FoundInstancers);

		// Create necessary curves.
		CreateCurves(FoundCurves);
	}

#endif
}


void
UHoudiniAssetComponent::ReleaseObjectGeoPartResources(bool bDeletePackages)
{
	ReleaseObjectGeoPartResources(StaticMeshes, bDeletePackages);
}


void
UHoudiniAssetComponent::ReleaseObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap,
	bool bDeletePackages)
{
	// Record generated static meshes which we need to delete.
	TArray<UObject*> StaticMeshesToDelete;

	// Get Houdini logo.
	UStaticMesh* HoudiniLogoMesh = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh();

	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshMap); Iter; ++Iter)
	{
		UStaticMesh* StaticMesh = Iter.Value();
		if(StaticMesh)
		{
			// Locate corresponding component.
			UStaticMeshComponent* const* FoundStaticMeshComponent = StaticMeshComponents.Find(StaticMesh);
			if(FoundStaticMeshComponent)
			{
				// Remove component from map of static mesh components.
				StaticMeshComponents.Remove(StaticMesh);

				// Detach and destroy the component.
				UStaticMeshComponent* StaticMeshComponent = *FoundStaticMeshComponent;
				StaticMeshComponent->UnregisterComponent();
				StaticMeshComponent->DetachFromParent();
				StaticMeshComponent->DestroyComponent();

				// Remove this component from the list of attached components.
				AttachChildren.Remove(StaticMeshComponent);
			}
		}

		if(bDeletePackages && StaticMesh && StaticMesh != HoudiniLogoMesh)
		{
			// Make sure this static mesh is not referenced.
			UObject* ObjectMesh = (UObject*) StaticMesh;
			FReferencerInformationList Referencers;

			bool bReferenced = true;

			{
				// Check if object is referenced and get its referencers, if it is.
				bReferenced = IsReferenced(ObjectMesh, GARBAGE_COLLECTION_KEEPFLAGS, true, &Referencers);
			}

			if(!bReferenced || IsObjectReferencedLocally(StaticMesh, Referencers))
			{
				// Only delete generated mesh if it has not been saved manually.
				UPackage* Package = Cast<UPackage>(StaticMesh->GetOuter());
				if(!Package || !Package->bHasBeenFullyLoaded)
				{
					StaticMeshesToDelete.Add(StaticMesh);
				}
			}
		}
	}

	// Remove unused meshes.
	StaticMeshMap.Empty();

#if WITH_EDITOR

	// Delete no longer used generated static meshes.
	int32 MeshNum = StaticMeshesToDelete.Num();
	if(bDeletePackages && MeshNum > 0)
	{
		for(int32 MeshIdx = 0; MeshIdx < MeshNum; ++MeshIdx)
		{
			UObject* ObjectToDelete = StaticMeshesToDelete[MeshIdx];
			ObjectTools::DeleteSingleObject(ObjectToDelete, false);
		}
	}

#endif
}


bool
UHoudiniAssetComponent::IsObjectReferencedLocally(UStaticMesh* StaticMesh,
	FReferencerInformationList& Referencers) const
{
	if(Referencers.InternalReferences.Num() == 0 && Referencers.ExternalReferences.Num() == 1)
	{
		const FReferencerInformation& ExternalReferencer = Referencers.ExternalReferences[0];
		if(ExternalReferencer.Referencer == this)
		{
			// Only referencer is this component.
			return true;
		}
	}

	return false;
}


void
UHoudiniAssetComponent::CollectSubstanceParameters(TMap<FString, UHoudiniAssetParameter*>& SubstanceParameters) const
{
	SubstanceParameters.Empty();

	for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TConstIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		if(HoudiniAssetParameter && HoudiniAssetParameter->IsSubstanceParameter())
		{
			SubstanceParameters.Add(HoudiniAssetParameter->GetParameterName(), HoudiniAssetParameter);
		}
	}
}


void
UHoudiniAssetComponent::CollectAllParametersOfType(UClass* ParameterClass,
	TMap<FString, UHoudiniAssetParameter*>& ClassParameters) const
{
	ClassParameters.Empty();

	for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TConstIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		if(HoudiniAssetParameter && HoudiniAssetParameter->IsA(ParameterClass))
		{
			ClassParameters.Add(HoudiniAssetParameter->GetParameterName(), HoudiniAssetParameter);
		}
	}
}


UHoudiniAssetParameter*
UHoudiniAssetComponent::FindParameter(const FString& ParameterName) const
{
	UHoudiniAssetParameter* const* FoundHoudiniAssetParameter = ParameterByName.Find(ParameterName);
	UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;

	if(FoundHoudiniAssetParameter)
	{
		HoudiniAssetParameter = *FoundHoudiniAssetParameter;
	}

	return HoudiniAssetParameter;
}



void
UHoudiniAssetComponent::GetAllUsedStaticMeshes(TArray<UStaticMesh*>& UsedStaticMeshes)
{
	UsedStaticMeshes.Empty();

	// Add all static meshes.
	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshes); Iter; ++Iter)
	{
		UStaticMesh* StaticMesh = Iter.Value();
		if(StaticMesh)
		{
			UsedStaticMeshes.Add(StaticMesh);
		}
	}
}


bool
UHoudiniAssetComponent::CheckGlobalSettingScaleFactors() const
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if(HoudiniRuntimeSettings)
	{
		return (GeneratedGeometryScaleFactor == HoudiniRuntimeSettings->GeneratedGeometryScaleFactor &&
				TransformScaleFactor == HoudiniRuntimeSettings->TransformScaleFactor);
	}

	return (GeneratedGeometryScaleFactor == HAPI_UNREAL_SCALE_FACTOR_POSITION &&
			TransformScaleFactor == HAPI_UNREAL_SCALE_FACTOR_TRANSLATION);
}


#if WITH_EDITOR

bool
UHoudiniAssetComponent::IsInstantiatingOrCooking() const
{
	return HapiGUID.IsValid();
}


bool
UHoudiniAssetComponent::HasBeenInstantiatedButNotCooked() const
{
	return (FHoudiniEngineUtils::IsValidAssetId(AssetId) && (0 == AssetCookCount));
}


void
UHoudiniAssetComponent::AssignUniqueActorLabel()
{
	if(GEditor && FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		AHoudiniAssetActor* HoudiniAssetActor = GetHoudiniAssetActorOwner();
		if(HoudiniAssetActor)
		{
			FString UniqueName;
			if(FHoudiniEngineUtils::GetHoudiniAssetName(AssetId, UniqueName))
			{
				FActorLabelUtilities::SetActorLabelUnique(HoudiniAssetActor, UniqueName);
			}
		}
	}
}


void
UHoudiniAssetComponent::StartHoudiniUIUpdateTicking()
{
	// If we have no timer delegate spawned for ui update, spawn one.
	if(!TimerDelegateUIUpdate.IsBound() && GEditor)
	{
		TimerDelegateUIUpdate = FTimerDelegate::CreateUObject(this, &UHoudiniAssetComponent::TickHoudiniUIUpdate);

		// We need to register delegate with the timer system.
		static const float TickTimerDelay = 0.25f;
		GEditor->GetTimerManager()->SetTimer(TimerHandleUIUpdate, TimerDelegateUIUpdate, TickTimerDelay, true);
	}
}


void
UHoudiniAssetComponent::StopHoudiniUIUpdateTicking()
{
	if(TimerDelegateUIUpdate.IsBound() && GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(TimerHandleUIUpdate);
		TimerDelegateUIUpdate.Unbind();
	}
}


void
UHoudiniAssetComponent::TickHoudiniUIUpdate()
{
	UpdateEditorProperties(true);
}


void
UHoudiniAssetComponent::StartHoudiniTicking()
{
	// If we have no timer delegate spawned for this component, spawn one.
	if(!TimerDelegateCooking.IsBound() && GEditor)
	{
		TimerDelegateCooking = FTimerDelegate::CreateUObject(this, &UHoudiniAssetComponent::TickHoudiniComponent);

		// We need to register delegate with the timer system.
		static const float TickTimerDelay = 0.25f;
		GEditor->GetTimerManager()->SetTimer(TimerHandleCooking, TimerDelegateCooking, TickTimerDelay, true);

		// Grab current time for delayed notification.
		HapiNotificationStarted = FPlatformTime::Seconds();
	}
}


void
UHoudiniAssetComponent::StopHoudiniTicking()
{
	if(TimerDelegateCooking.IsBound() && GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(TimerHandleCooking);
		TimerDelegateCooking.Unbind();

		// Reset time for delayed notification.
		HapiNotificationStarted = 0.0;
	}
}


void
UHoudiniAssetComponent::PostCook(bool bCookError)
{
	// Show busy cursor.
	FScopedBusyCursor ScopedBusyCursor;

	// Create parameters and inputs.
	CreateParameters();
	CreateInputs();
	CreateHandles();

	if(bCookError)
	{
		return;
	}

	FTransform ComponentTransform;
	TMap<FHoudiniGeoPartObject, UStaticMesh*> NewStaticMeshes;
	if(FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(this, GetOutermost(),
		StaticMeshes, NewStaticMeshes, ComponentTransform))
	{
		// Remove all duplicates. After this operation, old map will have meshes which we need
		// to deallocate.
		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator
			Iter(NewStaticMeshes); Iter; ++Iter)
		{
			const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
			UStaticMesh* StaticMesh = Iter.Value();

			// Remove mesh from previous map of meshes.
			UStaticMesh* const* FoundOldStaticMesh = StaticMeshes.Find(HoudiniGeoPartObject);
			if(FoundOldStaticMesh)
			{
				UStaticMesh* OldStaticMesh = *FoundOldStaticMesh;

				if(OldStaticMesh == StaticMesh)
				{
					// Mesh has not changed, we need to remove it from old map to avoid
					// deallocation.
					StaticMeshes.Remove(HoudiniGeoPartObject);
				}
			}
		}

		// Free meshes and components that are no longer used.
		ReleaseObjectGeoPartResources(StaticMeshes, true);

		// Set meshes and create new components for those meshes that do not have them.
		if(NewStaticMeshes.Num() > 0)
		{
			CreateObjectGeoPartResources(NewStaticMeshes);
		}
		else
		{
			CreateStaticMeshHoudiniLogoResource(NewStaticMeshes);
		}
	}

	// Invoke cooks of downstream assets.
	if(bCookingTriggersDownstreamCooks)
	{
		for(TMap<UHoudiniAssetComponent*, TSet<int32>>::TIterator IterAssets(DownstreamAssetConnections);
			IterAssets;
			++IterAssets)
		{
			UHoudiniAssetComponent* DownstreamAsset = IterAssets.Key();
			DownstreamAsset->NotifyParameterChanged(nullptr);
		}
	}
}


void
UHoudiniAssetComponent::TickHoudiniComponent()
{
	// Get settings.
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	FHoudiniEngineTaskInfo TaskInfo;
	bool bStopTicking = false;
	bool bFinishedLoadedInstantiation = false;

	static float NotificationFadeOutDuration = 2.0f;
	static float NotificationExpireDuration = 2.0f;
	static double NotificationUpdateFrequency = 2.0f;

	// Check whether we want to display Slate cooking and instantiation notifications.
	bool bDisplaySlateCookingNotifications = false;
	if(HoudiniRuntimeSettings)
	{
		bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;
	}

	if(HapiGUID.IsValid())
	{
		// If we have a valid task GUID.
		if(FHoudiniEngine::Get().RetrieveTaskInfo(HapiGUID, TaskInfo))
		{
			if(EHoudiniEngineTaskState::None != TaskInfo.TaskState)
			{
				if(!NotificationPtr.IsValid() && bDisplaySlateCookingNotifications)
				{
					FNotificationInfo Info(TaskInfo.StatusText);

					Info.bFireAndForget = false;
					Info.FadeOutDuration = NotificationFadeOutDuration;
					Info.ExpireDuration = NotificationExpireDuration;

					TSharedPtr<FSlateDynamicImageBrush> HoudiniBrush = FHoudiniEngine::Get().GetHoudiniLogoBrush();
					if(HoudiniBrush.IsValid())
					{
						Info.Image = HoudiniBrush.Get();
					}

					if((FPlatformTime::Seconds() - HapiNotificationStarted) >= NotificationUpdateFrequency)
					{
						if(!IsPlayModeActive())
						{
							NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
						}
					}
				}
			}

			switch(TaskInfo.TaskState)
			{
				case EHoudiniEngineTaskState::FinishedInstantiation:
				{
					HOUDINI_LOG_MESSAGE(TEXT("    FinishedInstantiation."));

					if(FHoudiniEngineUtils::IsValidAssetId(TaskInfo.AssetId))
					{
						// Set new asset id.
						SetAssetId(TaskInfo.AssetId);

						// Assign unique actor label based on asset name.
						AssignUniqueActorLabel();

						// Create default preset buffer.
						CreateDefaultPreset();

						// If necessary, set asset transform.
						if(bUploadTransformsToHoudiniEngine)
						{
							// Retrieve the current component-to-world transform for this component.
							if(!FHoudiniEngineUtils::HapiSetAssetTransform(AssetId, GetComponentTransform()))
							{
								HOUDINI_LOG_MESSAGE(TEXT("Failed Uploading Initial Transformation back to HAPI."));
							}
						}

						if(NotificationPtr.IsValid() && bDisplaySlateCookingNotifications)
						{
							TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
							if(NotificationItem.IsValid())
							{
								NotificationItem->SetText(TaskInfo.StatusText);
								NotificationItem->ExpireAndFadeout();

								NotificationPtr.Reset();
							}
						}

						FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
						HapiGUID.Invalidate();

						// We just finished instantiation, we need to reset cook counter.
						AssetCookCount = 0;

						if(TaskInfo.bLoadedComponent)
						{
							bFinishedLoadedInstantiation = true;
						}

						FHoudiniEngine::Get().SetHapiState(HAPI_RESULT_SUCCESS);
					}
					else
					{
						bStopTicking = true;
						HOUDINI_LOG_MESSAGE(TEXT("    Received invalid asset id."));
					}

					break;
				}

				case EHoudiniEngineTaskState::FinishedCooking:
				{
					HOUDINI_LOG_MESSAGE(TEXT("    FinishedCooking."));

					if(FHoudiniEngineUtils::IsValidAssetId(TaskInfo.AssetId))
					{
						// Set new asset id.
						SetAssetId(TaskInfo.AssetId);

						// Call post cook event.
						PostCook();

						// Need to update rendering information.
						UpdateRenderingInformation();

						// Force editor to redraw viewports.
						if(GEditor)
						{
							GEditor->RedrawAllViewports();
						}

						// Update properties panel after instantiation.
						UpdateEditorProperties(true);
					}
					else
					{
						HOUDINI_LOG_MESSAGE(TEXT("    Received invalid asset id."));
					}

					if(NotificationPtr.IsValid() && bDisplaySlateCookingNotifications)
					{
						TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
						if(NotificationItem.IsValid())
						{
							NotificationItem->SetText(TaskInfo.StatusText);
							NotificationItem->ExpireAndFadeout();

							NotificationPtr.Reset();
						}
					}

					FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
					HapiGUID.Invalidate();

					bStopTicking = true;
					AssetCookCount++;

					break;
				}

				case EHoudiniEngineTaskState::FinishedCookingWithErrors:
				{
					HOUDINI_LOG_MESSAGE(TEXT("    FinishedCookingWithErrors."));

					if(FHoudiniEngineUtils::IsValidAssetId(TaskInfo.AssetId))
					{
						// Call post cook event with error parameter. This will create parameters, inputs and handles.
						PostCook(true);

						// Create default preset buffer.
						CreateDefaultPreset();

						// Update properties panel.
						UpdateEditorProperties(true);

						// If necessary, set asset transform.
						if(bUploadTransformsToHoudiniEngine)
						{
							// Retrieve the current component-to-world transform for this component.
							if(!FHoudiniEngineUtils::HapiSetAssetTransform(AssetId, GetComponentTransform()))
							{
								HOUDINI_LOG_MESSAGE(TEXT("Failed Uploading Initial Transformation back to HAPI."));
							}
						}
					}

					if(NotificationPtr.IsValid() && bDisplaySlateCookingNotifications)
					{
						TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
						if(NotificationItem.IsValid())
						{
							NotificationItem->SetText(TaskInfo.StatusText);
							NotificationItem->ExpireAndFadeout();

							NotificationPtr.Reset();
						}
					}

					FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
					HapiGUID.Invalidate();

					bStopTicking = true;
					AssetCookCount++;

					break;
				}

				case EHoudiniEngineTaskState::Aborted:
				case EHoudiniEngineTaskState::FinishedInstantiationWithErrors:
				{
					HOUDINI_LOG_MESSAGE(TEXT("    FinishedInstantiationWithErrors."));

					bool bLicensingIssue = false;
					switch(TaskInfo.Result)
					{
						case HAPI_RESULT_NO_LICENSE_FOUND:
						{
							FHoudiniEngine::Get().SetHapiState(HAPI_RESULT_NO_LICENSE_FOUND);

							bLicensingIssue = true;
							break;
						}

						case HAPI_RESULT_DISALLOWED_NC_LICENSE_FOUND:
						case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_C_LICENSE:
						case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_LC_LICENSE:
						case HAPI_RESULT_DISALLOWED_LC_ASSET_WITH_C_LICENSE:
						{
							bLicensingIssue = true;
							break;
						}

						default:
						{
							break;
						}
					}

					if(bLicensingIssue)
					{
						const FString& StatusMessage = TaskInfo.StatusText.ToString();
						HOUDINI_LOG_MESSAGE(TEXT("%s"), *StatusMessage);

						FString WarningTitle = TEXT("Houdini Engine Plugin Warning");
						FText WarningTitleText = FText::FromString(WarningTitle);
						FString WarningMessage = FString::Printf(TEXT("Houdini License issue - %s."), *StatusMessage);

						FMessageDialog::Debugf(FText::FromString(WarningMessage), &WarningTitleText);
					}

					if(NotificationPtr.IsValid() && bDisplaySlateCookingNotifications)
					{
						TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
						if(NotificationItem.IsValid())
						{
							NotificationItem->SetText(TaskInfo.StatusText);
							NotificationItem->ExpireAndFadeout();

							NotificationPtr.Reset();
						}
					}

					if(TaskInfo.bLoadedComponent)
					{
						bFinishedLoadedInstantiation = true;
					}

					FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
					HapiGUID.Invalidate();

					bStopTicking = true;
					AssetCookCount = 0;

					break;
				}

				case EHoudiniEngineTaskState::Processing:
				{

					if(NotificationPtr.IsValid() && bDisplaySlateCookingNotifications)
					{
						TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
						if(NotificationItem.IsValid())
						{
							NotificationItem->SetText(TaskInfo.StatusText);
						}
					}

					break;
				}

				case EHoudiniEngineTaskState::None:
				default:
				{
					break;
				}
			}
		}
		else
		{
			// Task information does not exist, we can stop ticking.
			HapiGUID.Invalidate();
			bStopTicking = true;
		}
	}

	if(!IsInstantiatingOrCooking())
	{
		if(HasBeenInstantiatedButNotCooked() || bParametersChanged || bComponentTransformHasChanged)
		{
			// Grab current time for delayed notification.
			HapiNotificationStarted = FPlatformTime::Seconds();

			// We just submitted a task, we want to continue ticking.
			bStopTicking = false;

			if(bWaitingForUpstreamAssetsToInstantiate)
			{
				// We are waiting for upstream assets to instantiate.

				bWaitingForUpstreamAssetsToInstantiate = false;
				for(auto LocalInput : Inputs)
				{
					bWaitingForUpstreamAssetsToInstantiate |= LocalInput->DoesInputAssetNeedInstantiation();
				}

				// Try instantiating this asset again.
				if(!bWaitingForUpstreamAssetsToInstantiate)
				{
					bLoadedComponentRequiresInstantiation = true;
				}
			}
			else if(bLoadedComponentRequiresInstantiation)
			{
				// This component has been loaded and requires instantiation.

				bLoadedComponentRequiresInstantiation = false;
				StartTaskAssetInstantiation(true);
			}
			else if(bFinishedLoadedInstantiation)
			{
				// If we are doing first cook after instantiation.

				// Update parameter node id for all loaded parameters.
				UpdateLoadedParameters();

				// Additionally, we need to update and create assets for all input parameters that have geos assigned.
				UpdateLoadedInputs();

				// We also need to upload loaded curve points.
				UploadLoadedCurves();

				// If we finished loading instantiation, we can restore preset data.
				if(PresetBuffer.Num() > 0)
				{
					FHoudiniEngineUtils::SetAssetPreset(AssetId, PresetBuffer);
					PresetBuffer.Empty();
				}

				// Upload changed parameters back to HAPI.
				UploadChangedParameters();

				// Create asset cooking task object and submit it for processing.
				StartTaskAssetCooking();
			}
			else if(bEnableCooking)
			{
				// Upload changed parameters back to HAPI.
				UploadChangedParameters();

				// Create asset cooking task object and submit it for processing.
				StartTaskAssetCooking();
			}
			else
			{
				// This will only happen if cooking is disabled.
				bStopTicking = true;
			}
		}
		else
		{
			// Nothing has changed, we can terminate ticking.
			bStopTicking = true;
		}
	}

	if(bStopTicking)
	{
		StopHoudiniTicking();
	}
}


void
UHoudiniAssetComponent::UpdateEditorProperties(bool bConditionalUpdate)
{
	AHoudiniAssetActor* HoudiniAssetActor = GetHoudiniAssetActorOwner();

	FPropertyEditorModule& PropertyModule =
		FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Locate the details panel.
	FName DetailsPanelName = "LevelEditorSelectionDetails";
	TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsPanelName);

	if(DetailsView.IsValid())
	{
		if(DetailsView->IsLocked())
		{
			// If details panel is locked, locate selected actors and check if this component belongs to one of them.

			const TArray<TWeakObjectPtr<AActor> >& SelectedDetailActors = DetailsView->GetSelectedActors();
			bool bFoundActor = false;

			for(int32 ActorIdx = 0, ActorNum = SelectedDetailActors.Num(); ActorIdx < ActorNum; ++ActorIdx)
			{
				TWeakObjectPtr<AActor> SelectedActor = SelectedDetailActors[ActorIdx];
				if(SelectedActor.IsValid() && SelectedActor.Get() == HoudiniAssetActor)
				{
					bFoundActor = true;
					break;
				}
			}

			// Details panel is locked, but our actor is not selected.
			if(!bFoundActor)
			{
				return;
			}
		}
		else
		{
			// If our actor is not selected (and details panel is not locked) don't do any updates.
			if(!HoudiniAssetActor->IsSelected())
			{
				return;
			}
		}
	}
	else
	{
		// We have no details panel, nothing to update.
		return;
	}

	if(GEditor && HoudiniAssetActor && bIsNativeComponent)
	{
		if(bConditionalUpdate && FSlateApplication::Get().HasAnyMouseCaptor())
		{
			// We want to avoid UI update if this is a conditional update and widget has captured the mouse.
			StartHoudiniUIUpdateTicking();
			return;
		}

		TArray<UObject*> SelectedActors;
		SelectedActors.Add(HoudiniAssetActor);

		// Reset selected actor to itself, force refresh and override the lock.
		DetailsView->SetObjects(SelectedActors, true, true);
	}

	StopHoudiniUIUpdateTicking();
}


void
UHoudiniAssetComponent::StartTaskAssetInstantiation(bool bLoadedComponent, bool bStartTicking)
{
	// We first need to make sure all our asset inputs have been instantiated and reconnected.
	for(auto LocalInput : Inputs)
	{
		bool bInputAssetNeedsInstantiation = LocalInput->DoesInputAssetNeedInstantiation();
		if(bInputAssetNeedsInstantiation)
		{
			UHoudiniAssetComponent* LocalInputAssetComponent = LocalInput->GetConnectedInputAssetComponent();
			LocalInputAssetComponent->NotifyParameterChanged(nullptr);
			bWaitingForUpstreamAssetsToInstantiate = true;
		}
	}

	if(!bWaitingForUpstreamAssetsToInstantiate)
	{
		// Check if asset has multiple Houdini assets inside.
		HAPI_AssetLibraryId AssetLibraryId = -1;
		TArray<HAPI_StringHandle> AssetNames;

		if(FHoudiniEngineUtils::GetAssetNames(HoudiniAsset, AssetLibraryId, AssetNames))
		{
			HAPI_StringHandle PickedAssetName = AssetNames[0];
			bool bShowMultiAssetDialog = false;

			const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
			if(HoudiniRuntimeSettings)
			{
				bShowMultiAssetDialog = HoudiniRuntimeSettings->bShowMultiAssetDialog;
			}

			if(bShowMultiAssetDialog && AssetNames.Num() > 1)
			{
				// If we have more than one asset, we need to present user with choice dialog.

				TSharedPtr<SWindow> ParentWindow;

				// Check if the main frame is loaded. When using the old main frame it may not be.
				if(FModuleManager::Get().IsModuleLoaded("MainFrame"))
				{
					IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
					ParentWindow = MainFrame.GetParentWindow();
				}

				if(ParentWindow.IsValid())
				{
					TSharedPtr<SAssetSelectionWidget> AssetSelectionWidget;

					TSharedRef<SWindow> Window = SNew(SWindow)
						.Title(LOCTEXT("WindowTitle", "Select an asset to instantiate"))
						.ClientSize(FVector2D(640, 480))
						.SupportsMinimize(false)
						.SupportsMaximize(false)
						.HasCloseButton(false);

					Window->SetContent(SAssignNew(AssetSelectionWidget, SAssetSelectionWidget)
						.WidgetWindow(Window)
						.AvailableAssetNames(AssetNames));

					if(AssetSelectionWidget->IsValidWidget())
					{
						FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

						int32 DialogPickedAssetName = AssetSelectionWidget->GetSelectedAssetName();
						if(-1 != DialogPickedAssetName)
						{
							PickedAssetName = DialogPickedAssetName;
						}
					}
				}
			}

			// Create new GUID to identify this request.
			HapiGUID = FGuid::NewGuid();

			FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetInstantiation, HapiGUID);
			Task.Asset = HoudiniAsset;
			Task.ActorName = GetOuter()->GetName();
			Task.bLoadedComponent = bLoadedComponent;
			Task.AssetLibraryId = AssetLibraryId;
			Task.AssetHapiName = PickedAssetName;
			FHoudiniEngine::Get().AddTask(Task);
		}
		else
		{
			HOUDINI_LOG_MESSAGE(TEXT("Cancelling asset instantiation - unable to retrieve asset names."));
			return;
		}
	}

	// Start ticking - this will poll the cooking system for completion.
	if(bStartTicking)
	{
		StartHoudiniTicking();
	}
}


void
UHoudiniAssetComponent::StartTaskAssetCookingManual()
{
	if(!IsInstantiatingOrCooking())
	{
		if(FHoudiniEngineUtils::IsValidAssetId(GetAssetId()))
		{
			StartTaskAssetCooking(true);
			bParametersChanged = true;
		}
		else
		{
			if(bLoadedComponent)
			{
				// This is a loaded component which requires instantiation.
				StartTaskAssetInstantiation(true, true);
				bParametersChanged = true;
			}
		}
	}
}


void
UHoudiniAssetComponent::StartTaskAssetResetManual()
{
	if(!IsInstantiatingOrCooking())
	{
		if(FHoudiniEngineUtils::IsValidAssetId(GetAssetId()))
		{
			if(FHoudiniEngineUtils::SetAssetPreset(GetAssetId(), DefaultPresetBuffer))
			{
				UnmarkChangedParameters();
				StartTaskAssetCookingManual();
			}
		}
		else
		{
			if(bLoadedComponent)
			{
				// This is a loaded component which requires instantiation.
				bLoadedComponentRequiresInstantiation = true;
				bParametersChanged = true;

				// Replace serialized preset buffer with default preset buffer.
				PresetBuffer = DefaultPresetBuffer;
				StartHoudiniTicking();
			}
		}
	}
}


void
UHoudiniAssetComponent::StartTaskAssetRebuildManual()
{
	if(!IsInstantiatingOrCooking())
	{
		bool bInstantiate = false;

		if(FHoudiniEngineUtils::IsValidAssetId(AssetId))
		{
			if(FHoudiniEngineUtils::GetAssetPreset(AssetId, PresetBuffer))
			{
				// We need to delete the asset and request a new one.
				StartTaskAssetDeletion();

				// We need to instantiate.
				bInstantiate = true;
			}
		}
		else
		{
			if(bLoadedComponent)
			{
				// We need to instantiate.
				bInstantiate = true;
			}
		}

		if(bInstantiate)
		{
			HapiGUID = FGuid::NewGuid();

			// If this is a loaded component, then we just need to instantiate.
			bLoadedComponentRequiresInstantiation = true;
			bParametersChanged = true;

			StartHoudiniTicking();
		}
	}
}


void
UHoudiniAssetComponent::StartTaskAssetDeletion()
{
	if(FHoudiniEngineUtils::IsValidAssetId(AssetId) && bIsNativeComponent)
	{
		// Generate GUID for our new task.
		FGuid HapiDeletionGUID = FGuid::NewGuid();

		// Create asset deletion task object and submit it for processing.
		FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetDeletion, HapiDeletionGUID);
		Task.AssetId = AssetId;
		FHoudiniEngine::Get().AddTask(Task);

		// Reset asset id
		AssetId = -1;

		// We do not need to tick as we are not interested in result.
	}
}


void
UHoudiniAssetComponent::StartTaskAssetCooking(bool bStartTicking)
{
	if(!IsInstantiatingOrCooking())
	{
		// Generate GUID for our new task.
		HapiGUID = FGuid::NewGuid();

		FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetCooking, HapiGUID);
		Task.ActorName = GetOuter()->GetName();
		Task.AssetComponent = this;
		FHoudiniEngine::Get().AddTask(Task);

		if(bStartTicking)
		{
			StartHoudiniTicking();
		}
	}
}


void
UHoudiniAssetComponent::ResetHoudiniResources()
{
	if(HapiGUID.IsValid())
	{
		// If we have a valid task GUID.
		FHoudiniEngineTaskInfo TaskInfo;

		if(FHoudiniEngine::Get().RetrieveTaskInfo(HapiGUID, TaskInfo))
		{
			FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
			HapiGUID.Invalidate();
			StopHoudiniTicking();

			// Get settings.
			const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

			// Check whether we want to display Slate cooking and instantiation notifications.
			bool bDisplaySlateCookingNotifications = false;
			if(HoudiniRuntimeSettings)
			{
				bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;
			}

			if(NotificationPtr.IsValid() && bDisplaySlateCookingNotifications)
			{
				TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
				if(NotificationItem.IsValid())
				{
					NotificationItem->ExpireAndFadeout();
					NotificationPtr.Reset();
				}
			}
		}
	}

	// Start asset deletion.
	StartTaskAssetDeletion();
}


void
UHoudiniAssetComponent::SubscribeEditorDelegates()
{
	// Add begin and end delegates for play-in-editor.
	DelegateHandleBeginPIE = FEditorDelegates::BeginPIE.AddUObject(this, &UHoudiniAssetComponent::OnPIEEventBegin);
	DelegateHandleEndPIE = FEditorDelegates::EndPIE.AddUObject(this, &UHoudiniAssetComponent::OnPIEEventEnd);

	// Add delegate for asset post import.
	DelegateHandleAssetPostImport =
		FEditorDelegates::OnAssetPostImport.AddUObject(this, &UHoudiniAssetComponent::OnAssetPostImport);

	// Add delegate for viewport drag and drop events.
	DelegateHandleApplyObjectToActor =
		FEditorDelegates::OnApplyObjectToActor.AddUObject(this, &UHoudiniAssetComponent::OnApplyObjectToActor);
}


void
UHoudiniAssetComponent::UnsubscribeEditorDelegates()
{
	// Remove begin and end delegates for play-in-editor.
	FEditorDelegates::BeginPIE.Remove(DelegateHandleBeginPIE);
	FEditorDelegates::EndPIE.Remove(DelegateHandleEndPIE);

	// Remove delegate for asset post import.
	FEditorDelegates::OnAssetPostImport.Remove(DelegateHandleAssetPostImport);

	// Remove delegate for viewport drag and drop events.
	FEditorDelegates::OnApplyObjectToActor.Remove(DelegateHandleApplyObjectToActor);
}


void
UHoudiniAssetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(!bIsNativeComponent)
	{
		return;
	}

	UProperty* Property = PropertyChangedEvent.MemberProperty;

	if(!Property)
	{
		return;
	}

	if(Property->GetName() == TEXT("Mobility"))
	{
		// Mobility was changed, we need to update it for all attached components as well.
		for(TArray<USceneComponent*>::TIterator Iter(AttachChildren); Iter; ++Iter)
		{
			USceneComponent* SceneComponent = *Iter;
			SceneComponent->SetMobility(Mobility);
		}

		return;
	}
	else if(Property->GetName() == TEXT("bVisible"))
	{
		// Visibility has changed, propagate it to children.
		SetVisibility(bVisible, true);
		return;
	}

	if(Property->HasMetaData(TEXT("Category")))
	{
		const FString& Category = Property->GetMetaData(TEXT("Category"));
		static const FString CategoryHoudiniGeneratedStaticMeshSettings = TEXT("HoudiniGeneratedStaticMeshSettings");
		static const FString CategoryLighting = TEXT("Lighting");

		if(CategoryHoudiniGeneratedStaticMeshSettings == Category)
		{
			// We are changing one of the mesh generation properties, we need to update all static meshes.
			for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator Iter(StaticMeshComponents); Iter; ++Iter)
			{
				UStaticMesh* StaticMesh = Iter.Key();

				SetStaticMeshGenerationParameters(StaticMesh);
				FHoudiniScopedGlobalSilence ScopedGlobalSilence;
				StaticMesh->Build(true);
				RefreshCollisionChange(StaticMesh);
			}

			return;
		}
		else if(CategoryLighting == Category)
		{
			if(Property->GetName() == TEXT("CastShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, CastShadow);
			}
			else if(Property->GetName() == TEXT("bCastDynamicShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastDynamicShadow);
			}
			else if(Property->GetName() == TEXT("bCastStaticShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastStaticShadow);
			}
			else if(Property->GetName() == TEXT("bCastVolumetricTranslucentShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastVolumetricTranslucentShadow);
			}
			else if(Property->GetName() == TEXT("bCastInsetShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastInsetShadow);
			}
			else if(Property->GetName() == TEXT("bCastHiddenShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastHiddenShadow);
			}
			else if(Property->GetName() == TEXT("bCastShadowAsTwoSided"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastShadowAsTwoSided);
			}
			else if(Property->GetName() == TEXT("bLightAsIfStatic"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bLightAsIfStatic);
			}
			else if(Property->GetName() == TEXT("bLightAttachmentsAsGroup"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bLightAttachmentsAsGroup);
			}
			else if(Property->GetName() == TEXT("IndirectLightingCacheQuality"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, IndirectLightingCacheQuality);
			}
		}
	}
}


void
UHoudiniAssetComponent::RemoveAllAttachedComponents()
{
	while(AttachChildren.Num() != 0)
	{
		USceneComponent* Component = AttachChildren[AttachChildren.Num() - 1];

		Component->DetachFromParent();
		Component->UnregisterComponent();
		Component->DestroyComponent();
	}

	AttachChildren.Empty();
}


void
UHoudiniAssetComponent::OnComponentClipboardCopy(UHoudiniAssetComponent* HoudiniAssetComponent)
{
	// Store copied component.
	CopiedHoudiniComponent = HoudiniAssetComponent;

	if(bIsNativeComponent)
	{
		// This component has been loaded.
		bLoadedComponent = true;
	}

	// Mark this component as imported.
	bComponentCopyImported = true;
}


void
UHoudiniAssetComponent::OnPIEEventBegin(const bool bIsSimulating)
{
	// We are now in PIE mode.
	bIsPlayModeActive = true;
}


void
UHoudiniAssetComponent::OnPIEEventEnd(const bool bIsSimulating)
{
	// We are no longer in PIE mode.
	bIsPlayModeActive = false;
}


void
UHoudiniAssetComponent::OnAssetPostImport(UFactory* Factory, UObject* Object)
{
	if(bComponentCopyImported && CopiedHoudiniComponent)
	{
		// Show busy cursor.
		FScopedBusyCursor ScopedBusyCursor;

		// Get original asset id.
		HAPI_AssetId CopiedHoudiniComponentAssetId = CopiedHoudiniComponent->AssetId;

		// Set Houdini asset.
		HoudiniAsset = CopiedHoudiniComponent->HoudiniAsset;

		// Copy preset buffer.
		if(FHoudiniEngineUtils::IsValidAssetId(CopiedHoudiniComponentAssetId))
		{
			FHoudiniEngineUtils::GetAssetPreset(CopiedHoudiniComponentAssetId, PresetBuffer);
		}
		else
		{
			PresetBuffer = CopiedHoudiniComponent->PresetBuffer;
		}

		// Copy default preset buffer.
		DefaultPresetBuffer = CopiedHoudiniComponent->DefaultPresetBuffer;

		// Clean up all generated and auto-attached components.
		RemoveAllAttachedComponents();

		// Release static mesh related resources.
		ReleaseObjectGeoPartResources(StaticMeshes);
		StaticMeshes.Empty();
		StaticMeshComponents.Empty();

		// Copy parameters.
		{
			ClearParameters();
			CopiedHoudiniComponent->DuplicateParameters(this);
		}

		// Copy inputs.
		{
			ClearInputs();
			CopiedHoudiniComponent->DuplicateInputs(this);
		}

		// Copy instance inputs.
		{
			ClearInstanceInputs();
			CopiedHoudiniComponent->DuplicateInstanceInputs(this);
		}

		// We need to reconstruct geometry from copied actor.
		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(CopiedHoudiniComponent->StaticMeshes);
			Iter; ++Iter)
		{
			FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
			UStaticMesh* StaticMesh = Iter.Value();

			// Duplicate static mesh and all related generated Houdini materials and textures.
			UStaticMesh* DuplicatedStaticMesh = 
				FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(StaticMesh, this, HoudiniGeoPartObject);

			if(DuplicatedStaticMesh)
			{
				// Store this duplicated mesh.
				StaticMeshes.Add(FHoudiniGeoPartObject(HoudiniGeoPartObject, true), DuplicatedStaticMesh);
			}
		}

		// We need to reconstruct splines.
		for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator
			Iter(CopiedHoudiniComponent->SplineComponents); Iter; ++Iter)
		{
			FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
			UHoudiniSplineComponent* HoudiniSplineComponent = Iter.Value();
			
			// Duplicate spline component.
			UHoudiniSplineComponent* DuplicatedSplineComponent = 
				DuplicateObject<UHoudiniSplineComponent>(HoudiniSplineComponent, this);

			if(DuplicatedSplineComponent)
			{
				DuplicatedSplineComponent->SetFlags(RF_Transactional | RF_Public);
				SplineComponents.Add(HoudiniGeoPartObject, DuplicatedSplineComponent);
			}
		}

		// Copy material information.
		HoudiniAssetComponentMaterials = 
			DuplicateObject<UHoudiniAssetComponentMaterials>(CopiedHoudiniComponent->HoudiniAssetComponentMaterials,
				this);

		// Perform any necessary post loading.
		PostLoad();

		DuplicateHandles(CopiedHoudiniComponent);

		// Mark this component as no longer copy imported and reset copied component.
		bComponentCopyImported = false;
		CopiedHoudiniComponent = nullptr;
	}
}


void
UHoudiniAssetComponent::OnApplyObjectToActor(UObject* ObjectToApply, AActor* ActorToApplyTo)
{
	if(GetHoudiniAssetActorOwner() == ActorToApplyTo)
	{
		// We want to handle material replacements.
		UMaterial* Material = Cast<UMaterial>(ObjectToApply);
		if(Material)
		{
			bool bMaterialReplaced = false;

			TMap<UStaticMesh*, int32> MaterialReplacementsMap;

			// We need to detect which components have material overriden, and replace it on their corresponding
			// generated static meshes.
			for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator Iter(StaticMeshComponents); Iter; ++Iter)
			{
				UStaticMesh* StaticMesh = Iter.Key();
				UStaticMeshComponent* StaticMeshComponent = Iter.Value();

				if(StaticMeshComponent && StaticMesh)
				{
					const TArray<class UMaterialInterface*>& OverrideMaterials = StaticMeshComponent->OverrideMaterials;
					for(int32 MaterialIdx = 0; MaterialIdx < OverrideMaterials.Num(); ++MaterialIdx)
					{
						UMaterialInterface* OverridenMaterial = OverrideMaterials[MaterialIdx];
						if(OverridenMaterial && OverridenMaterial == Material)
						{
							if(MaterialIdx < StaticMesh->Materials.Num())
							{
								MaterialReplacementsMap.Add(StaticMesh, MaterialIdx);
							}
						}
					}
				}
			}

			for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator Iter(InstanceInputs); Iter; ++Iter)
			{
				UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Iter.Value();
				if(HoudiniAssetInstanceInput)
				{
					HoudiniAssetInstanceInput->GetMaterialReplacementMeshes(Material, MaterialReplacementsMap);
				}
			}

			if(MaterialReplacementsMap.Num() > 0)
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
					LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Replacement"), this);

				for(TMap<UStaticMesh*, int32>::TIterator Iter(MaterialReplacementsMap); Iter; ++Iter)
				{
					UStaticMesh* StaticMesh = Iter.Key();
					int32 MaterialIdx = Iter.Value();

					// Get old material.
					UMaterialInterface* OldMaterial = StaticMesh->Materials[MaterialIdx];

					// Locate geo part object.
					FHoudiniGeoPartObject HoudiniGeoPartObject = LocateGeoPartObject(StaticMesh);
					if(HoudiniGeoPartObject.IsValid())
					{
						if(ReplaceMaterial(HoudiniGeoPartObject, Material, OldMaterial, MaterialIdx))
						{
							StaticMesh->Modify();
							StaticMesh->Materials[MaterialIdx] = Material;

							StaticMesh->PreEditChange(nullptr);
							StaticMesh->PostEditChange();
							StaticMesh->MarkPackageDirty();

							UStaticMeshComponent* StaticMeshComponent = LocateStaticMeshComponent(StaticMesh);
							if(StaticMeshComponent)
							{
								StaticMeshComponent->Modify();
								StaticMeshComponent->SetMaterial(MaterialIdx, Material);

								bMaterialReplaced = true;
							}

							TArray<UInstancedStaticMeshComponent*> InstancedStaticMeshComponents;
							if(LocateInstancedStaticMeshComponents(StaticMesh, InstancedStaticMeshComponents))
							{
								for(int32 Idx = 0; Idx < InstancedStaticMeshComponents.Num(); ++Idx)
								{
									UInstancedStaticMeshComponent* InstancedStaticMeshComponent =
										InstancedStaticMeshComponents[Idx];

									if(InstancedStaticMeshComponent)
									{
										InstancedStaticMeshComponent->Modify();
										InstancedStaticMeshComponent->SetMaterial(MaterialIdx, Material);

										bMaterialReplaced = true;
									}
								}
							}
						}
					}
				}

				if(bMaterialReplaced)
				{
					UpdateEditorProperties(false);
				}
			}
		}
	}
}


void
UHoudiniAssetComponent::CreateDefaultPreset()
{
	if(!bLoadedComponent)
	{
		if(!FHoudiniEngineUtils::GetAssetPreset(GetAssetId(), DefaultPresetBuffer))
		{
			DefaultPresetBuffer.Empty();
		}
	}
}

#endif


FBoxSphereBounds
UHoudiniAssetComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds Bounds;

	if(AttachChildren.Num() == 0)
	{
		Bounds = FBoxSphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX,
			FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));
	}
	else
	{
		if(AttachChildren[0])
		{
			Bounds = AttachChildren[0]->CalcBounds(LocalToWorld);
		}
	}

	for(int32 Idx = 1; Idx < AttachChildren.Num(); ++Idx)
	{
		if(AttachChildren[Idx])
		{
			Bounds = Bounds + AttachChildren[Idx]->CalcBounds(LocalToWorld);
		}
	}

	return Bounds;
}


void
UHoudiniAssetComponent::OnUpdateTransform(bool bSkipPhysicsMove, ETeleportType Teleport)
{
	Super::OnUpdateTransform(bSkipPhysicsMove, Teleport);

#if WITH_EDITOR

	// Only if asset has been cooked.
	if(AssetCookCount > 0)
	{
		// If we have to upload transforms.
		if(bUploadTransformsToHoudiniEngine)
		{
			// Retrieve the current component-to-world transform for this component.
			if(!FHoudiniEngineUtils::HapiSetAssetTransform(AssetId, GetComponentTransform()))
			{
				HOUDINI_LOG_MESSAGE(TEXT("Failed Uploading Transformation change back to HAPI."));
			}
		}

		// If transforms trigger cooks, we need to schedule one.
		if(bTransformChangeTriggersCooks)
		{
			bComponentTransformHasChanged = true;
			StartHoudiniTicking();
		}
	}

#endif

}


void
UHoudiniAssetComponent::UpdateRenderingInformation()
{
	// Need to send this to render thread at some point.
	MarkRenderStateDirty();

	// Update physics representation right away.
	RecreatePhysicsState();
	for(TArray<USceneComponent*>::TIterator Iter(AttachChildren); Iter; ++Iter)
	{
		USceneComponent* SceneComponent = *Iter;
		SceneComponent->RecreatePhysicsState();
	}

	// Since we have new asset, we need to update bounds.
	UpdateBounds();
}


void
UHoudiniAssetComponent::PostLoadReattachComponents()
{
	for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator Iter(StaticMeshComponents); Iter; ++Iter)
	{
		UStaticMeshComponent* StaticMeshComponent = Iter.Value();
		if(StaticMeshComponent)
		{
			StaticMeshComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);
		}
	}

	for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter(SplineComponents); Iter; ++Iter)
	{
		UHoudiniSplineComponent* HoudiniSplineComponent = Iter.Value();
		if(HoudiniSplineComponent)
		{
			HoudiniSplineComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);
		}
	}

	for(TMap<FString, UHoudiniHandleComponent*>::TIterator Iter(HandleComponents); Iter; ++Iter)
	{
		UHoudiniHandleComponent* HoudiniHandleComponent = Iter.Value();
		if(HoudiniHandleComponent)
		{
			HoudiniHandleComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);
		}
	}
}


void
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	Super::OnComponentCreated();

	// Create Houdini logo static mesh and component for it.
	CreateStaticMeshHoudiniLogoResource(StaticMeshes);

	// Create replacement material object.
	if(!HoudiniAssetComponentMaterials)
	{
		HoudiniAssetComponentMaterials =
			NewObject<UHoudiniAssetComponentMaterials>(this, UHoudiniAssetComponentMaterials::StaticClass(),
				NAME_None, RF_Transactional);
	}

#if WITH_EDITOR

	// Subscribe to delegates.
	SubscribeEditorDelegates();

#endif
}


void
UHoudiniAssetComponent::BeginDestroy()
{
	Super::BeginDestroy();
}


void
UHoudiniAssetComponent::OnComponentDestroyed()
{
	// Release static mesh related resources.
	ReleaseObjectGeoPartResources(StaticMeshes);
	StaticMeshes.Empty();
	StaticMeshComponents.Empty();

	// Release all curve related resources.
	ClearCurves();

	// Destroy all parameters.
	ClearParameters();

	// Destroy all inputs.
	ClearInputs();

	// Destroy all instance inputs.
	ClearInstanceInputs();

	// Destroy all handles.
	ClearHandles();

	// Inform downstream assets that we are dieing.
	ClearDownstreamAssets();

#if WITH_EDITOR

	// Release all Houdini related resources.
	ResetHoudiniResources();

	// Unsubscribe from Editor events.
	UnsubscribeEditorDelegates();

#endif

	Super::OnComponentDestroyed();
}


void
UHoudiniAssetComponent::OnRegister()
{
	Super::OnRegister();

	// We need to recreate render states for loaded components.
	if(bLoadedComponent)
	{
		// Static meshes.
		for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator Iter(StaticMeshComponents); Iter; ++Iter)
		{
			UStaticMeshComponent* StaticMeshComponent = Iter.Value();
			if(StaticMeshComponent)
			{
				// Recreate render state.
				StaticMeshComponent->RecreateRenderState_Concurrent();

				// Need to recreate physics state.
				StaticMeshComponent->RecreatePhysicsState();
			}
		}

		// Instanced static meshes.
		for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator Iter(InstanceInputs); Iter; ++Iter)
		{
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Iter.Value();

			// Recreate render state.
			HoudiniAssetInstanceInput->RecreateRenderStates();

			// Recreate physics state.
			HoudiniAssetInstanceInput->RecreatePhysicsStates();
		}
	}
}


void
UHoudiniAssetComponent::OnUnregister()
{
	Super::OnUnregister();
}


bool
UHoudiniAssetComponent::ContainsHoudiniLogoGeometry() const
{
	return bContainsHoudiniLogoGeometry;
}


void
UHoudiniAssetComponent::CreateStaticMeshHoudiniLogoResource(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap)
{
	if(!bIsNativeComponent)
	{
		return;
	}

	// Create Houdini logo static mesh and component for it.
	FHoudiniGeoPartObject HoudiniGeoPartObject;
	StaticMeshMap.Add(HoudiniGeoPartObject, FHoudiniEngine::Get().GetHoudiniLogoStaticMesh());
	CreateObjectGeoPartResources(StaticMeshMap);
	bContainsHoudiniLogoGeometry = true;
}


void
UHoudiniAssetComponent::PostLoad()
{
	Super::PostLoad();

	// We loaded a component which has no asset associated with it.
	if(!HoudiniAsset)
	{
		// Set geometry to be Houdini logo geometry, since we have no other geometry.
		CreateStaticMeshHoudiniLogoResource(StaticMeshes);
		return;
	}

#if WITH_EDITOR

	// Show busy cursor.
	FScopedBusyCursor ScopedBusyCursor;

#endif

	if(StaticMeshes.Num() > 0)
	{
		CreateObjectGeoPartResources(StaticMeshes);
	}
	else
	{
		CreateStaticMeshHoudiniLogoResource(StaticMeshes);
	}

	// Perform post load initialization on parameters.
	PostLoadInitializeParameters();

	// Perform post load initialization on instance inputs.
	PostLoadInitializeInstanceInputs();

	// Post attach components to parent asset component.
	PostLoadReattachComponents();

	// Update static mobility.
	if(EComponentMobility::Static == Mobility)
	{
		for(TArray<USceneComponent*>::TIterator Iter(AttachChildren); Iter; ++Iter)
		{
			USceneComponent* SceneComponent = *Iter;
			SceneComponent->SetMobility(EComponentMobility::Static);
		}
	}

	// Need to update rendering information.
	UpdateRenderingInformation();

#if WITH_EDITOR

	// Force editor to redraw viewports.
	if(GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Update properties panel after instantiation.
	UpdateEditorProperties(false);

#endif
}


void
UHoudiniAssetComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!Ar.IsSaving() && !Ar.IsLoading())
	{
		return;
	}

	// Serialize component flags.
	Ar << HoudiniAssetComponentFlagsPacked;

	// State of this component.
	EHoudiniAssetComponentState::Enum ComponentState = EHoudiniAssetComponentState::Invalid;

	if(Ar.IsSaving())
	{
		if(FHoudiniEngineUtils::IsValidAssetId(AssetId))
		{
			// Asset has been previously instantiated.

			if(HapiGUID.IsValid())
			{
				// Asset is being re-cooked asynchronously.
				ComponentState = EHoudiniAssetComponentState::BeingCooked;
			}
			else
			{
				// We have no pending asynchronous cook requests.
				ComponentState = EHoudiniAssetComponentState::Instantiated;
			}
		}
		else
		{
			if(HoudiniAsset)
			{
				// Asset has not been instantiated and therefore must have asynchronous instantiation
				// request in progress.
				ComponentState = EHoudiniAssetComponentState::None;
			}
			else
			{
				// Component is in invalid state (for example is a default class object).
				ComponentState = EHoudiniAssetComponentState::Invalid;
			}
		}
	}

	// Serialize format version.
	HoudiniAssetComponentVersion = VER_HOUDINI_ENGINE_COMPONENT_AUTOMATIC_VERSION;
	Ar << HoudiniAssetComponentVersion;

	// Serialize component state.
	SerializeEnumeration<EHoudiniAssetComponentState::Enum>(Ar, ComponentState);

	// Serialize scaling information and import axis.
	Ar << GeneratedGeometryScaleFactor;
	Ar << TransformScaleFactor;
	SerializeEnumeration<EHoudiniRuntimeSettingsAxisImport>(Ar, ImportAxis);

	// Serialize generated component GUID.
	Ar << ComponentGUID;

	// If component is in invalid state, we can skip the rest of serialization.
	if(EHoudiniAssetComponentState::Invalid == ComponentState)
	{
		return;
	}

	// If we have no asset, we can stop.
	if(!HoudiniAsset && Ar.IsSaving())
	{
		return;
	}

	// Serialize Houdini asset.
	UHoudiniAsset* HoudiniSerializedAsset = nullptr;

	if(Ar.IsSaving())
	{
		HoudiniSerializedAsset = HoudiniAsset;
	}

	Ar << HoudiniSerializedAsset;

	if(Ar.IsLoading())
	{
		if(Ar.IsTransacting() && HoudiniAsset != HoudiniSerializedAsset)
		{
			bTransactionAssetChange = true;
			PreviousTransactionHoudiniAsset = HoudiniAsset;
		}

		HoudiniAsset = HoudiniSerializedAsset;
	}

	// If we are going into playmode, save asset id.
	if(bIsPlayModeActive)
	{
		// Store or restore asset id.
		Ar << AssetId;
	}

	// Serialization of default preset.
	Ar << DefaultPresetBuffer;

	// Serialization of preset.
	{
		bool bPresetSaved = false;

		if(Ar.IsSaving() && FHoudiniEngineUtils::IsValidAssetId(AssetId))
		{
			if(FHoudiniEngineUtils::GetAssetPreset(AssetId, PresetBuffer))
			{
				bPresetSaved = true;
			}
		}

		Ar << bPresetSaved;

		if(bPresetSaved)
		{
			Ar << PresetBuffer;

			if(Ar.IsSaving())
			{
				// We no longer need preset buffer.
				PresetBuffer.Empty();
			}
		}
	}

	// Serialize parameters.
	Ar << Parameters;

	// Serialize parameters name map.
	if(HoudiniAssetComponentVersion >= VER_HOUDINI_ENGINE_COMPONENT_PARAMETER_NAME_MAP)
	{
		Ar << ParameterByName;
	}
	else
	{
		if(Ar.IsLoading())
		{
			ParameterByName.Empty();

			// Otherwise if we are loading an older serialization format, we can reconstruct parameters name map.
			for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
			{
				UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
				if(HoudiniAssetParameter)
				{
					ParameterByName.Add(HoudiniAssetParameter->GetParameterName(), HoudiniAssetParameter);
				}
			}
		}
	}

	// Serialize inputs.
	SerializeInputs(Ar);

	// Serialize material replacements and material assignments.
	Ar << HoudiniAssetComponentMaterials;

	// Serialize geo parts and generated static meshes.
	Ar << StaticMeshes;
	Ar << StaticMeshComponents;

	// Serialize instance inputs (we do this after geometry loading as we might need it).
	SerializeInstanceInputs(Ar);

	// Serialize curves.
	Ar << SplineComponents;

	// Serialize handles.
	Ar << HandleComponents;

	// Serialize downstream asset connections.
	Ar << DownstreamAssetConnections;

	if(Ar.IsLoading() && bIsNativeComponent)
	{
		// This component has been loaded.
		bLoadedComponent = true;
	}
}


#if WITH_EDITOR


void
UHoudiniAssetComponent::SetStaticMeshGenerationParameters(UStaticMesh* StaticMesh)
{
	if(StaticMesh)
	{
		// Make sure static mesh has a new lighting guid.
		StaticMesh->LightingGuid = FGuid::NewGuid();
		StaticMesh->LODGroup = NAME_None;

		// Set resolution of lightmap.
		StaticMesh->LightMapResolution = GeneratedLightMapResolution;

		// Set Bias multiplier for Light Propagation Volume lighting.
		StaticMesh->LpvBiasMultiplier = GeneratedLpvBiasMultiplier;

		// Set light map coordinate index.
		StaticMesh->LightMapCoordinateIndex = GeneratedLightMapCoordinateIndex;

		// Set method for LOD texture factor computation.
		StaticMesh->bUseMaximumStreamingTexelRatio = bGeneratedUseMaximumStreamingTexelRatio;

		// Set distance where textures using UV 0 are streamed in/out.
		StaticMesh->StreamingDistanceMultiplier = GeneratedStreamingDistanceMultiplier;

		// Add user data.
		for(int32 AssetUserDataIdx = 0; AssetUserDataIdx < GeneratedAssetUserData.Num(); ++AssetUserDataIdx)
		{
			StaticMesh->AddAssetUserData(GeneratedAssetUserData[AssetUserDataIdx]);
		}

		StaticMesh->CreateBodySetup();
		UBodySetup* BodySetup = StaticMesh->BodySetup;
		check(BodySetup);

		// Set flag whether physics triangle mesh will use double sided faces when doing scene queries.
		BodySetup->bDoubleSidedGeometry = bGeneratedDoubleSidedGeometry;

		// Assign physical material for simple collision.
		BodySetup->PhysMaterial = GeneratedPhysMaterial;

		// Assign collision trace behavior.
		BodySetup->CollisionTraceFlag = GeneratedCollisionTraceFlag;

		// Assign walkable slope behavior.
		BodySetup->WalkableSlopeOverride = GeneratedWalkableSlopeOverride;

		// We want to use all of geometry for collision detection purposes.
		BodySetup->bMeshCollideAll = true;
	}
}


AActor*
UHoudiniAssetComponent::CloneComponentsAndCreateActor()
{
	// Display busy cursor.
	FScopedBusyCursor ScopedBusyCursor;

	ULevel* Level = GetHoudiniAssetActorOwner()->GetLevel();
	AActor* Actor = NewObject<AActor>(Level, NAME_None);
	Actor->AddToRoot();

	USceneComponent* RootComponent =
		NewObject<USceneComponent>(Actor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);

	RootComponent->Mobility = EComponentMobility::Movable;
	RootComponent->bVisualizeComponent = true;

	const FTransform& ComponentWorldTransform = GetComponentTransform();
	RootComponent->SetWorldLocationAndRotation(ComponentWorldTransform.GetLocation(),
		ComponentWorldTransform.GetRotation());

	Actor->SetRootComponent(RootComponent);
	Actor->AddInstanceComponent(RootComponent);

	RootComponent->RegisterComponent();

	// Duplicate static mesh components.
	{
		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshes); Iter; ++Iter)
		{
			FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
			UStaticMesh* StaticMesh = Iter.Value();

			// Retrieve referenced static mesh component.
			UStaticMeshComponent* const* FoundStaticMeshComponent = StaticMeshComponents.Find(StaticMesh);
			UStaticMeshComponent* StaticMeshComponent = nullptr;

			if(FoundStaticMeshComponent)
			{
				StaticMeshComponent = *FoundStaticMeshComponent;
			}
			else
			{
				continue;
			}

			// Bake the referenced static mesh.
			UStaticMesh* OutStaticMesh = FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(StaticMesh,
				this, HoudiniGeoPartObject, true);

			if(OutStaticMesh)
			{
				FAssetRegistryModule::AssetCreated(OutStaticMesh);
			}

			// Create static mesh component for baked mesh.
			UStaticMeshComponent* DuplicatedComponent =
				NewObject<UStaticMeshComponent>(Actor, UStaticMeshComponent::StaticClass(), NAME_None);

			Actor->AddInstanceComponent(DuplicatedComponent);

			DuplicatedComponent->SetStaticMesh(OutStaticMesh);
			DuplicatedComponent->SetVisibility(true);
			DuplicatedComponent->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);

			// If this is a collision geo, we need to make it invisible.
			if(HoudiniGeoPartObject.IsCollidable())
			{
				DuplicatedComponent->SetVisibility(false);
			}

			DuplicatedComponent->AttachTo(RootComponent);
			DuplicatedComponent->RegisterComponent();
		}
	}

	// Duplicate instanced static mesh components.
	{
		for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator
			IterInstanceInputs(InstanceInputs); IterInstanceInputs; ++IterInstanceInputs)
		{
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstanceInputs.Value();

			if(HoudiniAssetInstanceInput)
			{
				HoudiniAssetInstanceInput->CloneComponentsAndAttachToActor(Actor);
			}
		}
	}

	return Actor;
}


bool
UHoudiniAssetComponent::IsCookingEnabled() const
{
	return bEnableCooking;
}


void
UHoudiniAssetComponent::PreEditUndo()
{
	Super::PreEditUndo();
}


void
UHoudiniAssetComponent::PostEditUndo()
{
	Super::PostEditUndo();
}


void
UHoudiniAssetComponent::PostEditImport()
{
	Super::PostEditImport();

	AHoudiniAssetActor* CopiedActor = FHoudiniEngineUtils::LocateClipboardActor(TEXT(""));
	if(CopiedActor)
	{
		OnComponentClipboardCopy(CopiedActor->HoudiniAssetComponent);
	}
}

#endif


void
UHoudiniAssetComponent::PostInitProperties()
{
	Super::PostInitProperties();

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	if(HoudiniRuntimeSettings)
	{
		// Copy cooking defaults from settings.
		bEnableCooking = HoudiniRuntimeSettings->bEnableCooking;
		bUploadTransformsToHoudiniEngine = HoudiniRuntimeSettings->bUploadTransformsToHoudiniEngine;
		bTransformChangeTriggersCooks = HoudiniRuntimeSettings->bTransformChangeTriggersCooks;

		// Copy static mesh generation parameters from settings.
		bGeneratedDoubleSidedGeometry = HoudiniRuntimeSettings->bDoubleSidedGeometry;
		GeneratedPhysMaterial = HoudiniRuntimeSettings->PhysMaterial;
		GeneratedCollisionTraceFlag = HoudiniRuntimeSettings->CollisionTraceFlag;
		GeneratedLpvBiasMultiplier = HoudiniRuntimeSettings->LpvBiasMultiplier;
		GeneratedLightMapResolution = HoudiniRuntimeSettings->LightMapResolution;
		GeneratedLightMapCoordinateIndex = HoudiniRuntimeSettings->LightMapCoordinateIndex;
		bGeneratedUseMaximumStreamingTexelRatio = HoudiniRuntimeSettings->bUseMaximumStreamingTexelRatio;
		GeneratedStreamingDistanceMultiplier = HoudiniRuntimeSettings->StreamingDistanceMultiplier;
		GeneratedWalkableSlopeOverride = HoudiniRuntimeSettings->WalkableSlopeOverride;
		GeneratedFoliageDefaultSettings = HoudiniRuntimeSettings->FoliageDefaultSettings;
		GeneratedAssetUserData = HoudiniRuntimeSettings->AssetUserData;
	}
}


bool
UHoudiniAssetComponent::LocateStaticMeshes(const FString& ObjectName,
	TMap<FString, TArray<FHoudiniGeoPartObject> >& InOutObjectsToInstance, bool bSubstring) const
{
	// See if map has entry for this object name.
	if(!InOutObjectsToInstance.Contains(ObjectName))
	{
		InOutObjectsToInstance.Add(ObjectName, TArray<FHoudiniGeoPartObject>());
	}

	{
		// Get array entry for this object name.
		TArray<FHoudiniGeoPartObject>& Objects = InOutObjectsToInstance[ObjectName];

		// Go through all geo part objects and see if we have matches.
		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TConstIterator Iter(StaticMeshes); Iter; ++Iter)
		{
			const FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
			UStaticMesh* StaticMesh = Iter.Value();

			if(StaticMesh && ObjectName.Len() > 0)
			{
				if(bSubstring && ObjectName.Len() >= HoudiniGeoPartObject.ObjectName.Len())
				{
					int32 Index = ObjectName.Find(*HoudiniGeoPartObject.ObjectName, ESearchCase::IgnoreCase,
						ESearchDir::FromEnd, INDEX_NONE);

					if((Index != -1) && (Index + HoudiniGeoPartObject.ObjectName.Len() == ObjectName.Len()))
					{
						Objects.Add(HoudiniGeoPartObject);
					}
				}
				else if(HoudiniGeoPartObject.ObjectName.Equals(ObjectName))
				{
					Objects.Add(HoudiniGeoPartObject);
				}
			}
		}
	}

	// Sort arrays.
	for(TMap<FString, TArray<FHoudiniGeoPartObject> >::TIterator Iter(InOutObjectsToInstance); Iter; ++Iter)
	{
		TArray<FHoudiniGeoPartObject>& Objects = Iter.Value();
		Objects.Sort(FHoudiniGeoPartObjectSortPredicate());
	}

	return InOutObjectsToInstance.Num() > 0;
}


bool
UHoudiniAssetComponent::LocateStaticMeshes(int32 ObjectToInstanceId,
	TArray<FHoudiniGeoPartObject>& InOutObjectsToInstance) const
{
	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TConstIterator Iter(StaticMeshes); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
		UStaticMesh* StaticMesh = Iter.Value();

		if(StaticMesh && HoudiniGeoPartObject.ObjectId == ObjectToInstanceId)
		{
			InOutObjectsToInstance.Add(HoudiniGeoPartObject);
		}
	}

	// Sort array.
	InOutObjectsToInstance.Sort(FHoudiniGeoPartObjectSortPredicate());

	return InOutObjectsToInstance.Num() > 0;
}


FHoudiniGeoPartObject
UHoudiniAssetComponent::LocateGeoPartObject(UStaticMesh* StaticMesh) const
{
	FHoudiniGeoPartObject GeoPartObject;

	const FHoudiniGeoPartObject* FoundGeoPartObject = StaticMeshes.FindKey(StaticMesh);
	if(FoundGeoPartObject)
	{
		GeoPartObject = *FoundGeoPartObject;
	}

	return GeoPartObject;
}


bool
UHoudiniAssetComponent::IsPlayModeActive() const
{
	return bIsPlayModeActive;
}


#if WITH_EDITOR

void
UHoudiniAssetComponent::CreateCurves(const TArray<FHoudiniGeoPartObject>& FoundCurves)
{
	TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*> NewSplineComponents;
	for(TArray<FHoudiniGeoPartObject>::TConstIterator Iter(FoundCurves); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = *Iter;

		// Retrieve node id from geo part.
		HAPI_NodeId NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId(AssetId);
		if(-1 == NodeId)
		{
			// Invalid node id.
			continue;
		}

		if(!HoudiniGeoPartObject.HasParameters(AssetId))
		{
			// We have no parameters on this curve.
			continue;
		}

		FString CurvePointsString;
		EHoudiniSplineComponentType::Enum CurveTypeValue = EHoudiniSplineComponentType::Bezier;
		EHoudiniSplineComponentMethod::Enum CurveMethodValue = EHoudiniSplineComponentMethod::CVs;
		int32 CurveClosed = 1;

		HAPI_AttributeInfo AttributeRefinedCurvePositions;
		TArray<float> RefinedCurvePositions;
		if(!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_POSITION,
			AttributeRefinedCurvePositions, RefinedCurvePositions))
		{
			continue;
		}

		if(!AttributeRefinedCurvePositions.exists && RefinedCurvePositions.Num() > 0)
		{
			continue;
		}

		// Transfer refined positions to position vector and perform necessary axis swap.
		TArray<FVector> CurveDisplayPoints;
		FHoudiniEngineUtils::ConvertScaleAndFlipVectorData(RefinedCurvePositions, CurveDisplayPoints);

		if(!FHoudiniEngineUtils::HapiGetParameterDataAsString(NodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, TEXT(""),
				CurvePointsString) ||
		   !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_TYPE,
				(int32) EHoudiniSplineComponentType::Bezier, (int32&) CurveTypeValue) ||
		   !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_METHOD,
				(int32) EHoudiniSplineComponentMethod::CVs, (int32&) CurveMethodValue) ||
		   !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 1, CurveClosed))
		{
			continue;
		}

		// Process coords string and extract positions.
		TArray<FVector> CurvePoints;
		FHoudiniEngineUtils::ExtractStringPositions(CurvePointsString, CurvePoints);

		// Check if this curve already exists.
		UHoudiniSplineComponent* const* FoundHoudiniSplineComponent = SplineComponents.Find(HoudiniGeoPartObject);
		UHoudiniSplineComponent* HoudiniSplineComponent = nullptr;

		if(FoundHoudiniSplineComponent)
		{
			// Curve already exists, we can reuse it.
			HoudiniSplineComponent = *FoundHoudiniSplineComponent;

			// Remove it from old map.
			SplineComponents.Remove(HoudiniGeoPartObject);
		}
		else
		{
			// We need to create new curve.
			HoudiniSplineComponent = NewObject<UHoudiniSplineComponent>(this, UHoudiniSplineComponent::StaticClass(),
				NAME_None, RF_Public | RF_Transactional);
		}

		// If we have no parent, we need to re-attach.
		if(!HoudiniSplineComponent->AttachParent)
		{
			HoudiniSplineComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);
		}

		HoudiniSplineComponent->SetVisibility(true);

		// If component is not registered, register it.
		if(!HoudiniSplineComponent->IsRegistered())
		{
			HoudiniSplineComponent->RegisterComponent();
		}

		// Add to map of components.
		NewSplineComponents.Add(HoudiniGeoPartObject, HoudiniSplineComponent);

		// Transform the component by transformation provided by HAPI.
		HoudiniSplineComponent->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);

		// Construct curve from available data.
		HoudiniSplineComponent->Construct(HoudiniGeoPartObject, CurvePoints, CurveDisplayPoints, CurveTypeValue,
			CurveMethodValue, (CurveClosed == 1));
	}

	ClearCurves();
	SplineComponents = NewSplineComponents;
}


bool
UHoudiniAssetComponent::CreateParameters()
{
	if(!FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		// There's no Houdini asset, we can return.
		return true;
	}

	bool bTreatRampParametersAsMultiparms = false;
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if(HoudiniRuntimeSettings)
	{
		bTreatRampParametersAsMultiparms = HoudiniRuntimeSettings->bTreatRampParametersAsMultiparms;
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// Map of newly created and reused parameters.
	TMap<HAPI_ParmId, UHoudiniAssetParameter*> NewParameters;
	TMap<FString, UHoudiniAssetParameter*> NewParameterByName;

	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo),
		false);

	HAPI_NodeInfo NodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId,
		&NodeInfo), false );

	if(NodeInfo.parmCount > 0)
	{
		// Retrieve parameters.
		TArray<HAPI_ParmInfo> ParmInfos;
		ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &ParmInfos[0], 0,
				NodeInfo.parmCount), false);

		// Create properties for parameters.
		for(int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx)
		{
			// Retrieve param info at this index.
			const HAPI_ParmInfo& ParmInfo = ParmInfos[ParamIdx];

			// If parameter is invisible, skip it.
			if(ParmInfo.invisible)
			{
				continue;
			}

			// See if this parameter has already been created.
			UHoudiniAssetParameter* const* FoundHoudiniAssetParameter = Parameters.Find(ParmInfo.id);
			UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;

			// If parameter exists, we can reuse it.
			if(FoundHoudiniAssetParameter)
			{
				HoudiniAssetParameter = *FoundHoudiniAssetParameter;

				// Remove parameter from current map.
				Parameters.Remove(ParmInfo.id);

				// Reinitialize parameter and add it to map.
				HoudiniAssetParameter->CreateParameter(this, nullptr, AssetInfo.nodeId, ParmInfo);
				NewParameters.Add(ParmInfo.id, HoudiniAssetParameter);
				continue;
			}

			switch(ParmInfo.type)
			{
				case HAPI_PARMTYPE_STRING:
				{
					if(!ParmInfo.choiceCount)
					{
						HoudiniAssetParameter = UHoudiniAssetParameterString::Create(this, nullptr, AssetInfo.nodeId,
							ParmInfo);
					}
					else
					{
						HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(this, nullptr, AssetInfo.nodeId,
							ParmInfo);
					}

					break;
				}

				case HAPI_PARMTYPE_INT:
				{
					if(!ParmInfo.choiceCount)
					{
						HoudiniAssetParameter = UHoudiniAssetParameterInt::Create(this, nullptr, AssetInfo.nodeId,
							ParmInfo);
					}
					else
					{
						HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(this, nullptr, AssetInfo.nodeId,
							ParmInfo);
					}

					break;
				}

				case HAPI_PARMTYPE_FLOAT:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterFloat::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_TOGGLE:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterToggle::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_COLOR:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterColor::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_LABEL:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterLabel::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_BUTTON:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterButton::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_SEPARATOR:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterSeparator::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_FOLDERLIST:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterFolderList::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_FOLDER:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterFolder::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_MULTIPARMLIST:
				{
					if(!bTreatRampParametersAsMultiparms && (HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType ||
						HAPI_RAMPTYPE_COLOR == ParmInfo.rampType))
					{
						HoudiniAssetParameter = UHoudiniAssetParameterRamp::Create(this, nullptr,
							AssetInfo.nodeId, ParmInfo);
					}
					else
					{
						HoudiniAssetParameter = UHoudiniAssetParameterMultiparm::Create(this, nullptr,
							AssetInfo.nodeId, ParmInfo);
					}

					break;
				}

				case HAPI_PARMTYPE_PATH_FILE:
				case HAPI_PARMTYPE_PATH_FILE_GEO:
				case HAPI_PARMTYPE_PATH_FILE_IMAGE:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterFile::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_PATH_NODE:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterString::Create(this, nullptr, AssetInfo.nodeId,
						ParmInfo);
					break;
				}

				default:
				{
					// Just ignore unsupported types for now.
					continue;
				}
			}

			// Add this parameter to the map.
			NewParameters.Add(ParmInfo.id, HoudiniAssetParameter);
		}

		// We do another pass to patch parent links.
		for(int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx)
		{
			// Retrieve param info at this index.
			const HAPI_ParmInfo& ParmInfo = ParmInfos[ParamIdx];

			// Locate corresponding parameter.
			UHoudiniAssetParameter* const* FoundHoudiniAssetParameter = NewParameters.Find(ParmInfo.id);
			UHoudiniAssetParameter* HoudiniAssetParentParameter = nullptr;

			if(FoundHoudiniAssetParameter)
			{
				UHoudiniAssetParameter* HoudiniAssetParameter = *FoundHoudiniAssetParameter;

				// Get parent parm id.
				HAPI_ParmId ParentParmId = HoudiniAssetParameter->GetParmParentId();
				if(-1 != ParentParmId)
				{
					// Locate corresponding parent parameter.
					UHoudiniAssetParameter* const* FoundHoudiniAssetParentParameter = NewParameters.Find(ParentParmId);
					if(FoundHoudiniAssetParentParameter)
					{
						HoudiniAssetParentParameter = *FoundHoudiniAssetParentParameter;
					}
				}

				// Set parent for this parameter.
				HoudiniAssetParameter->SetParentParameter(HoudiniAssetParentParameter);

				if(HAPI_PARMTYPE_FOLDERLIST == ParmInfo.type)
				{
					// For folder lists we need to add children manually.
					HoudiniAssetParameter->ResetChildParameters();

					for(int32 ChildIdx = 0; ChildIdx < ParmInfo.size; ++ChildIdx)
					{
						// Children folder parm infos come after folder list parm info.
						const HAPI_ParmInfo& ChildParmInfo = ParmInfos[ParamIdx + ChildIdx + 1];

						UHoudiniAssetParameter* const* FoundHoudiniAssetParameterChild =
							NewParameters.Find(ChildParmInfo.id);

						if(FoundHoudiniAssetParameterChild)
						{
							UHoudiniAssetParameter* HoudiniAssetParameterChild = *FoundHoudiniAssetParameterChild;
							HoudiniAssetParameterChild->SetParentParameter(HoudiniAssetParameter);
						}
					}
				}
			}
		}

		// Another pass to notify parameters that all children parameters have been assigned and to populate name look
		// up map for faster querying.
		for(int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx)
		{
			// Retrieve param info at this index.
			const HAPI_ParmInfo& ParmInfo = ParmInfos[ParamIdx];

			// Locate corresponding parameter.
			UHoudiniAssetParameter* const* FoundHoudiniAssetParameter = NewParameters.Find(ParmInfo.id);
			UHoudiniAssetParameter* HoudiniAssetParentParameter = nullptr;

			if(FoundHoudiniAssetParameter)
			{
				UHoudiniAssetParameter* HoudiniAssetParameter = *FoundHoudiniAssetParameter;
				if(HoudiniAssetParameter->HasChildParameters())
				{
					HoudiniAssetParameter->NotifyChildParametersCreated();
				}

				// Add this parameter to parameter name look up map.
				NewParameterByName.Add(HoudiniAssetParameter->GetParameterName(), HoudiniAssetParameter);
			}
		}
	}

	// Remove all unused parameters.
	ClearParameters();

	// Update parameters.
	Parameters = NewParameters;
	ParameterByName = NewParameterByName;

	return true;
}


void
UHoudiniAssetComponent::NotifyParameterWillChange(UHoudiniAssetParameter* HoudiniAssetParameter)
{

}


void
UHoudiniAssetComponent::NotifyParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	if(bLoadedComponent && !FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		bLoadedComponentRequiresInstantiation = true;
	}

	bParametersChanged = true;
	StartHoudiniTicking();
}


void
UHoudiniAssetComponent::NotifyHoudiniSplineChanged(UHoudiniSplineComponent* HoudiniSplineComponent)
{
	if(bLoadedComponent && !FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		bLoadedComponentRequiresInstantiation = true;
	}

	bParametersChanged = true;
	StartHoudiniTicking();
}


void
UHoudiniAssetComponent::UnmarkChangedParameters()
{
	if(bParametersChanged)
	{
		for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

			// If parameter has changed, unmark it.
			if(HoudiniAssetParameter->HasChanged())
			{
				HoudiniAssetParameter->UnmarkChanged();
			}
		}
	}
}


void
UHoudiniAssetComponent::UploadChangedParameters()
{
	if(bParametersChanged)
	{
		// Upload inputs.
		for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(Inputs); IterInputs; ++IterInputs)
		{
			UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;

			// If input has changed, upload it to HAPI.
			if(HoudiniAssetInput->HasChanged())
			{
				HoudiniAssetInput->UploadParameterValue();
			}
		}

		// Upload parameters.
		for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

			// If parameter has changed, upload it to HAPI.
			if(HoudiniAssetParameter->HasChanged())
			{
				HoudiniAssetParameter->UploadParameterValue();
			}
		}
	}

	// We no longer have changed parameters.
	bParametersChanged = false;
}


void
UHoudiniAssetComponent::UpdateLoadedParameters()
{
	HAPI_AssetInfo AssetInfo;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo))
	{
		return;
	}

	for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		HoudiniAssetParameter->SetNodeId(AssetInfo.nodeId);
	}
}


bool
UHoudiniAssetComponent::CreateHandles()
{
	if(!FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		// There's no Houdini asset, we can return.
		return false;
	}

	HAPI_AssetInfo AssetInfo;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo))
	{
		return false;
	}

	FHandleComponentMap NewHandleComponents;

	// If we have handles.
	if(AssetInfo.handleCount > 0)
	{
		TArray<HAPI_HandleInfo> HandleInfos;
		HandleInfos.SetNumZeroed(AssetInfo.handleCount);

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetHandleInfo(FHoudiniEngine::Get().GetSession(), AssetId,
			&HandleInfos[0], 0, AssetInfo.handleCount))
		{
			return false;
		}

		for(int32 HandleIdx = 0; HandleIdx < AssetInfo.handleCount; ++HandleIdx)
		{
			// Retrieve handle info for this index.
			const HAPI_HandleInfo& HandleInfo = HandleInfos[HandleIdx];

			// If we do not have bindings, we can skip.
			if(HandleInfo.bindingsCount <= 0)
			{
				continue;
			}

			FString TypeName = TEXT("");

			{
				FHoudiniEngineString HoudiniEngineString(HandleInfo.typeNameSH);
				if(!HoudiniEngineString.ToFString(TypeName))
				{
					continue;
				}

				if(!TypeName.Equals(TEXT(HAPI_UNREAL_HANDLE_TRANSFORM)))
				{
					continue;
				}
			}


			FString HandleName = TEXT("");

			{
				FHoudiniEngineString HoudiniEngineString(HandleInfo.nameSH);
				if(!HoudiniEngineString.ToFString(HandleName))
				{
					continue;
				}
			}

			UHoudiniHandleComponent* HandleComponent = nullptr;
			UHoudiniHandleComponent** FoundHandleComponent = HandleComponents.Find(HandleName);

			if(FoundHandleComponent)
			{
				HandleComponent = *FoundHandleComponent;

				// Remove so that it's not destroyed.
				HandleComponents.Remove(HandleName);
			}
			else
			{
				HandleComponent =
					NewObject<UHoudiniHandleComponent>(this, UHoudiniHandleComponent::StaticClass(),
						NAME_None, RF_Public | RF_Transactional);
			}

			if(!HandleComponent)
			{
				continue;
			}

			// If we have no parent, we need to re-attach.
			if(!HandleComponent->AttachParent)
			{
				HandleComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);
			}

			HandleComponent->SetVisibility(true);

			// If component is not registered, register it.
			if(!HandleComponent->IsRegistered())
			{
				HandleComponent->RegisterComponent();
			}

			if (HandleComponent->Construct(AssetId, HandleIdx, HandleName, HandleInfo, Parameters))
			{
				NewHandleComponents.Add(HandleName, HandleComponent);
			}
		}
	}

	ClearHandles();
	HandleComponents = NewHandleComponents;

	return true;
}


void
UHoudiniAssetComponent::CreateInputs()
{
	if(!FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		// There's no Houdini asset, we can return.
		return;
	}

	// Inputs have been created already.
	if(Inputs.Num() > 0)
	{
		return;
	}

	HAPI_AssetInfo AssetInfo;
	int32 InputCount = 0;
	if((HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo))
		&& AssetInfo.hasEverCooked)
	{
		InputCount = AssetInfo.geoInputCount;
	}

	// Create inputs.
	Inputs.SetNumZeroed(InputCount);
	for(int32 InputIdx = 0; InputIdx < InputCount; ++InputIdx)
	{
		Inputs[InputIdx] = UHoudiniAssetInput::Create(this, InputIdx);
	}
}


void
UHoudiniAssetComponent::UpdateLoadedInputs()
{
	for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(Inputs); IterInputs; ++IterInputs)
	{
		UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;
		HoudiniAssetInput->UploadParameterValue();
	}
}


void
UHoudiniAssetComponent::UploadLoadedCurves()
{
	for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter(SplineComponents); Iter; ++Iter)
	{
		UHoudiniSplineComponent* HoudiniSplineComponent = Iter.Value();
		HoudiniSplineComponent->UploadControlPoints();
	}
}


void
UHoudiniAssetComponent::CreateInstanceInputs(const TArray<FHoudiniGeoPartObject>& Instancers)
{
	TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*> NewInstanceInputs;

	for(TArray<FHoudiniGeoPartObject>::TConstIterator Iter(Instancers); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = *Iter;

		// Check if this instance input already exists.
		UHoudiniAssetInstanceInput* const* FoundHoudiniAssetInstanceInput =
			InstanceInputs.Find(HoudiniGeoPartObject.ObjectId);
		UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = nullptr;

		if(FoundHoudiniAssetInstanceInput)
		{
			// Input already exists, we can reuse it.
			HoudiniAssetInstanceInput = *FoundHoudiniAssetInstanceInput;

			// Remove it from old map.
			InstanceInputs.Remove(HoudiniGeoPartObject.ObjectId);
		}
		else
		{
			// Otherwise we need to create new instance input.
			HoudiniAssetInstanceInput = UHoudiniAssetInstanceInput::Create(this, HoudiniGeoPartObject);
		}

		if(!HoudiniAssetInstanceInput)
		{
			// Invalid instance input.
			continue;
		}

		// Add input to new map.
		NewInstanceInputs.Add(HoudiniGeoPartObject.ObjectId, HoudiniAssetInstanceInput);

		// Create or re-create this input.
		HoudiniAssetInstanceInput->CreateInstanceInput();
	}

	ClearInstanceInputs();
	InstanceInputs = NewInstanceInputs;
}


void
UHoudiniAssetComponent::DuplicateParameters(UHoudiniAssetComponent* DuplicatedHoudiniComponent)
{
	TMap<HAPI_ParmId, UHoudiniAssetParameter*>& InParameters = DuplicatedHoudiniComponent->Parameters;
	TMap<FString, UHoudiniAssetParameter*>& InParametersByName = DuplicatedHoudiniComponent->ParameterByName;

	for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		HAPI_ParmId HoudiniAssetParameterKey = IterParams.Key();
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

		if(-1 != HoudiniAssetParameterKey)
		{
			// Duplicate parameter.
			UHoudiniAssetParameter* DuplicatedHoudiniAssetParameter =
				DuplicateObject(HoudiniAssetParameter, DuplicatedHoudiniComponent);

			// PIE does not like standalone flags.
			DuplicatedHoudiniAssetParameter->ClearFlags(RF_Standalone);

			DuplicatedHoudiniAssetParameter->SetHoudiniAssetComponent(DuplicatedHoudiniComponent);

			InParameters.Add(HoudiniAssetParameterKey, DuplicatedHoudiniAssetParameter);
			InParametersByName.Add(DuplicatedHoudiniAssetParameter->GetParameterName(),
				DuplicatedHoudiniAssetParameter);
		}
	}
}


void
UHoudiniAssetComponent::DuplicateHandles(UHoudiniAssetComponent* SrcAssetComponent)
{
	for(auto const& SrcNameToHandle : SrcAssetComponent->HandleComponents)
	{
		// Duplicate spline component.
		UHoudiniHandleComponent* NewHandleComponent =
			DuplicateObject<UHoudiniHandleComponent>(SrcNameToHandle.Value, this);

		if(NewHandleComponent)
		{
			NewHandleComponent->SetFlags(RF_Transactional | RF_Public);
			NewHandleComponent->ResolveDuplicatedParameters(Parameters);
			HandleComponents.Add(SrcNameToHandle.Key, NewHandleComponent);
		}
	}
}


void
UHoudiniAssetComponent::DuplicateInputs(UHoudiniAssetComponent* DuplicatedHoudiniComponent)
{
	TArray<UHoudiniAssetInput*>& InInputs = DuplicatedHoudiniComponent->Inputs;

	for(int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
	{
		// Retrieve input at this index.
		UHoudiniAssetInput* AssetInput = Inputs[InputIdx];

		// Duplicate input.
		UHoudiniAssetInput* DuplicatedAssetInput = DuplicateObject(AssetInput, DuplicatedHoudiniComponent);
		DuplicatedAssetInput->SetHoudiniAssetComponent(DuplicatedHoudiniComponent);

		// PIE does not like standalone flags.
		DuplicatedAssetInput->ClearFlags(RF_Standalone);

		InInputs.Add(DuplicatedAssetInput);
	}
}


void
UHoudiniAssetComponent::DuplicateInstanceInputs(UHoudiniAssetComponent* DuplicatedHoudiniComponent)
{
	TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>& InInstanceInputs = DuplicatedHoudiniComponent->InstanceInputs;

	for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator
		IterInstanceInputs(InstanceInputs); IterInstanceInputs; ++IterInstanceInputs)
	{
		HAPI_ObjectId HoudiniInstanceInputKey = IterInstanceInputs.Key();
		UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstanceInputs.Value();

		UHoudiniAssetInstanceInput* DuplicatedHoudiniAssetInstanceInput =
			UHoudiniAssetInstanceInput::Create(DuplicatedHoudiniComponent, HoudiniAssetInstanceInput);

		// PIE does not like standalone flags.
		DuplicatedHoudiniAssetInstanceInput->ClearFlags(RF_Standalone);

		InInstanceInputs.Add(HoudiniInstanceInputKey, DuplicatedHoudiniAssetInstanceInput);
	}
}

#endif

void
UHoudiniAssetComponent::ClearInstanceInputs()
{
	for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator
		IterInstanceInputs(InstanceInputs); IterInstanceInputs; ++IterInstanceInputs)
	{
		UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstanceInputs.Value();
		HoudiniAssetInstanceInput->ConditionalBeginDestroy();
	}

	InstanceInputs.Empty();
}


void
UHoudiniAssetComponent::ClearCurves()
{
	for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter(SplineComponents); Iter; ++Iter)
	{
		UHoudiniSplineComponent* SplineComponent = Iter.Value();

		SplineComponent->DetachFromParent();
		SplineComponent->UnregisterComponent();
		SplineComponent->DestroyComponent();

		// Remove this component from the list of attached components.
		AttachChildren.Remove(SplineComponent);
	}

	SplineComponents.Empty();
}


void
UHoudiniAssetComponent::ClearParameters()
{
	for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		HoudiniAssetParameter->ConditionalBeginDestroy();
	}

	Parameters.Empty();
	ParameterByName.Empty();
}


void
UHoudiniAssetComponent::ClearHandles()
{
	for(auto& NameToComponent : HandleComponents)
	{
		UHoudiniHandleComponent* HandleComponent = NameToComponent.Value;

		HandleComponent->DetachFromParent();
		HandleComponent->UnregisterComponent();
		HandleComponent->DestroyComponent();

		// Remove this component from the list of attached components.
		AttachChildren.Remove(HandleComponent);
	}

	HandleComponents.Empty();
}


void
UHoudiniAssetComponent::ClearInputs()
{
	for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(Inputs); IterInputs; ++IterInputs)
	{
		UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;

		// Destroy connected Houdini asset.
		HoudiniAssetInput->ConditionalBeginDestroy();
	}

	Inputs.Empty();
}


void
UHoudiniAssetComponent::ClearDownstreamAssets()
{
	for(TMap<UHoudiniAssetComponent*, TSet<int32>>::TIterator IterAssets(DownstreamAssetConnections);
		IterAssets; ++IterAssets)
	{
		UHoudiniAssetComponent* DownstreamAsset = IterAssets.Key();
		TSet<int32>& LocalInputIndicies = IterAssets.Value();
		for(auto LocalInputIndex : LocalInputIndicies)
		{
			DownstreamAsset->Inputs[LocalInputIndex]->ExternalDisconnectInputAssetActor();
		}
	}

	DownstreamAssetConnections.Empty();
}


UStaticMesh*
UHoudiniAssetComponent::LocateStaticMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const
{
	UStaticMesh* const* FoundStaticMesh = StaticMeshes.Find(HoudiniGeoPartObject);
	UStaticMesh* StaticMesh = nullptr;

	if(FoundStaticMesh)
	{
		StaticMesh = *FoundStaticMesh;
	}

	return StaticMesh;
}


UStaticMeshComponent*
UHoudiniAssetComponent::LocateStaticMeshComponent(UStaticMesh* StaticMesh) const
{
	UStaticMeshComponent* const* FoundStaticMeshComponent = StaticMeshComponents.Find(StaticMesh);
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	if(FoundStaticMeshComponent)
	{
		StaticMeshComponent = *FoundStaticMeshComponent;
	}

	return StaticMeshComponent;
}


bool
UHoudiniAssetComponent::LocateInstancedStaticMeshComponents(UStaticMesh* StaticMesh,
	TArray<UInstancedStaticMeshComponent*>& Components)
{
	Components.Empty();

	bool bResult = false;

	for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator Iter(InstanceInputs); Iter; ++Iter)
	{
		UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Iter.Value();
		if(HoudiniAssetInstanceInput)
		{
			bResult |= HoudiniAssetInstanceInput->CollectAllInstancedStaticMeshComponents(Components, StaticMesh);
		}
	}

	return bResult;
}


void
UHoudiniAssetComponent::SerializeInputs(FArchive& Ar)
{
	if(Ar.IsLoading())
	{
		if(!Ar.IsTransacting())
		{
			ClearInputs();
		}
	}

	Ar << Inputs;

	if(Ar.IsLoading())
	{
		if(!Ar.IsTransacting())
		{
			for(int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
			{
				Inputs[InputIdx]->SetHoudiniAssetComponent(this);
			}
		}
	}
}


void
UHoudiniAssetComponent::SerializeInstanceInputs(FArchive& Ar)
{
	if(Ar.IsLoading())
	{
		ClearInstanceInputs();
	}

	// Serialize number of instance inputs.
	int32 InstanceInputCount = InstanceInputs.Num();
	Ar << InstanceInputCount;

	if(Ar.IsSaving())
	{
		for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator
			IterInstanceInputs(InstanceInputs); IterInstanceInputs; ++IterInstanceInputs)
		{
			HAPI_ObjectId HoudiniInstanceInputKey = IterInstanceInputs.Key();
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstanceInputs.Value();

			Ar << HoudiniInstanceInputKey;
			Ar << HoudiniAssetInstanceInput;
		}
	}
	else if(Ar.IsLoading())
	{
		for(int32 InstanceInputIdx = 0; InstanceInputIdx < InstanceInputCount; ++InstanceInputIdx)
		{
			HAPI_ObjectId HoudiniInstanceInputKey = -1;
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = nullptr;

			Ar << HoudiniInstanceInputKey;
			Ar << HoudiniAssetInstanceInput;

			HoudiniAssetInstanceInput->SetHoudiniAssetComponent(this);
			InstanceInputs.Add(HoudiniInstanceInputKey, HoudiniAssetInstanceInput);
		}
	}
}


void
UHoudiniAssetComponent::PostLoadInitializeInstanceInputs()
{
	for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator
		IterInstanceInputs(InstanceInputs); IterInstanceInputs; ++IterInstanceInputs)
	{
		UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstanceInputs.Value();
		HoudiniAssetInstanceInput->SetHoudiniAssetComponent(this);

	}
}


void
UHoudiniAssetComponent::PostLoadInitializeParameters()
{
	// We need to re-patch parent parameter links.
	for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

		HoudiniAssetParameter->SetHoudiniAssetComponent(this);

		HAPI_ParmId ParentParameterId = HoudiniAssetParameter->GetParmParentId();
		if(-1 != ParentParameterId)
		{
			UHoudiniAssetParameter* const* FoundParentParameter = Parameters.Find(ParentParameterId);
			if(FoundParentParameter)
			{
				HoudiniAssetParameter->SetParentParameter(*FoundParentParameter);
			}
		}
	}

	// Notify all parameters that have children that loading is complete.
	for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		if(HoudiniAssetParameter)
		{
			if(HoudiniAssetParameter->HasChildParameters())
			{
				HoudiniAssetParameter->NotifyChildParametersLoaded();
			}
		}
	}
}


void
UHoudiniAssetComponent::RemoveStaticMeshComponent(UStaticMesh* StaticMesh)
{
	UStaticMeshComponent* const* FoundStaticMeshComponent = StaticMeshComponents.Find(StaticMesh);
	if(FoundStaticMeshComponent)
	{
		StaticMeshComponents.Remove(StaticMesh);

		UStaticMeshComponent* StaticMeshComponent = *FoundStaticMeshComponent;
		if(StaticMeshComponent)
		{
			StaticMeshComponent->DetachFromParent();
			StaticMeshComponent->UnregisterComponent();
			StaticMeshComponent->DestroyComponent();
		}
	}
}


const FGuid&
UHoudiniAssetComponent::GetComponentGuid() const
{
	return ComponentGUID;
}


UMaterialInterface*
UHoudiniAssetComponent::GetReplacementMaterial(const FHoudiniGeoPartObject& HoudiniGeoPartObject,
	const FString& MaterialName)
{
	UMaterialInterface* ReplacementMaterial = nullptr;

	if(HoudiniAssetComponentMaterials)
	{
		TMap<FHoudiniGeoPartObject, TMap<FString, UMaterialInterface*> >& MaterialReplacements =
			HoudiniAssetComponentMaterials->Replacements;

		if(MaterialReplacements.Contains(HoudiniGeoPartObject))
		{
			TMap<FString, UMaterialInterface*>& FoundReplacements = MaterialReplacements[HoudiniGeoPartObject];

			UMaterialInterface* const* FoundReplacementMaterial = FoundReplacements.Find(MaterialName);
			if(FoundReplacementMaterial)
			{
				ReplacementMaterial = *FoundReplacementMaterial;
			}
		}
	}

	return ReplacementMaterial;
}


bool
UHoudiniAssetComponent::GetReplacementMaterialShopName(const FHoudiniGeoPartObject& HoudiniGeoPartObject,
	UMaterialInterface* MaterialInterface, FString& MaterialName)
{
	if(HoudiniAssetComponentMaterials)
	{
		TMap<FHoudiniGeoPartObject, TMap<FString, UMaterialInterface*> >& MaterialReplacements =
			HoudiniAssetComponentMaterials->Replacements;

		if(MaterialReplacements.Contains(HoudiniGeoPartObject))
		{
			TMap<FString, UMaterialInterface*>& FoundReplacements = MaterialReplacements[HoudiniGeoPartObject];

			const FString* FoundMaterialShopName = FoundReplacements.FindKey(MaterialInterface);
			if(FoundMaterialShopName)
			{
				MaterialName = *FoundMaterialShopName;
				return true;
			}
		}
	}

	return false;
}


UMaterial*
UHoudiniAssetComponent::GetAssignmentMaterial(const FString& MaterialName)
{
	UMaterial* Material = nullptr;

	if(HoudiniAssetComponentMaterials)
	{
		TMap<FString, UMaterial*>& MaterialAssignments = HoudiniAssetComponentMaterials->Assignments;

		UMaterial* const* FoundMaterial = MaterialAssignments.Find(MaterialName);
		if(FoundMaterial)
		{
			Material = *FoundMaterial;
		}
	}

	return Material;
}


bool
UHoudiniAssetComponent::ReplaceMaterial(const FHoudiniGeoPartObject& HoudiniGeoPartObject,
	UMaterialInterface* NewMaterialInterface, UMaterialInterface* OldMaterialInterface, int32 MaterialIndex)
{
	if(!HoudiniAssetComponentMaterials)
	{
		return false;
	}

	UStaticMesh* StaticMesh = LocateStaticMesh(HoudiniGeoPartObject);
	if(!StaticMesh)
	{
		return false;
	}

	UStaticMeshComponent* StaticMeshComponent = LocateStaticMeshComponent(StaticMesh);
	if(!StaticMeshComponent)
	{
		TArray<UInstancedStaticMeshComponent*> InstancedStaticMeshComponents;
		if(!LocateInstancedStaticMeshComponents(StaticMesh, InstancedStaticMeshComponents))
		{
			return false;
		}
	}

	TMap<FHoudiniGeoPartObject, TMap<FString, UMaterialInterface*> >& MaterialReplacements =
		HoudiniAssetComponentMaterials->Replacements;

	TMap<FString, UMaterial*>& MaterialAssignments = HoudiniAssetComponentMaterials->Assignments;

	UMaterial* DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

	if(!MaterialReplacements.Contains(HoudiniGeoPartObject))
	{
		// If there's no replacement map for this geo part object, add one.
		MaterialReplacements.Add(HoudiniGeoPartObject, TMap<FString, UMaterialInterface*>());
	}

	// Retrieve replacements for this geo part object.
	TMap<FString, UMaterialInterface*>& MaterialReplacementsValues = MaterialReplacements[HoudiniGeoPartObject];

	const FString* FoundMaterialShopName = MaterialReplacementsValues.FindKey(OldMaterialInterface);
	if(FoundMaterialShopName)
	{
		// This material has been replaced previously. Replace old material with new material.
		FString MaterialShopName = *FoundMaterialShopName;
		MaterialReplacementsValues.Add(MaterialShopName, NewMaterialInterface);
	}
	else
	{
		UMaterial* OldMaterial = Cast<UMaterial>(OldMaterialInterface);
		if(OldMaterial)
		{
			// We have no previous replacement for this material, see if we have it in list of material assignments.
			FoundMaterialShopName = MaterialAssignments.FindKey(OldMaterial);
			if(FoundMaterialShopName)
			{
				// This material has been assigned previously. Add material replacement entry.
				FString MaterialShopName = *FoundMaterialShopName;
				MaterialReplacementsValues.Add(MaterialShopName, NewMaterialInterface);
			}
			else if(OldMaterial == DefaultMaterial)
			{
				// This is replacement for default material. Add material replacement entry.
				FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
				MaterialReplacementsValues.Add(MaterialShopName, NewMaterialInterface);
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}


void
UHoudiniAssetComponent::RemoveReplacementMaterial(const FHoudiniGeoPartObject& HoudiniGeoPartObject,
	const FString& MaterialName)
{
	if(HoudiniAssetComponentMaterials)
	{
		TMap<FHoudiniGeoPartObject, TMap<FString, UMaterialInterface*> >& MaterialReplacements =
			HoudiniAssetComponentMaterials->Replacements;

		if(MaterialReplacements.Contains(HoudiniGeoPartObject))
		{
			TMap<FString, UMaterialInterface*>& MaterialReplacementsValues =
				MaterialReplacements[HoudiniGeoPartObject];

			MaterialReplacementsValues.Remove(MaterialName);
		}
	}
}


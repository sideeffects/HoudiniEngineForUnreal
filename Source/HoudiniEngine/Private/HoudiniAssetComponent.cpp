/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"
#include <vector>
#include <string>
#include <stdint.h>


TMap<UHoudiniAsset*, UClass*> 
UHoudiniAssetComponent::AssetClassRegistry;


UHoudiniAssetComponent::UHoudiniAssetComponent(const FPostConstructInitializeProperties& PCIP) : 
	Super(PCIP),
	OriginalClass(nullptr),
	bIsNativeComponent(false),
	bIsCooking(false),
	AssetId(-1),
	Material(nullptr)
{
	// These are experimental.
	bRenderInMainPass = true;
	//bTickInEditor = true;
	//bTreatAsBackgroundForOcclusion = false;
	//bNeverNeedsRenderUpdate = false;

	PrimaryComponentTick.bCanEverTick = false;
}


void 
UHoudiniAssetComponent::SetNative(bool InbIsNativeComponent)
{
	bIsNativeComponent = InbIsNativeComponent;
}


void 
UHoudiniAssetComponent::OnRep_HoudiniAsset(UHoudiniAsset* OldHoudiniAsset)
{
	HOUDINI_LOG_MESSAGE(TEXT("OnRep_HoudiniAsset, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	// Only do stuff if this actually changed from the last local value.
	if(OldHoudiniAsset != HoudiniAsset)
	{
		// We have to force a call to SetHoudiniAsset with a new HoudiniAsset.
		UHoudiniAsset* NewHoudiniAsset = HoudiniAsset;
		HoudiniAsset = NULL;

		SetHoudiniAsset(NewHoudiniAsset);
	}
}


void 
UHoudiniAssetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHoudiniAssetComponent, HoudiniAsset);
}


bool 
UHoudiniAssetComponent::SetHoudiniAsset(UHoudiniAsset* NewHoudiniAsset)
{
	HOUDINI_LOG_MESSAGE(TEXT("Setting asset, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	// Do nothing if we are already using the supplied Houdini asset.
	if(NewHoudiniAsset == HoudiniAsset)
	{
		return false;
	}

	// Don't allow changing Houdini assets if "static" and registered.
	AActor* Owner = GetOwner();
	if(Mobility == EComponentMobility::Static && IsRegistered() && Owner != NULL)
	{
		FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SetHoudiniAssetOnStatic", "Calling SetHoudiniAsset on '{0}' but Mobility is Static."), FText::FromString(GetPathName(this))));
		return false;
	}

	HoudiniAsset = NewHoudiniAsset;

	// Need to send this to render thread at some point.
	MarkRenderStateDirty();

	// Update physics representation right away.
	RecreatePhysicsState();

	// Notify the streaming system. Don't use Update(), because this may be the first time the mesh has been set
	// and the component may have to be added to the streaming system for the first time.
	GStreamingManager->NotifyPrimitiveAttached(this, DPT_Spawned);

	// Since we have new asset, we need to update bounds.
	UpdateBounds();
	return true;
}


FBoxSphereBounds 
UHoudiniAssetComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	if(HoudiniAsset)
	{
		//FBoxSphereBounds NewBounds = HoudiniAsset->GetBounds().TransformBy(LocalToWorld);
	}
	else
	{

	}
	/*
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector::ZeroVector;
	NewBounds.BoxExtent = FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
	NewBounds.SphereRadius = FMath::Sqrt(3.0f * FMath::Square(HALF_WORLD_MAX));
	return NewBounds;
	*/

	//return Super::CalcBounds(LocalToWorld);
	//return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);

	 const FVector UnitV(1,1,1);
	 const FBox VeryVeryBig( -UnitV * HALF_WORLD_MAX, UnitV * HALF_WORLD_MAX );
	 return FBoxSphereBounds( VeryVeryBig );
}


int32 
UHoudiniAssetComponent::GetNumMaterials() const
{
	return 1;
}


FPrimitiveSceneProxy* 
UHoudiniAssetComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	
	if(HoudiniMeshTris.Num() > 0)
	{
		Proxy = new FHoudiniMeshSceneProxy(this);
	}
	
	return Proxy;
}


/*
void 
UHoudiniAssetComponent::BeginDestroy()
{
	Super::BeginDestroy();
	HOUDINI_LOG_MESSAGE(TEXT("Starting destruction, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}

void 
UHoudiniAssetComponent::FinishDestroy()
{
	Super::FinishDestroy();
	HOUDINI_LOG_MESSAGE(TEXT("Finishing destruction, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}
*/


void 
UHoudiniAssetComponent::MonkeyPatchArrayProperties(UArrayProperty* ArrayProperty)
{

}


void 
UHoudiniAssetComponent::MonkeyPatchCreateProperties(UClass* ClassInstance)
{
	if(-1 == AssetId)
	{
		// Asset has not been instantiated, there's nothing we can do.
		return;
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	HAPI_AssetInfo AssetInfo;
	Result = HAPI_GetAssetInfo(AssetId, &AssetInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return;
	}

	HAPI_NodeInfo NodeInfo;
	Result = HAPI_GetNodeInfo(AssetInfo.nodeId, &NodeInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		
	}
		
	std::vector<HAPI_ParmInfo> ParmInfo;
	ParmInfo.reserve(NodeInfo.parmCount);
	Result = HAPI_GetParameters(AssetInfo.nodeId, &ParmInfo[0], 0, NodeInfo.parmCount);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		
	}

	// Retrieve integer values for this asset.
	std::vector<int> ParmValuesIntegers;
	ParmValuesIntegers.reserve(NodeInfo.parmIntValueCount);
	Result = HAPI_GetParmIntValues(AssetInfo.nodeId, &ParmValuesIntegers[0], 0, NodeInfo.parmIntValueCount);
	if(HAPI_RESULT_SUCCESS != Result)
	{
	
	}
	
	// Retrieve float values for this asset.
	std::vector<float> ParmValuesFloats;
	ParmValuesFloats.reserve(NodeInfo.parmFloatValueCount);
	Result = HAPI_GetParmFloatValues(AssetInfo.nodeId, &ParmValuesFloats[0], 0, NodeInfo.parmFloatValueCount);
	if(HAPI_RESULT_SUCCESS != Result)
	{

	}

	// Retrieve string values for this asset.
	std::vector<HAPI_StringHandle> ParmStringFloats;
	ParmStringFloats.reserve(NodeInfo.parmStringValueCount);
	Result = HAPI_GetParmStringValues(AssetInfo.nodeId, true, &ParmStringFloats[0], 0, NodeInfo.parmStringValueCount);
	if (HAPI_RESULT_SUCCESS != Result)
	{

	}	

	// We need to insert new properties and new children in the beginning of single link list.
	// This way properties and children from the original class can be reused and will not have
	// their next pointers altered.
	UProperty* PropertyFirst = nullptr;
	UProperty* PropertyLast = PropertyFirst;

	UField* ChildFirst = nullptr;
	UField* ChildLast = ChildFirst;

	// Calculate next 128 boundary past scratch space (or use scratch space address if it is 16bb aligned).
	//const char* ScratchSpaceAligned = (const char*) ((reinterpret_cast<uintptr_t>(buffer) + 15) & ~15);
	//uint32 ValuesOffsetStart = (const char*) ScratchSpaceAligned - (const char*) this;
	//uint32 ValuesOffsetStart = offsetof(UHoudiniAssetComponent, buffer);
	uint32 ValuesOffsetStart = sizeof(UHoudiniAssetComponent);
	uint32 ValuesOffsetEnd = ValuesOffsetStart;

	for(int idx = 0; idx < NodeInfo.parmCount; ++idx)
	{
		// Retrieve param info at this index.
		const HAPI_ParmInfo& ParmInfoIter = ParmInfo[idx];
			
		// Skip unsupported param types for now.
		switch(ParmInfoIter.type)
		{
			case HAPI_PARMTYPE_INT:
			case HAPI_PARMTYPE_FLOAT:
			case HAPI_PARMTYPE_TOGGLE:
			case HAPI_PARMTYPE_COLOR:
			case HAPI_PARMTYPE_STRING:
			{
				break;
			}

			default:
			{
				// Just ignore unsupported types for now.
				continue;
			}
		}
			
		// Retrieve length of this parameter's name.
		int32 ParmNameLength = 0;
		Result = HAPI_GetStringBufLength(ParmInfoIter.nameSH, &ParmNameLength);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We have encountered an error retrieving length of this parameter's name, continue onto next parameter.
			continue;
		}

		// If length of name of this parameter is zero, continue onto next parameter.
		if(!ParmNameLength)
		{
			continue;
		}
						
		// Retrieve name for this parameter.
		std::vector<char> ParmName;
		ParmName.reserve(ParmNameLength + 1);
		ParmName[ParmNameLength] = '\0';
		Result = HAPI_GetString(ParmInfoIter.nameSH, &ParmName[0], ParmNameLength);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We have encountered an error retrieving the name of this parameter, continue onto next parameter.
			continue;
		}

		// We need to convert name to a string Unreal understands.
		FUTF8ToTCHAR ParamNameStringConverter(&ParmName[0]);
		FName ParmNameConverted = ParamNameStringConverter.Get();

		// Retrieve length of this parameter's label.
		int32 ParmLabelLength = 0;
		Result = HAPI_GetStringBufLength(ParmInfoIter.labelSH, &ParmLabelLength);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We have encountered an error retrieving length of this parameter's label, continue onto next parameter.
			continue;
		}

		// Retrieve label for this parameter.
		std::vector<char> ParmLabel;
		ParmLabel.reserve(ParmLabelLength + 1);
		ParmLabel[ParmLabelLength] = '\0';
		Result = HAPI_GetString(ParmInfoIter.labelSH, &ParmLabel[0], ParmLabelLength);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We have encountered an error retrieving the label of this parameter, continue onto next parameter.
			continue;
		}

		// We need to convert label to a string Unreal understands.
		FUTF8ToTCHAR ParamLabelStringConverter(&ParmLabel[0]);
			
		// Create a property.
		static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient | RF_Native;
		static const uint64 PropertyFlags =  UINT64_C(69793219077);
		UProperty* Property = nullptr;

		switch(ParmInfoIter.type)
		{
			case HAPI_PARMTYPE_INT:
			{
				if(ParmInfoIter.size == 1)
				{
					// Create property object.
					Property = new(ClassInstance, ParmNameConverted, PropertyObjectFlags) UIntProperty(FPostConstructInitializeProperties(), EC_CppProperty, ValuesOffsetEnd, 0x0);

					// Write property data to which it refers by offset.
					*(int*)((char*) this + ValuesOffsetEnd) = ParmValuesIntegers[ParmInfoIter.intValuesIndex];

					// Increment offset for next property.
					ValuesOffsetEnd += sizeof(int);

					//Property->SetMetaData("MakeStructureDefaultValue", *FString::FromInt(ParmValuesIntegers[ParmInfoIter.intValuesIndex]));
				}
				/*
				else
				{
					Property = new(ClassInstance, ParamNameStringConverter.Get(), PropertyObjectFlags) UArrayProperty(FPostConstructInitializeProperties(), EC_CppProperty, ValuesOffsetEnd, 0x0);						
				}
				*/

				break;
			}

			case HAPI_PARMTYPE_FLOAT:
			{
				if(ParmInfoIter.size == 1)
				{
					Property = new(ClassInstance, ParmNameConverted, PropertyObjectFlags) UFloatProperty(FPostConstructInitializeProperties(), EC_CppProperty, ValuesOffsetEnd, 0x0);

					// Write property data to which it refers by offset.
					*(float*)((char*) this + ValuesOffsetEnd) = ParmValuesFloats[ParmInfoIter.floatValuesIndex];

					ValuesOffsetEnd += sizeof(float);

					//Property->SetMetaData("MakeStructureDefaultValue", *FString::SanitizeFloat(ParmValuesFloats[ParmInfoIter.floatValuesIndex]));
				}

				break;
			}

			case HAPI_PARMTYPE_TOGGLE:
			{
				if(ParmInfoIter.size == 1)
				{
					Property = new(ClassInstance, ParmNameConverted, PropertyObjectFlags) UBoolProperty(FPostConstructInitializeProperties(), EC_CppProperty, ValuesOffsetEnd, 0x0, ~0, sizeof(bool), true);
					
					// Write property data to which it refers by offset.
					*(bool*)((char*) this + ValuesOffsetEnd) = ParmValuesIntegers[ParmInfoIter.intValuesIndex] != 0;
					
					ValuesOffsetEnd += sizeof(bool);
				}

				break;
			}

			case HAPI_PARMTYPE_COLOR:
			{
				if(ParmInfoIter.size == 1)
				{
					// Implement this.
				}

				break;
			}

			case HAPI_PARMTYPE_STRING:
			{
				if(ParmInfoIter.size == 1)
				{
					// Implement this.
				}

				break;
			}
			
			default:
			{
				ASSUME(0);
			}
		}

		if(!Property)
		{
			// There was an error creating a property - report and skip to next param.
			continue;
		}
		
		// Otherwise, perform relevant property initialization.
		Property->PropertyLinkNext = nullptr;
		Property->SetMetaData(TEXT("Category"), TEXT("HoudiniAsset"));
		Property->PropertyFlags = PropertyFlags;

		// Use label instead of name if it is present.
		if(ParmLabelLength)
		{
			Property->SetMetaData(TEXT("DisplayName"), ParamLabelStringConverter.Get());
		}

		// Set UI and physical ranges, if present.
		if(ParmInfoIter.hasUIMin)
		{
			Property->SetMetaData(TEXT("UIMin"), *FString::SanitizeFloat(ParmInfoIter.UIMin));
		}

		if(ParmInfoIter.hasUIMax)
		{
			Property->SetMetaData(TEXT("UIMax"), *FString::SanitizeFloat(ParmInfoIter.UIMax));
		}

		if(ParmInfoIter.hasMin)
		{
			Property->SetMetaData(TEXT("ClampMin"), *FString::SanitizeFloat(ParmInfoIter.min));
		}
		
		if(ParmInfoIter.hasMax)
		{
			Property->SetMetaData(TEXT("ClampMax"), *FString::SanitizeFloat(ParmInfoIter.max));
		}

		// Insert this newly created property in link list of properties.
		if(!PropertyFirst)
		{
			PropertyFirst = Property;
			PropertyLast = Property;
		}
		else
		{
			PropertyLast->PropertyLinkNext = Property;
			PropertyLast = Property;
		}

		// Insert this newly created property into link list of children.
		if(!ChildFirst)
		{
			ChildFirst = Property;
			ChildLast = Property;
		}
		else
		{
			ChildLast->Next = Property;
			ChildLast = Property;
		}
	}

	if(PropertyFirst)
	{
		ClassInstance->PropertyLink = PropertyFirst;
		PropertyLast->PropertyLinkNext = GetClass()->PropertyLink;
	}

	if(ChildFirst)
	{
		ClassInstance->Children = ChildFirst;
		ChildLast->Next = GetClass()->Children;
	}
}


void 
UHoudiniAssetComponent::MonkeyPatchClassInformation(UClass* ClassInstance)
{
	UClass** Address = (UClass**) this;
	UClass** EndAddress = (UClass**) this + sizeof(UHoudiniAssetComponent);

	//Grab class of UHoudiniAssetComponent.
	UClass* HoudiniAssetComponentClass = GetClass();

	while(*Address != HoudiniAssetComponentClass && Address < EndAddress)
	{
		Address++;
	}

	if(*Address == HoudiniAssetComponentClass)
	{
		*Address = ClassInstance;
	}
}


void 
UHoudiniAssetComponent::CookingCompleted(HAPI_AssetId InAssetId, const char* AssetName)
{
	HOUDINI_LOG_MESSAGE(TEXT("Notification about finished cooking, AssetId = %d, AssetName = %s."), AssetId, AssetName);

	// We are no longer cooking.
	bIsCooking = false;

	// Copy over the supplied asset id.
	AssetId = InAssetId;

	{
		HAPI_Result Result = HAPI_RESULT_SUCCESS;

		HAPI_AssetInfo AssetInfo;
		Result = HAPI_GetAssetInfo(AssetId, &AssetInfo);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			return;
		}

		HAPI_PartInfo PartInfo;
		Result = HAPI_GetPartInfo(AssetId, 0, 0, 0, &PartInfo);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			return;
		}

		std::vector<int> VertexList;
		VertexList.reserve(PartInfo.vertexCount);
		Result = HAPI_GetVertexList(AssetId, 0, 0, 0, &VertexList[0], 0, PartInfo.vertexCount);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			return;
		}

		HAPI_AttributeInfo AttribInfo;
		Result = HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "P", HAPI_ATTROWNER_POINT, &AttribInfo);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			return;
		}

		// Retrieve positions.
		std::vector<float> Positions;
		Positions.reserve(AttribInfo.count * AttribInfo.tupleSize);
		//positions are always o na point.
		Result = HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "P", &AttribInfo, &Positions[0], 0, AttribInfo.count);
		if(HAPI_RESULT_SUCCESS != Result)
		{	
			return;
		}

		HoudiniMeshTris.Reset();

		for(int i = 0; i < PartInfo.vertexCount / 3; ++i)
		{
			FHoudiniMeshTriangle triangle;

			// Need to flip the Y with the Z since UE4 is Z-up.
			// Need to flip winding order also.

			triangle.Vertex0.X = Positions[VertexList[i * 3 + 0] * 3 + 0] * 100.0f;
			triangle.Vertex0.Z = Positions[VertexList[i * 3 + 0] * 3 + 1] * 100.0f;
			triangle.Vertex0.Y = Positions[VertexList[i * 3 + 0] * 3 + 2] * 100.0f;

			triangle.Vertex2.X = Positions[VertexList[i * 3 + 1] * 3 + 0] * 100.0f;
			triangle.Vertex2.Z = Positions[VertexList[i * 3 + 1] * 3 + 1] * 100.0f;
			triangle.Vertex2.Y = Positions[VertexList[i * 3 + 1] * 3 + 2] * 100.0f;

			triangle.Vertex1.X = Positions[VertexList[i * 3 + 2] * 3 + 0] * 100.0f;
			triangle.Vertex1.Z = Positions[VertexList[i * 3 + 2] * 3 + 1] * 100.0f;
			triangle.Vertex1.Y = Positions[VertexList[i * 3 + 2] * 3 + 2] * 100.0f;

			HoudiniMeshTris.Push(triangle);
		}

		// Retrieve uv data.
		{
			HAPI_AttributeInfo AttribInfo;
			// can be both, check both. could be on detail.
			Result = HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "uv", HAPI_ATTROWNER_VERTEX, &AttribInfo);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				return;
			}

			std::vector<float> UVs;
			UVs.reserve(AttribInfo.count * AttribInfo.tupleSize);
			Result = HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "uv", &AttribInfo, &UVs[0], 0, AttribInfo.count);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				return;
			}

			for (int i = 0; i < PartInfo.vertexCount / 3; ++i)
			{
				FHoudiniMeshTriangle& triangle = HoudiniMeshTris[i];

				triangle.TextureCoordinate0.X = UVs[i * 9 + 0];
				triangle.TextureCoordinate0.Y = 1.0f - UVs[i * 9 + 1];
				
				triangle.TextureCoordinate2.X = UVs[i * 9 + 3];
				triangle.TextureCoordinate2.Y = 1.0f - UVs[i * 9 + 4];

				triangle.TextureCoordinate1.X = UVs[i * 9 + 6];
				triangle.TextureCoordinate1.Y = 1.0f - UVs[i * 9 + 7];
			}
		}





		// Load textures.
		HAPI_MaterialInfo MaterialInfo;
		Result = HAPI_GetMaterial(AssetId, PartInfo.materialId, &MaterialInfo);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			return;
		}

		HAPI_NodeInfo NodeInfo;
		Result = HAPI_GetNodeInfo(MaterialInfo.nodeId, &NodeInfo);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			return;
		}

		std::vector<HAPI_ParmInfo> NodeParams;
		NodeParams.resize(NodeInfo.parmCount);
		Result = HAPI_GetParameters(NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			return;
		}

		int TexturesLoaded = 0;

		for(int ParmIdx = 0; ParmIdx < NodeInfo.parmCount; ++ParmIdx)
		{
			HAPI_ParmInfo& NodeParmInfo = NodeParams[ParmIdx];
			HAPI_StringHandle NodeParmHandle = NodeParmInfo.nameSH;

			int NodeParmNameLength = 0;
			Result = HAPI_GetStringBufLength(NodeParmHandle, &NodeParmNameLength);
			if(HAPI_RESULT_SUCCESS != Result)
			{
				continue;
			}

			std::vector<char> NodeParmName;
			NodeParmName.reserve(NodeParmNameLength + 1);
			NodeParmName[NodeParmNameLength] = '\0';
			Result = HAPI_GetString(NodeParmHandle, &NodeParmName[0], NodeParmNameLength);
			if(HAPI_RESULT_SUCCESS != Result)
			{
				continue;
			}

			if(!strncmp(&NodeParmName[0], "map", 3) || !strncmp(&NodeParmName[0], "ogl_tex1", 8) || !strncmp(&NodeParmName[0], "baseColorMap", 12))
			{
				Result = HAPI_RenderTextureToImage(AssetInfo.id, MaterialInfo.id, NodeParmInfo.id);
				if(HAPI_RESULT_SUCCESS != Result)
				{
					continue;
				}

				//extractHoudiniImageToTexture( material_info, folder_path, "C A" );
				HAPI_ImageInfo ImageInfo;
				Result = HAPI_GetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
				if(HAPI_RESULT_SUCCESS != Result)
				{
					continue;
				}

				ImageInfo.dataFormat = HAPI_IMAGE_DATA_INT8;
				ImageInfo.interleaved = true;
				ImageInfo.packing = HAPI_IMAGE_PACKING_RGBA;

				Result = HAPI_SetImageInfo(MaterialInfo.assetId, MaterialInfo.id, ImageInfo);
				if(HAPI_RESULT_SUCCESS != Result)
				{
					continue;
				}

				int ImageBufferSize = 0;
				Result = HAPI_ExtractImageToMemory(MaterialInfo.assetId, MaterialInfo.id, HAPI_RAW_FORMAT_NAME, "C A", &ImageBufferSize);
				if(HAPI_RESULT_SUCCESS != Result)
				{
					continue;
				}

				std::vector<char> ImageBuffer;
				ImageBuffer.reserve(ImageBufferSize);
				Result = HAPI_GetImageMemoryBuffer(MaterialInfo.assetId, MaterialInfo.id, &ImageBuffer[0], ImageBufferSize);
				if(HAPI_RESULT_SUCCESS != Result)
				{
					continue;
				}

				
				/*
				FName MaterialName = MakeUniqueObjectName(this->GetOuter(), UMaterial::StaticClass(), TEXT("HoudiniAsset_Material"));
				UMaterial* Material = ConstructObject<UMaterial>(UMaterial::StaticClass(), this->GetOuter(), MaterialName, this->GetFlags());
				Material->TwoSided = false;
				Material->SetLightingModel(MLM_DefaultLit);

				MaterialExportUtils::FFlattenMaterial FlattenMaterial;

				FString DiffuseTextureName = MakeUniqueObjectName(this->GetOuter(), UTexture2D::StaticClass(), TEXT("Texture_Diffuse")).ToString();
				FCreateTexture2DParameters TexParams;
				TexParams.bUseAlpha = false;
				TexParams.CompressionSettings = TC_Default;
				TexParams.bDeferCompression = true;
				TexParams.bSRGB = false;
				*/
				/*
				UTexture2D* DiffuseTexture = FImageUtils::CreateTexture2D(
					ImageInfo.xRes,
					ImageInfo.yRes,
					FlattenMaterial.DiffuseSamples,
					this,
					TEXT("sdf"),
					GetFlags(),
					FCreateTexture2DParameters());
				*/

				// Create material if does not exist already.
				if(!Material)
				{
					Material = ConstructObject<UMaterial>(UMaterial::StaticClass(), this->GetOuter(), TEXT("DiffuseSpaceship"),  RF_Public | RF_Standalone | RF_Transient);
				
					/*
					FString MaterialFullName = TEXT("MaterialSpaceShipLongName");
					FString NewPackageName = FPackageName::GetLongPackagePath(GetOutermost()->GetName()) + TEXT("/") + MaterialFullName;
					NewPackageName = PackageTools::SanitizePackageName(NewPackageName);
					UPackage* Package = CreatePackage(NULL, *NewPackageName);
					UMaterialFactoryNew* MaterialFactory = new UMaterialFactoryNew(FPostConstructInitializeProperties());
					UMaterial* Material = (UMaterial*)MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *MaterialFullName, RF_Standalone|RF_Public, NULL, GWarn);
					*/

					Material->TwoSided = false;
					Material->SetLightingModel(MLM_DefaultLit);

					// Assign material.
					SetMaterial(0, Material);
				}



				// Create diffuse texture.
				UTexture2D* DiffuseTexture = UTexture2D::CreateTransient(ImageInfo.xRes, ImageInfo.yRes, PF_B8G8R8A8);

				// Lock texture for modification.
				uint8* MipData = static_cast<uint8*>(DiffuseTexture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

				// Create base map.
				uint8* DestPtr = NULL;
				const FColor* SrcPtr = NULL;
				uint32 SrcWidth = ImageInfo.xRes;
				uint32 SrcHeight = ImageInfo.yRes;
				const char* SrcData = &ImageBuffer[0];

				for(uint32 y = 0; y < SrcHeight; y++)
				{
					DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];
					
					for(uint32 x = 0; x < SrcWidth; x++)
					{
						uint32 DataOffset = y * SrcWidth * 4 + x * 4;

						*DestPtr++ = *(uint8*)(SrcData + DataOffset + 0); //B
						*DestPtr++ = *(uint8*)(SrcData + DataOffset + 1); //G
						*DestPtr++ = *(uint8*)(SrcData + DataOffset + 2); //R
						*DestPtr++ = *(uint8*)(SrcData + DataOffset + 3); //A
						//*DestPtr++ = 0xFF; //A
					}
				}

				// Unlock the texture.
				DiffuseTexture->PlatformData->Mips[0].BulkData.Unlock();
				DiffuseTexture->UpdateResource();


				

				// Assign texture to material.
				UMaterialExpressionTextureSample* Expression = ConstructObject<UMaterialExpressionTextureSample>(UMaterialExpressionTextureSample::StaticClass(), Material);
				Material->Expressions.Add(Expression);
				Material->DiffuseColor.Expression = Expression;
				Expression->Texture = DiffuseTexture;


				
			}
		}
	}

	//const uint32 CustomDataStartOffset = 24000;

	// See if we have this class already.
	UClass** StoredHoudiniClass = UHoudiniAssetComponent::AssetClassRegistry.Find(HoudiniAsset);
	UClass* MonkeyPatchAssetPatchedClass = nullptr;

	// Class does not exist, we need to create one.
	if(!StoredHoudiniClass)
	{
		// We need to monkey patch this component instance.
		EObjectFlags MonkeyPatchedClassFlags = RF_Public | RF_Standalone | RF_Transient | RF_Native | RF_RootSet;
		//RF_RootSet = 0x00000080,	///< Object will not be garbage collected, even if unreferenced.
		//RF_Public = 0x00000001,	///< Object is visible outside its package.
		//RF_Standalone = 0x00000002,	///< Keep object around for editing even if unreferenced.
		//RF_Native = 0x00000004,	///< Native (UClass only).

		// Grab default object of UClass.
		UObject* ClassOfUClass = UClass::StaticClass()->GetDefaultObject();

		//Grab class of UHoudiniAssetComponent.
		UClass* ClassOfUHoudiniAssetComponent = UHoudiniAssetComponent::StaticClass();

		MonkeyPatchAssetPatchedClass = ConstructObject<UClass>(UClass::StaticClass(), this->GetOutermost(), FName(*GetClass()->GetName()), MonkeyPatchedClassFlags, ClassOfUHoudiniAssetComponent, true);
		//MonkeyPatchAssetPatchedClass = ConstructObject<UClass>(UClass::StaticClass(), this->GetOutermost(), FName(*this->GetName()), MonkeyPatchedClassFlags, ClassOfUHoudiniAssetComponent, true);
		MonkeyPatchAssetPatchedClass->ClassFlags = UHoudiniAssetComponent::StaticClassFlags;
		MonkeyPatchAssetPatchedClass->ClassCastFlags = UHoudiniAssetComponent::StaticClassCastFlags();
		MonkeyPatchAssetPatchedClass->ClassConfigName = UHoudiniAssetComponent::StaticConfigName();

		MonkeyPatchAssetPatchedClass->ClassDefaultObject = this->GetClass()->ClassDefaultObject;
		MonkeyPatchAssetPatchedClass->ClassConstructor = ClassOfUHoudiniAssetComponent->ClassConstructor;
		MonkeyPatchAssetPatchedClass->ClassAddReferencedObjects = ClassOfUHoudiniAssetComponent->ClassAddReferencedObjects;
		MonkeyPatchAssetPatchedClass->MinAlignment = ClassOfUHoudiniAssetComponent->MinAlignment;
		MonkeyPatchAssetPatchedClass->PropertiesSize = ClassOfUHoudiniAssetComponent->PropertiesSize + 65536;
		//MonkeyPatchAssetPatchedClass->PropertiesSize = ClassOfUHoudiniAssetComponent->PropertiesSize;
		MonkeyPatchAssetPatchedClass->SetSuperStruct(ClassOfUHoudiniAssetComponent->GetSuperStruct());

		MonkeyPatchAssetPatchedClass->ClassReps = ClassOfUHoudiniAssetComponent->ClassReps;
		MonkeyPatchAssetPatchedClass->NetFields = ClassOfUHoudiniAssetComponent->NetFields;
		MonkeyPatchAssetPatchedClass->ReferenceTokenStream = ClassOfUHoudiniAssetComponent->ReferenceTokenStream;
		MonkeyPatchAssetPatchedClass->NativeFunctionLookupTable = ClassOfUHoudiniAssetComponent->NativeFunctionLookupTable;
		//MonkeyPatchAssetPatchedClass->FuncMap

		// Iterate original properties ~ find Houdini asset.
		/*
		UProperty* PropertyIterator = GetClass()->PropertyLink;
		UProperty* LastProperty = NULL;
		while(PropertyIterator)
		{
			LastProperty = PropertyIterator;
			PropertyIterator = PropertyIterator->PropertyLinkNext;
		}
		*/

		/*
		MonkeyPatchAssetPatchedClass->PropertyLink = GetClass()->PropertyLink;

		// Create first custom property.
		UProperty* PatchedProperty0 = new(MonkeyPatchAssetPatchedClass, TEXT("PatchedPropertyUint"), RF_Public | RF_Transient | RF_Native) UIntProperty(FPostConstructInitializeProperties(), EC_CppProperty, CustomDataStartOffset, 0x0);
		//UProperty* PatchedProperty = new(MonkeyPatchAssetPatchedClass, TEXT("PatchedProperty"), RF_Public | RF_Transient | RF_Native) UIntProperty(FPostConstructInitializeProperties(), EC_CppProperty, CustomDataStartOffset, 0x0);
		PatchedProperty0->SetMetaData(TEXT("Category"), TEXT("HoudiniAsset"));
		PatchedProperty0->PropertyFlags = 69793219077;
		//PatchedProperty0->PropertyLinkNext = nullptr;
		PatchedProperty0->PropertyLinkNext = MonkeyPatchAssetPatchedClass->PropertyLink;
		MonkeyPatchAssetPatchedClass->PropertyLink = PatchedProperty0;
	
		// Create second custom property.
		UProperty* PatchedProperty1 = new(MonkeyPatchAssetPatchedClass, TEXT("PatchedPropertyFloat"), RF_Public | RF_Transient | RF_Native) UFloatProperty(FPostConstructInitializeProperties(), EC_CppProperty, CustomDataStartOffset + 4, 0x0);
		//UProperty* PatchedProperty = new(MonkeyPatchAssetPatchedClass, TEXT("PatchedProperty"), RF_Public | RF_Transient | RF_Native) UIntProperty(FPostConstructInitializeProperties(), EC_CppProperty, CustomDataStartOffset, 0x0);
		PatchedProperty1->SetMetaData(TEXT("Category"), TEXT("HoudiniAsset"));
		PatchedProperty1->PropertyFlags = 69793219077;
		//PatchedProperty1->PropertyLinkNext = PatchedProperty0;
		PatchedProperty1->PropertyLinkNext = MonkeyPatchAssetPatchedClass->PropertyLink;
		MonkeyPatchAssetPatchedClass->PropertyLink = PatchedProperty1;

		// Patch children.
		UField* ChildrenIterator = MonkeyPatchAssetPatchedClass->Children;
		UField* LastChild = nullptr;
		while(ChildrenIterator)
		{
			LastChild = ChildrenIterator;
			ChildrenIterator = ChildrenIterator->Next;
		}

		if(LastChild)
		{
			LastChild->Next = ClassOfUHoudiniAssetComponent->Children;
		}
		else
		{
			MonkeyPatchAssetPatchedClass->Children = ClassOfUHoudiniAssetComponent->Children;
		}
		*/

		// Generate custom properties for our new monkey patched class.
		MonkeyPatchCreateProperties(MonkeyPatchAssetPatchedClass);
		
		// Store our new monkey patched class in registry.
		UHoudiniAssetComponent::AssetClassRegistry.Add(HoudiniAsset, MonkeyPatchAssetPatchedClass);
	}
	else
	{
		MonkeyPatchAssetPatchedClass = *StoredHoudiniClass;
	}

	// Before monkey patching, store original class instance, so we can patch it back for proper destruction.
	OriginalClass = GetClass();

	// Attempt to monkey patch this instance with new class information.
	MonkeyPatchClassInformation(MonkeyPatchAssetPatchedClass);

	// Mark render state as dirty - we have updated the data.
	MarkRenderStateDirty();
}


void
UHoudiniAssetComponent::CookingFailed()
{
	HOUDINI_LOG_MESSAGE(TEXT("Notificatoin about failed cooking."));
}


void 
UHoudiniAssetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Retrieve property which changed.
	UProperty* Property = PropertyChangedEvent.Property;

	// Retrieve property category.
	static const FString CategoryHoudiniAsset = TEXT("HoudiniAsset");
	const FString& Category = Property->GetMetaData(TEXT("Category"));

	if(Category != CategoryHoudiniAsset)
	{
		// This property is not in category we are interested in, just jump out.
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}

	// Retrieve offset into scratch space for this property.
	uint32 ValueOffset = Property->GetOffset_ForDebug();
	
	// Get the name of property.
	std::wstring PropertyName(*Property->GetName());
	std::string PropertyNameConverted(PropertyName.begin(), PropertyName.end());

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// Retrieve asset information.
	HAPI_AssetInfo AssetInfo;
	Result = HAPI_GetAssetInfo(AssetId, &AssetInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		// Error retrieving asset information, do not proceed.
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}

	// Locate corresponding param.
	HAPI_ParmId ParamId;
	Result = HAPI_GetParmIdFromName(AssetInfo.nodeId, PropertyNameConverted.c_str(), &ParamId);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		// Error locating corresponding param, do not proceed.
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}

	// Get parameter information.
	HAPI_ParmInfo ParamInfo;
	Result = HAPI_GetParameters(AssetInfo.nodeId, &ParamInfo, ParamId, 1);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		// Error retrieving param information, do not proceed.
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}
	
	// Based on type, upload new values to Houdini Engine.
	if(UIntProperty::StaticClass() == Property->GetClass())
	{
		int Value = *(int*)((const char*) this + ValueOffset);
		Result = HAPI_SetParmIntValues(AssetInfo.nodeId, &Value, ParamInfo.intValuesIndex, ParamInfo.size);

		if(HAPI_RESULT_SUCCESS != Result)
		{
			// Error setting a parameter.
			Super::PostEditChangeProperty(PropertyChangedEvent);
			return;
		}
	}
	else if(UFloatProperty::StaticClass() == Property->GetClass())
	{
		float Value = *(float*)((const char*) this + ValueOffset);
		Result = HAPI_SetParmFloatValues(AssetInfo.nodeId, &Value, ParamInfo.floatValuesIndex, ParamInfo.size);

		if(HAPI_RESULT_SUCCESS != Result)
		{
			// Error setting a parameter.
			Super::PostEditChangeProperty(PropertyChangedEvent);
			return;
		}
	}
	else if(UBoolProperty::StaticClass() == Property->GetClass())
	{
		int Value = *(bool*)((const char*) this + ValueOffset);
		Result = HAPI_SetParmIntValues(AssetInfo.nodeId, &Value, ParamInfo.intValuesIndex, ParamInfo.size);

		if(HAPI_RESULT_SUCCESS != Result)
		{
			// Error setting a parameter.
			Super::PostEditChangeProperty(PropertyChangedEvent);
			return;
		}
	}

	// At this point we can recook the data.
	Result = HAPI_CookAsset(AssetId, nullptr);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		// Error recooking.
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}


	// Some duplication, clean it up.
	{
		HAPI_AssetInfo AssetInfo;
		Result = HAPI_GetAssetInfo(AssetId, &AssetInfo);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			Super::PostEditChangeProperty(PropertyChangedEvent);
			return;
		}

		HAPI_PartInfo PartInfo;
		Result = HAPI_GetPartInfo(AssetId, 0, 0, 0, &PartInfo);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			Super::PostEditChangeProperty(PropertyChangedEvent);
			return;
		}

		std::vector<int> VertexList;
		VertexList.reserve(PartInfo.vertexCount);
		Result = HAPI_GetVertexList(AssetId, 0, 0, 0, &VertexList[0], 0, PartInfo.vertexCount);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			return;
		}

		HAPI_AttributeInfo AttribInfo;
		Result = HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "P", HAPI_ATTROWNER_POINT, &AttribInfo);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			Super::PostEditChangeProperty(PropertyChangedEvent);
			return;
		}

		std::vector<float> Positions;
		Positions.reserve(AttribInfo.count * AttribInfo.tupleSize);
		Result = HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "P", &AttribInfo, &Positions[0], 0, AttribInfo.count);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			Super::PostEditChangeProperty(PropertyChangedEvent);
			return;
		}

		HoudiniMeshTris.Reset();

		for(int i = 0; i < PartInfo.vertexCount / 3; ++i)
		{
			FHoudiniMeshTriangle triangle;

			// Need to flip the Y with the Z since UE4 is Z-up.
			// Need to flip winding order also.

			triangle.Vertex0.X = Positions[VertexList[i * 3 + 0] * 3 + 0] * 100.0f;
			triangle.Vertex0.Z = Positions[VertexList[i * 3 + 0] * 3 + 1] * 100.0f;
			triangle.Vertex0.Y = Positions[VertexList[i * 3 + 0] * 3 + 2] * 100.0f;

			triangle.Vertex2.X = Positions[VertexList[i * 3 + 1] * 3 + 0] * 100.0f;
			triangle.Vertex2.Z = Positions[VertexList[i * 3 + 1] * 3 + 1] * 100.0f;
			triangle.Vertex2.Y = Positions[VertexList[i * 3 + 1] * 3 + 2] * 100.0f;

			triangle.Vertex1.X = Positions[VertexList[i * 3 + 2] * 3 + 0] * 100.0f;
			triangle.Vertex1.Z = Positions[VertexList[i * 3 + 2] * 3 + 1] * 100.0f;
			triangle.Vertex1.Y = Positions[VertexList[i * 3 + 2] * 3 + 2] * 100.0f;

			HoudiniMeshTris.Push(triangle);
		}


		// Retrieve uv data.
		{
			HAPI_AttributeInfo AttribInfo;
			// can be both, check both. could be on detail.
			Result = HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "uv", HAPI_ATTROWNER_VERTEX, &AttribInfo);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				return;
			}

			std::vector<float> UVs;
			UVs.reserve(AttribInfo.count * AttribInfo.tupleSize);
			Result = HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "uv", &AttribInfo, &UVs[0], 0, AttribInfo.count);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				return;
			}

			for (int i = 0; i < PartInfo.vertexCount / 3; ++i)
			{
				FHoudiniMeshTriangle& triangle = HoudiniMeshTris[i];

				triangle.TextureCoordinate0.X = UVs[i * 9 + 0];
				triangle.TextureCoordinate0.Y = 1.0f - UVs[i * 9 + 1];

				triangle.TextureCoordinate2.X = UVs[i * 9 + 3];
				triangle.TextureCoordinate2.Y = 1.0f - UVs[i * 9 + 4];

				triangle.TextureCoordinate1.X = UVs[i * 9 + 6];
				triangle.TextureCoordinate1.Y = 1.0f - UVs[i * 9 + 7];
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void 
UHoudiniAssetComponent::OnRegister()
{
	Super::OnRegister();
	HOUDINI_LOG_MESSAGE(TEXT("Registering component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	// Make sure we have a Houdini asset to operate with.
	if(HoudiniAsset)
	{
		// Make sure we are attached to a Houdini asset actor.
		AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(GetOwner());
		if(HoudiniAssetActor)
		{
			if(bIsNativeComponent)
			{
				// This is a native component ~ belonging to a c++ actor, make sure actor is not used for preview.
				if(!HoudiniAssetActor->IsUsedForPreview())
				{
					HOUDINI_LOG_MESSAGE(TEXT("Native::OnRegister"));

					// Check if we need to cook the asset and it has not been started already.
					if((-1 == AssetId) && !bIsCooking)
					{
						// We are starting cooking the asset.
						bIsCooking = true;
						
						// Create new runnable to perform the cooking from the referenced asset.
						FHoudiniAssetCooking* HoudiniAssetCooking = new FHoudiniAssetCooking(this, HoudiniAsset);

						// Create a new thread to execute our runnable.
						FRunnableThread* Thread = FRunnableThread::Create(HoudiniAssetCooking, TEXT("HoudiniAssetCooking"), true, true, 0, TPri_Normal);
					}
				}
			}
			else
			{
				// This is a dynamic component ~ part of blueprint.
				HOUDINI_LOG_MESSAGE(TEXT("Dynamic::OnRegister"));
			}
		}
	}
}


void 
UHoudiniAssetComponent::OnUnregister()
{
	Super::OnUnregister();
	HOUDINI_LOG_MESSAGE(TEXT("Unregistering component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void 
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	Super::OnComponentCreated();
	HOUDINI_LOG_MESSAGE(TEXT("Creating component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void 
UHoudiniAssetComponent::OnComponentDestroyed()
{
	// We need to destroy Houdini asset, if it has been created.
	if(-1 != AssetId)
	{
		HAPI_DestroyAsset(AssetId);
	}

	// If this instance has been monkey patched, restore the original class.
	if(OriginalClass)
	{
		MonkeyPatchClassInformation(OriginalClass);
	}

	Super::OnComponentDestroyed();
	HOUDINI_LOG_MESSAGE(TEXT("Destroying component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);	
}


void 
UHoudiniAssetComponent::GetComponentInstanceData(FComponentInstanceDataCache& Cache) const
{
	// Called before we throw away components during RerunConstructionScripts, to cache any data we wish to persist across that operation.
	Super::GetComponentInstanceData(Cache);
	HOUDINI_LOG_MESSAGE(TEXT("Requesting data for caching, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void 
UHoudiniAssetComponent::ApplyComponentInstanceData(const FComponentInstanceDataCache& Cache)
{
	// Called after we create new components during RerunConstructionScripts, to optionally apply any data backed up during GetComponentInstanceData.
	Super::ApplyComponentInstanceData(Cache);
	HOUDINI_LOG_MESSAGE(TEXT("Restoring data from caching, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}

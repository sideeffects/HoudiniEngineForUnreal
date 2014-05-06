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

#pragma once
#include "HoudiniAsset.generated.h"

UCLASS(Blueprintable, BlueprintType, EditInlineNew, HeaderGroup=HoudiniAsset, config=Editor) // collapsecategories, hidecategories=Object
class HOUDINIENGINE_API UHoudiniAsset : public UObject
{
	GENERATED_UCLASS_BODY()

//public:

	//UHoudiniAsset(const FPostConstructInitializeProperties& PCIP, const char* InAssetName, HAPI_AssetId InAssetId);

public:

	/** Given the provided raw OTL data, allocate sufficient buffer and store it. **/
	bool InitializeStorage(const uint8*& Buffer, const uint8* BufferEnd);
	
public: /** UObject methods. **/

	virtual void Serialize(FArchive& Ar) OVERRIDE;
	virtual void FinishDestroy() OVERRIDE;

public:

	/** Holds this asset's name. **/
	UPROPERTY(Transient, BlueprintReadOnly, Category = HoudiniAsset)
	FString AssetName;

protected:

	/** Buffer containing raw Houdini OTL data. **/
	uint8* HoudiniAssetBytes; 

	/** Field containing the size of raw Houdini OTL data in bytes. **/
	uint32 HoudiniAssetBytesCount;

	/** Holds this asset's handle. **/
	//HAPI_AssetId AssetId;
};

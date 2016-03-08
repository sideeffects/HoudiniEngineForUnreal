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
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterFile.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterFile : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterFile();

public:

	/** Create instance of this class. **/
	static UHoudiniAssetParameterFile* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

public:

	/** Create this parameter from HAPI information. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo) override;

#if WITH_EDITOR

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder) override;

#endif

	/** Upload parameter value to HAPI. **/
	virtual bool UploadParameterValue() override;

	/** Set parameter value. **/
	virtual bool SetParameterVariantValue(const FVariant& Variant, int32 Idx = 0, bool bTriggerModify = true,
		bool bRecordUndo = true) override;

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;

protected:

#if WITH_EDITOR

	void HandleFilePathPickerPathPicked(const FString& PickedPath, int32 Idx);

#endif

public:

	/** Return value of this property with optional fallback. **/
	const FString& GetParameterValue(int32 Idx, const FString& DefaultValue) const;

	/** Set value of this file property. **/
	void SetParameterValue(const FString& InValue, int32 Idx, bool bTriggerModify = true, bool bRecordUndo = true);

protected:

	/** Given a path check if it's relative. If it is, try to transform it into an absolute one. **/
	FString UpdateCheckRelativePath(const FString& PickedPath) const;

	/** Return filetype filter. **/
	FString ComputeFiletypeFilter(const FString& FilterList) const;

protected:

	/** Values of this property. **/
	TArray<FString> Values;

	/** Filters of this property. **/
	FString Filters;
};

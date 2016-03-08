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
#include "HoudiniAssetParameterString.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterString : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterString();

public:

	/** Create instance of this class. **/
	static UHoudiniAssetParameterString* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

public:

#if WITH_EDITOR

	/** Set value of this property, used by Slate. **/
	void SetValue(const FText& InValue, int32 Idx);

	/** Set value of this property through commit action, used by Slate. **/
	void SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 Idx);

#endif

protected:

	/** Values of this property. **/
	TArray<FString> Values;
};

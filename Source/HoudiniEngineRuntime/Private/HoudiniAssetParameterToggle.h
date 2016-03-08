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
#include "HoudiniAssetParameterToggle.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterToggle : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterToggle();

public:

	/** Create instance of this class. **/
	static UHoudiniAssetParameterToggle* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

public:

	/** Create this parameter from HAPI information. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo) override;

#if WITH_EDITOR

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder) override;

	/** Create widget for this parameter inside a given box. **/
	virtual void CreateWidget(TSharedPtr<SVerticalBox> VerticalBox);

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

	/** Get value of this property, used by Slate. **/
	void CheckStateChanged(ECheckBoxState NewState, int32 Idx);

	/** Return checked state of this property, used by Slate. **/
	ECheckBoxState IsChecked(int32 Idx) const;

#endif

protected:

	/** Values of this property. **/
	TArray<int32> Values;
};

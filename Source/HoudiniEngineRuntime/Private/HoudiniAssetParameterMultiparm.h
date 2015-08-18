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

#pragma once
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterMultiparm.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterMultiparm : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterMultiparm();

public:

	/** Create sintance of this class. **/
	static UHoudiniAssetParameterMultiparm* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;

public:

#if WITH_EDITOR

	/** Get value of this property, used by Slate. **/
	TOptional<int32> GetValue() const;

	/** Set value of this property, used by Slate. **/
	void SetValue(int32 InValue);

	/** Set value of this property through commit action, used by Slate. **/
	void SetValueCommitted(int32 InValue, ETextCommit::Type CommitType);

	/** Increment value, used by Slate. **/
	void AddElement();

	/** Decrement value, used by Slate. **/
	void RemoveElement();

#endif

protected:

	/** Value of this property. **/
	int32 Value;

};

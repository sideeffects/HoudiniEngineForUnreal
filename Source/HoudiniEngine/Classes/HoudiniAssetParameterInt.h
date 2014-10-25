/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Brokers define connection between assets (for example in content
 * browser) and actors.
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
#include "HoudiniAssetParameterInt.generated.h"


UCLASS()
class HOUDINIENGINE_API UHoudiniAssetParameterInt : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterInt();

public:

	/** Create sintance of this class. **/
	static UHoudiniAssetParameterInt* Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

public:

	/** Create this parameter from HAPI information. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo) override;

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder) override;

	/** Upload parameter value to HAPI. **/
	virtual bool UploadParameterValue() override;

public: /** UObject methods. **/

	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:

	/** Get value of this property, used by Slate. **/
	TOptional<int32> GetValue(int32 Idx) const;

	/** Set value of this property, used by Slate. **/
	void SetValue(int32 InValue, int32 Idx);

	/** Set value of this property through commit action, used by Slate. **/
	void SetValueCommitted(int32 InValue, ETextCommit::Type CommitType, int32 Idx);

	/** Delegate fired when slider for this property begins moving. **/
	void OnSliderMovingBegin(int32 Idx);

	/** Delegate fired when slider for this property has finished moving. **/
	void OnSliderMovingFinish(int32 InValue, int32 Idx);

protected:

	/** Values of this property. **/
	TArray<int32> Values;

	/** Min and Max values for this property. **/
	int32 ValueMin;
	int32 ValueMax;

	/** Min and Max values for UI for this property. **/
	int32 ValueUIMin;
	int32 ValueUIMax;
};

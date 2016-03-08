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
#include "HoudiniAssetParameterChoice.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterChoice : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterChoice();

public:

	/** Create instance of this class. **/
	static UHoudiniAssetParameterChoice* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	/** Retrieve value for string choice list, used by Slate. **/
	TOptional<TSharedPtr<FString> > GetValue(int32 Idx) const;

	/** Retrieve value for integer parameter. **/
	int32 GetParameterValueInt() const;

	/** Retrieve value for string parameter. **/
	const FString& GetParameterValueString() const;

	/** Return true if this is a string choice list. **/
	bool IsStringChoiceList() const;

	/** Set integer value. **/
	void SetValueInt(int32 Value, bool bTriggerModify = true, bool bRecordUndo = true);

#if WITH_EDITOR

	/** Helper method used to generate choice entry widget. **/
	TSharedRef<SWidget> CreateChoiceEntryWidget(TSharedPtr<FString> ChoiceEntry);

	/** Called when change of selection is triggered. **/
	void OnChoiceChange(TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType);

	/** Called to retrieve the name of selected item. **/
	FText HandleChoiceContentText() const;

#endif

protected:

	/** Choice values for this property. **/
	TArray<TSharedPtr<FString> > StringChoiceValues;

	/** Choice labels for this property. **/
	TArray<TSharedPtr<FString> > StringChoiceLabels;

	/** Value of this property. **/
	FString StringValue;

	/** Current value for this property. **/
	int32 CurrentValue;

	/** Is set to true when this choice list is a string choice list. **/
	bool bStringChoiceList;
};

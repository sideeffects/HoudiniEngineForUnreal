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
#include "HoudiniAssetParameterChoice.generated.h"


UCLASS()
class HOUDINIENGINE_API UHoudiniAssetParameterChoice : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterChoice();

public:

	/** Create sintance of this class. **/
	static UHoudiniAssetParameterChoice* Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

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

	/** Helper method used to generate choice entry widget. **/
	TSharedRef<SWidget> CreateChoiceEntryWidget(TSharedPtr<FString> ChoiceEntry);

	/** Called when change of selection is triggered. **/
	void OnChoiceChange(TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType);

	/** Called to retrieve the name of selected item. **/
	FString HandleChoiceContentText() const;

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

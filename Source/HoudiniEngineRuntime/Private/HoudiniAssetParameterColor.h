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
#include "HoudiniAssetParameterColor.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterColor : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterColor();

public:

	/** Create instance of this class. **/
	static UHoudiniAssetParameterColor* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	/** Return color for this color parameter. **/
	FLinearColor GetColor() const;

#if WITH_EDITOR

	/** Handle mouse click on color box. **/
	FReply OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

#endif

	/** Called when new color is selected. **/
	void OnPaintColorChanged(FLinearColor InNewColor);

protected:

#if WITH_EDITOR

	/** Color block widget. **/
	TSharedPtr<SColorBlock> ColorBlock;

#endif

	/** Color for this property. **/
	FLinearColor Color;
};

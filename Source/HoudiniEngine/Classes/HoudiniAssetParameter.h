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
#include "HoudiniAssetParameter.generated.h"

class FArchive;
class FReferenceCollector;
class IDetailCategoryBuilder;
class UHoudiniAssetComponent;


UCLASS()
class HOUDINIENGINE_API UHoudiniAssetParameter : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameter();

public:

	/** Create this parameter from HAPI information. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder);

	/** Upload parameter value to HAPI. **/
	virtual bool UploadParameterValue();

public:

	/** Return true if this parameter has been changed. **/
	bool HasChanged() const;

public: /** UObject methods. **/

	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

	/** Set parameter and node ids. **/
	void SetNodeParmIds(HAPI_NodeId InNodeId, HAPI_ParmId InParmId);

	/** Return true if parameter and node ids are valid. **/
	bool HasValidNodeParmIds() const;

	/** Set name and label. If label does not exist, name will be used instead for label. If error occurs, false will be returned. **/
	bool SetNameAndLabel(const HAPI_ParmInfo& ParmInfo);

	/** Check if parameter is visible. **/
	bool IsVisible(const HAPI_ParmInfo& ParmInfo) const;

	/** Helper function to retrieve parameter name from a given param info structure. Returns false if does not exist. **/
	bool RetrieveParameterName(const HAPI_ParmInfo& ParmInfo, FString& RetrievedName) const;

	/** Helper function to retrieve label name from a given param info structure. Returns false if does not exist. **/
	bool RetrieveParameterLabel(const HAPI_ParmInfo& ParmInfo, FString& RetrievedLabel) const;

	/** Mark this parameter as changed. This occurs when user modifies the value of this parameter through UI. **/
	void MarkChanged();

private:

	/** Helper function to retrieve HAPI string and convert it to Unreal one. **/
	bool RetrieveParameterString(HAPI_StringHandle StringHandle, FString& RetrievedName) const;

protected:

	/** Owner component. **/
	UHoudiniAssetComponent* HoudiniAssetComponent;

	/** Name of this parameter. **/
	FString Name;

	/** Label of this parameter. **/
	FString Label;

	/** Node this parameter belongs to. **/
	HAPI_NodeId NodeId;

	/** Id of this parameter. **/
	HAPI_ParmId ParmId;

	/** Is set to true if value of this parameter has been changed by user. **/
	bool bChanged;
};

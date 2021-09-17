/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "UObject/Object.h"
#include "Curves/RealCurve.h"
#include "HoudiniInput.h"
#include "HoudiniEngineRuntimeCommon.h"

#include "HoudiniParameter.generated.h"

UENUM()
enum class EHoudiniParameterType : uint8
{
	Invalid,

	Button,	
	ButtonStrip,
	Color,
	ColorRamp,
	File,
	FileDir,
	FileGeo,
	FileImage,
	Float,
	FloatRamp,
	Folder,
	FolderList,
	Input,
	Int,
	IntChoice,
	Label,
	MultiParm,	
	Separator,
	String,
	StringChoice,
	StringAssetRef,
	Toggle,
};



UCLASS(DefaultToInstanced)
class HOUDINIENGINERUNTIME_API UHoudiniParameter : public UObject
{

public:

	GENERATED_UCLASS_BODY()

	friend class UHoudiniAssetParameter;
	friend class FHoudiniEditorEquivalenceUtils;

	// 
	static UHoudiniParameter * Create(UObject* Outer, const FString& ParamName);

	// Equality, consider two param equals if they have the same name, type, tuple size and disabled status
	bool operator==(const UHoudiniParameter& other) const
	{
		return ( TupleSize == other.TupleSize && ParmType == other.ParmType
			&& Name.Equals(other.Name) && bIsDisabled == other.bIsDisabled );
	}

	bool Matches(const UHoudiniParameter& other) const { return (*this == other); };

	//------------------------------------------------------------------------------------------------
	// Accessors
	//------------------------------------------------------------------------------------------------

	// Get parent parameter for this parameter.
	virtual const FString & GetParameterName() const { return Name; };
	virtual const FString & GetParameterLabel() const { return Label; };
	virtual const FString GetParameterHelp() const { return Help; };

	virtual EHoudiniParameterType GetParameterType() const { return ParmType; };
	virtual int32 GetTupleSize() const { return TupleSize; };
	virtual int32 GetNodeId() const { return NodeId; };
	virtual int32 GetParmId() const { return ParmId; };
	virtual int32 GetParentParmId() const { return ParentParmId; };
	virtual int32 GetChildIndex() const { return ChildIndex; };

	virtual bool IsVisible() const { return bIsVisible; };
	virtual bool ShouldDisplay() const{ return bIsVisible && bIsParentFolderVisible && ParmType != EHoudiniParameterType::Invalid; };
	virtual bool IsDisabled() const { return bIsDisabled; };
	virtual bool HasChanged() const { return bHasChanged; };
	virtual bool NeedsToTriggerUpdate() const { return bNeedsToTriggerUpdate; };
	virtual bool IsDefault() const { return true; };
	virtual bool IsSpare() const { return bIsSpare; };
	virtual bool GetJoinNext() const { return bJoinNext; };
	virtual bool IsAutoUpdate() const { return bAutoUpdate; };

	virtual int32 GetTagCount() const { return TagCount; };
	virtual int32 GetValueIndex() const { return ValueIndex; };

	virtual TMap<FString, FString>& GetTags() { return Tags; };

	virtual bool IsChildParameter() const;

	virtual bool IsPendingRevertToDefault() const { return bPendingRevertToDefault; };
	virtual void GetTuplePendingRevertToDefault(TArray<int32>& OutArray) { OutArray = TuplePendingRevertToDefault; };

	virtual bool HasExpression() const { return bHasExpression; };
	virtual bool IsShowingExpression() const { return bShowExpression; };
	virtual FString GetExpression() const { return ParamExpression; };

	//------------------------------------------------------------------------------------------------
	// Mutators
	//------------------------------------------------------------------------------------------------

	// Set parent parameter for this parameter.
	virtual void SetParameterName(const FString& InName) { Name = InName; };
	virtual void SetParameterLabel(const FString& InLabel) { Label = InLabel; };
	virtual void SetParameterHelp(const FString& InHelp) { Help = InHelp; };

	virtual void SetParameterType(const EHoudiniParameterType& InType) { ParmType = InType; };
	virtual void SetTupleSize(const uint32& InTupleSize) { TupleSize = InTupleSize; };
	virtual void SetNodeId(const int32& InNodeId) { NodeId = InNodeId; };
	virtual void SetParmId(const int32& InParmId) { ParmId = InParmId; };
	virtual void SetParentParmId(const int32& InParentParmId) { ParentParmId = InParentParmId; };
	virtual void SetChildIndex(const int32& InChildIndex) { ChildIndex = InChildIndex; };

	virtual void SetIsChildOfMultiParm(const bool& IsChildOfMultiParam) { bIsChildOfMultiParm = IsChildOfMultiParam; };
	virtual bool GetIsChildOfMultiParm() const { return bIsChildOfMultiParm; };

	virtual void SetIsDirectChildOfMultiParm(const bool& IsDirectChildOfMultiParam) { bIsDirectChildOfMultiParm = IsDirectChildOfMultiParam; };
	virtual bool IsDirectChildOfMultiParm() const { return bIsDirectChildOfMultiParm; };

	virtual void SetVisible(const bool& InIsVisible) { bIsVisible = InIsVisible; };
	virtual void SetVisibleParent(const bool& InIsVisible) { bIsParentFolderVisible = InIsVisible; };
	virtual void SetDisabled(const bool& InIsDisabled) { bIsDisabled = InIsDisabled; };
	virtual void SetDefault(const bool& InIsDefault) { bIsDefault = InIsDefault; };
	virtual void SetSpare(const bool& InIsSpare) { bIsSpare = InIsSpare; };
	virtual void SetJoinNext(const bool& InJoinNext) { bJoinNext = InJoinNext; };

	virtual void SetTagCount(const uint32& InTagCount) { TagCount = InTagCount; };
	virtual void SetValueIndex(const uint32& InValueIndex) { ValueIndex = InValueIndex; };

	virtual void MarkChanged(const bool& bInChanged) { bHasChanged = bInChanged; SetNeedsToTriggerUpdate(bInChanged); };
	virtual void SetNeedsToTriggerUpdate(const bool& bInTriggersUpdate) { bNeedsToTriggerUpdate = bInTriggersUpdate; };
	virtual void RevertToDefault();
	virtual void RevertToDefault(const int32& TupleIndex);
	virtual void MarkDefault(const bool& bInDefault);

	virtual void SetHasExpression(const bool& InHasExpression) { bHasExpression = InHasExpression; };
	virtual void SetShowExpression(const bool& InShowExpression) { bShowExpression = InShowExpression; };
	virtual void SetExpression(const FString& InParamExpression) { ParamExpression = InParamExpression; };

	virtual void SetAutoUpdate(const bool& InAutoUpdate) { bAutoUpdate = InAutoUpdate; };

	static FString GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType InType);
	static EHoudiniRampInterpolationType GetHoudiniInterpMethodFromInt(int32 InInt);
	static EHoudiniRampInterpolationType GetHoudiniInterpMethodFromString(const FString& InString);

	static ERichCurveInterpMode EHoudiniRampInterpolationTypeToERichCurveInterpMode(EHoudiniRampInterpolationType InType);

	// Duplicate this object for state transfer between component instances and templates
	UHoudiniParameter* DuplicateAndCopyState(UObject* DestOuter, EObjectFlags InClearFlags=RF_NoFlags, EObjectFlags InSetFlags=RF_NoFlags);
	
	virtual void CopyStateFrom(UHoudiniParameter* InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags=RF_NoFlags, EObjectFlags InSetFlags=RF_NoFlags);
	
	// Replace any input references using the provided mapping
	virtual void RemapInputs(const TMap<UHoudiniInput*, UHoudiniInput*>& InputMapping) {};
	
	// Replace any parameter references using the provided mapping
	virtual void RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping) {};

	// Invalidate ids
	virtual void InvalidateData();

	//------------------------------------------------------------------------------------------------
	// Notifications
	//------------------------------------------------------------------------------------------------

	virtual void OnPreCook() {};

protected:

	//---------------------------------------------------------------------------------------------
	// ParmInfos
	//---------------------------------------------------------------------------------------------

	// 
	UPROPERTY()
	FString Name;

	// 
	UPROPERTY()
	FString Label;

	// Unreal type of the parameter
	UPROPERTY()
	EHoudiniParameterType ParmType;

	// Tuple size. For scalar parameters this value is 1, but for vector parameters this value can be greater.
	UPROPERTY()
	uint32 TupleSize;

	// Node this parameter belongs to.
	UPROPERTY(DuplicateTransient)
	int32 NodeId;

	// Id of this parameter.
	UPROPERTY(DuplicateTransient)
	int32 ParmId;

	// Id of parent parameter, -1 if root is parent.
	UPROPERTY(DuplicateTransient)
	int32 ParentParmId;

	// Child index within its parent parameter.
	UPROPERTY()
	int32 ChildIndex;

	// 
	UPROPERTY()
	bool bIsVisible;

	// Is visible in hierarchy. (e.g. parm can be visible, but containing folder is not)
	UPROPERTY()
	bool bIsParentFolderVisible;
	
	// 
	UPROPERTY()
	bool bIsDisabled;

	// Is set to true if value of this parameter has been changed by user.
	UPROPERTY()
	bool bHasChanged;

	// Is set to true if value of this parameter will trigger an update of the asset
	UPROPERTY()
	bool bNeedsToTriggerUpdate;

	// Indicates that this parameter is still using its default value
	UPROPERTY(Transient, DuplicateTransient)
	bool bIsDefault;

	// Permissions for file parms
	UPROPERTY()
	bool bIsSpare;

	// 
	UPROPERTY()
	bool bJoinNext;

	// 
	UPROPERTY()
	bool bIsChildOfMultiParm;

	UPROPERTY()
	bool bIsDirectChildOfMultiParm;

	// Indicates a parameter value needs to be reverted to its default
	UPROPERTY(DuplicateTransient)
	bool bPendingRevertToDefault;

	UPROPERTY(DuplicateTransient)
	TArray<int32> TuplePendingRevertToDefault;

	// 
	UPROPERTY()
	FString Help;

	// Number of tags on this parameter
	UPROPERTY()
	uint32 TagCount;	

	// The index to use to look into the values array in order to retrieve the actual value(s) of this parameter.
	UPROPERTY()
	int32 ValueIndex;

	//-------------------------------------------------------------------------------------------------------------------------
	// Expression
	// TODO: Use tuple array for this
	// Indicates the parameters has an expression value
	UPROPERTY()
	bool bHasExpression;

	// Indicates we are currently displaying the parameter's value
	UPROPERTY(DuplicateTransient)
	bool bShowExpression;

	// The parameter's expression
	UPROPERTY()
	FString ParamExpression;

	UPROPERTY()
	TMap<FString, FString> Tags;

	UPROPERTY()
	bool bAutoUpdate = true;


};



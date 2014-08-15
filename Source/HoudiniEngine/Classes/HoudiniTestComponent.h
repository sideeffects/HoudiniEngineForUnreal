/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once
#include "HoudiniTestComponent.generated.h"

class UActorComponent;
class UHoudiniTestComponent;

class FHoudiniTestComponentInstanceData : public FComponentInstanceDataBase
{
public:

	/** Constructor. **/
	FHoudiniTestComponentInstanceData(const UHoudiniTestComponent* SourceComponent) {}

	/** Destructor. **/
	virtual ~FHoudiniTestComponentInstanceData() {}

public:

	virtual bool MatchesComponent(const UActorComponent* Component) const override { return true; }

	/** **/

	int Value;
};

UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class HOUDINIENGINE_API UHoudiniTestComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UHoudiniTestComponent();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Value)
	int32 Value;

	int32 RememberedValue;

	mutable int32 Count;

	bool bIsDefaultClass;
	bool bIsBlueprintGeneratedClass;
	bool bIsBlueprintConstructionScriptClass;
	bool bIsBlueprintThumbnailSceneClass;

public: /** UObject methods. **/

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreSave() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;

private:
	/** Subscribe to Editor events. **/
	void SubscribeEditorDelegates();

	/** Unsubscribe from Editor events. **/
	void UnsubscribeEditorDelegates();

protected: /** UActorComponent methods. **/

	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed() override;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual FName GetComponentInstanceDataType() const override;
	virtual TSharedPtr<class FComponentInstanceDataBase> GetComponentInstanceData() const override;
	virtual void ApplyComponentInstanceData(TSharedPtr<class FComponentInstanceDataBase> ComponentInstanceData) override;

private: /** FEditorDelegates delegates. **/

	void OnPreSaveWorld(uint32 SaveFlags, class UWorld* World);
	void OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess);
	void OnPIEEventBegin(const bool bIsSimulating);
	void OnPIEEventEnd(const bool bIsSimulating);
};


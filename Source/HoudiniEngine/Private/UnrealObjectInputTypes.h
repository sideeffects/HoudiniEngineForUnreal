/*
* Copyright (c) <2023> Side Effects Software Inc.
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


#include "UnrealObjectInputRuntimeTypes.h"

#include "HAPI/HAPI_Common.h"

#include "UObject/ObjectPtr.h"

// Houdini Engine forward declarations

// Unreal Engine forward declarations
class AActor;
class UMeshComponent;
class UPrimitiveComponent;


/**
 * A modifier that adds:
 *   - an attribwrangle for setting unreal_material prim attribute to the overrides from the given
 *     MeshComponent by slots, or
 *   - an attribcreate for setting unreal_materialX point attributes to the overrides from the given
 *     MeshComponent by slots 
 */
class HOUDINIENGINE_API FUnrealObjectInputMaterialOverrides : public FUnrealObjectInputModifier
{
public:
	FUnrealObjectInputMaterialOverrides(FUnrealObjectInputNode& InOwner, UMeshComponent* InMC, bool bInUsePrimWrangle=true)
		: FUnrealObjectInputModifier(InOwner), MeshComponent(InMC), bUsePrimWrangle(bInUsePrimWrangle) {}

	static EUnrealObjectInputModifierType StaticGetType() { return EUnrealObjectInputModifierType::MaterialOverrides; }
	virtual EUnrealObjectInputModifierType GetType() const override { return StaticGetType(); }

	/** Set the MeshComponent. This set the modifier as requiring a rebuild. */
	void SetMeshComponent(UMeshComponent* InMeshComponent);
	UMeshComponent* GetMeshComponent() const { return MeshComponent; }

	/** Set whether to use a primitive wrangle or point attribcreate. */
	void SetUsePrimWrangle(bool bInUsePrimWrangle);
	/** Get whether to use a primitive wrangle or point attribcreate. */
	bool UsePrimWrangle() const { return bUsePrimWrangle; }

	virtual bool Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo) override;

protected:
	virtual bool UpdateAsPrimWrangle(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo);

	virtual bool UpdateAsPointAttribCreate(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo);

private:
	HAPI_NodeId EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId);
	
	/** The mesh component to get the material overrides from. */
	TObjectPtr<UMeshComponent> MeshComponent;

	/** Whether to use a primitive wrangle or to create point attributes per slot via attribcreate. */
	bool bUsePrimWrangle;
};


/** A modifier that adds an attribcreate for setting simple physical material override from a PrimitiveComponent. */
class HOUDINIENGINE_API FUnrealObjectInputPhysicalMaterialOverride : public FUnrealObjectInputModifier
{
public:
	FUnrealObjectInputPhysicalMaterialOverride(
			FUnrealObjectInputNode& InOwner, UPrimitiveComponent* InPrimitiveComponent, HAPI_AttributeOwner InAttrOwner)
		: FUnrealObjectInputModifier(InOwner), PrimitiveComponent(InPrimitiveComponent), AttributeOwner(InAttrOwner) {}

	static EUnrealObjectInputModifierType StaticGetType() { return EUnrealObjectInputModifierType::PhysicalMaterialOverride; }
	virtual EUnrealObjectInputModifierType GetType() const override { return StaticGetType(); }

	/**
	 * Setter for the primitive component from which to get the physical material override. Changes will mark the
	 * modifier as needing a rebuild.
	 */
	void SetPrimitiveComponent(UPrimitiveComponent* InPrimitiveComponent);
	/** Getter for the primitive component from which to get the physical material override. */
	UPrimitiveComponent* GetPrimitiveComponent() const { return PrimitiveComponent; }

	/** Setter for which class of attribute to create. Changes will mark the modifier as needing a rebuild.*/
	void SetAttributeOwner(HAPI_AttributeOwner InAttributeOwner);
	/** Getter for which class of attribute to create. */
	HAPI_AttributeOwner GetAttributeOwner() const { return AttributeOwner; }

	virtual bool Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo) override;
	
private:
	HAPI_NodeId EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId);

	/** The primitive component to get the physical material override from. */
	TObjectPtr<UPrimitiveComponent> PrimitiveComponent;

	/** The class of attribute to create. */
	HAPI_AttributeOwner AttributeOwner;
};


/** Modifier for adding level path and actor path attributes to actors that are imported by reference. */
class HOUDINIENGINE_API FUnrealObjectInputActorAsReference : public FUnrealObjectInputModifier
{
public:
	FUnrealObjectInputActorAsReference(FUnrealObjectInputNode& InOwner, AActor* InActor)
		: FUnrealObjectInputModifier(InOwner), Actor(InActor) {}

	static EUnrealObjectInputModifierType StaticGetType() { return EUnrealObjectInputModifierType::ActorAsReference; }
	virtual EUnrealObjectInputModifierType GetType() const override { return StaticGetType(); }

	/** Set the actor to get the actor and level path from. Changes will mark the modifier as requiring a rebuild. */
	void SetActor(AActor* InActor);
	/** Get the actor to get the actor and level path from. */
	AActor* GetActor() const { return Actor; }

	virtual bool Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo) override;

private:
	HAPI_NodeId EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId);

	/** The actor to get the actor and level path from. */
	TObjectPtr<AActor> Actor;
};


/** Modifier for data layer groups */
class HOUDINIENGINE_API FUnrealObjectInputDataLayer: public FUnrealObjectInputModifier
{
public:
	FUnrealObjectInputDataLayer(FUnrealObjectInputNode& InOwner, AActor* InActor)
		: FUnrealObjectInputModifier(InOwner), Actor(InActor) {}

	static EUnrealObjectInputModifierType StaticGetType() { return EUnrealObjectInputModifierType::DataLayerGroups; }
	virtual EUnrealObjectInputModifierType GetType() const override { return StaticGetType(); }

	void SetActor(AActor* InActor);
	AActor* GetActor() const { return Actor; }

	virtual bool Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo) override;

private:
	HAPI_NodeId EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId);

	TObjectPtr<AActor> Actor;
};

/** Modifier for HLODs */
class HOUDINIENGINE_API FUnrealObjectInputHLODAttributes : public FUnrealObjectInputModifier
{
public:
	FUnrealObjectInputHLODAttributes(FUnrealObjectInputNode& InOwner, AActor* InActor)
		: FUnrealObjectInputModifier(InOwner), Actor(InActor) {}

	static EUnrealObjectInputModifierType StaticGetType() { return EUnrealObjectInputModifierType::HLODAttributes; }
	virtual EUnrealObjectInputModifierType GetType() const override { return StaticGetType(); }

	void SetActor(AActor* InActor);
	AActor* GetActor() const { return Actor; }

	virtual bool Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo) override;

private:
	HAPI_NodeId EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId);

	TObjectPtr<AActor> Actor;
};

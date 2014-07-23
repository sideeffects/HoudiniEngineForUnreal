/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Represents a Houdini Asset in the UE4 Content Browser.
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
#include "HoudiniAssetMaterial.generated.h"

class UObject;
class UTexture2D;
class FReferenceCollector;

UCLASS()
class HOUDINIENGINE_API UHoudiniAssetMaterial : public UMaterial
{
	GENERATED_UCLASS_BODY()

public:

	/** Add texture to this material. **/
	void AddGeneratedTexture(UTexture2D* Texture);

public: /** UObject methods. **/

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

	/** Textures used by this material. **/
	TArray<UTexture2D*> GeneratedTextures;
};

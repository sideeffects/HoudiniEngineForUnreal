/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
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
#include "HoudiniGeoPartObject.h"
#include "HoudiniAssetComponentMaterials.generated.h"

class UMaterial;
class UMaterialInterface;
class UHoudiniAssetComponent;

UCLASS( EditInlineNew, config = Engine )
class HOUDINIENGINERUNTIME_API UHoudiniAssetComponentMaterials : public UObject
{
    GENERATED_UCLASS_BODY()

    friend class UHoudiniAssetComponent;
    friend struct FHoudiniEngineUtils;

    /** UObject methods. **/
    public:

        virtual void Serialize( FArchive & Ar ) override;
        static void AddReferencedObjects(
            UObject * InThis, FReferenceCollector & Collector);
    public:
        /** Duplicate this object for the given Outer.  Replace material references based on the given replacement map */
        class UHoudiniAssetComponentMaterials* Duplicate( class UHoudiniAssetComponent* InOuter, TMap<UObject*, UObject*>& InReplacements );

        /** Reset the object. **/
        void ResetMaterialInfo();

    protected:

        /** Material assignments. **/
        TMap< FString, UMaterialInterface * > Assignments;

        /** Material replacements. **/
        TMap< FHoudiniGeoPartObject, TMap< FString, UMaterialInterface * > > Replacements;

        /** Flags used by this instance. **/
        uint32 HoudiniAssetComponentMaterialsFlagsPacked;
};

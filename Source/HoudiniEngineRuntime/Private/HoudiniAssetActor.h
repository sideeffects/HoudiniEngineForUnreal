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
*      Damian Campeanu, Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once
#include "HoudiniAssetActor.generated.h"

class UHoudiniAssetComponent;

UCLASS( hidecategories = (Input), ConversionRoot, meta = (ChildCanTick) )
class HOUDINIENGINERUNTIME_API AHoudiniAssetActor : public AActor
{
    GENERATED_UCLASS_BODY()

    UPROPERTY( Category = HoudiniAssetActor, VisibleAnywhere, BlueprintReadOnly,
        meta = ( ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|HoudiniAsset" ) )
    UHoudiniAssetComponent * HoudiniAssetComponent;

    public:

        /** Return true if this actor is used for preview. **/
        bool IsUsedForPreview() const;

        /** Return actor component. **/
        UHoudiniAssetComponent* GetHoudiniAssetComponent() const;

        /** Reset actor's playtime. This time is used to set time and cook when in playmode. **/
        void ResetHoudiniCurrentPlaytime();

        /** Return actor's playtime. **/
        float GetHoudiniCurrentPlaytime() const;

    /** AActor methods. **/
    public:

#if WITH_EDITOR

        virtual bool ShouldImport( FString * ActorPropString, bool IsMovingLevel ) override;
        virtual bool ShouldExport() override;

        /** Used by the "Sync to Content Browser" right-click menu option in the editor. **/
        virtual bool GetReferencedContentObjects( TArray< UObject * >& Objects ) const;

        /** Called after a property has been changed **/
        virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;

#endif

    protected:

        /** Used to track current playtime. **/
        float CurrentPlayTime;
};

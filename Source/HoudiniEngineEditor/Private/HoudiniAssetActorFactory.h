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
#include "HoudiniAssetActorFactory.generated.h"

class FText;
class AActor;
class UObject;
class FAssetData;

UCLASS( config = Editor )
class UHoudiniAssetActorFactory : public UActorFactory
{
    GENERATED_UCLASS_BODY()

    /** UActorFactory methods. **/
    public:

        /** Return true if Actor can be created from a given asset. **/
        virtual bool CanCreateActorFrom( const FAssetData & AssetData, FText & OutErrorMsg ) override;

        /** Given an instance of an actor pertaining to this factory, find the asset that should be used to create a new actor. **/
        virtual UObject * GetAssetFromActorInstance( AActor * Instance ) override;

        /** Subclasses may implement this to modify the actor after it has been spawned. **/
        virtual void PostSpawnActor( UObject * Asset, AActor * NewActor ) override;

        /** Override this in derived factory classes if needed.  This is called after a blueprint is created by this factory to **/
        /** update the blueprint's CDO properties with state from the asset for this factory.                                   **/
        virtual void PostCreateBlueprint( UObject * Asset, AActor * CDO ) override;
};

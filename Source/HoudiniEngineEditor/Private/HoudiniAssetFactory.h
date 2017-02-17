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
#include "EditorReimportHandler.h"
#if WITH_EDITOR
#include "Factories/Factory.h"
#endif
#include "HoudiniAssetFactory.generated.h"

class UClass;
class UObject;
class FFeedbackContext;

UCLASS( config = Editor )
class UHoudiniAssetFactory : public UFactory, public FReimportHandler
{
    GENERATED_UCLASS_BODY()

    /** UFactory methods. **/
    private:

        virtual bool DoesSupportClass( UClass * Class ) override;
        virtual FText GetDisplayName() const override;
        virtual UObject * FactoryCreateBinary(
            UClass * InClass, UObject * InParent, FName InName, EObjectFlags Flags,
            UObject * Context, const TCHAR * Type, const uint8 *& Buffer, const uint8 * BufferEnd,
            FFeedbackContext * Warn ) override;

    /** FReimportHandler methods. **/
    public:

        virtual bool CanReimport( UObject * Obj, TArray< FString > & OutFilenames ) override;
        virtual void SetReimportPaths( UObject * Obj, const TArray< FString > & NewReimportPaths ) override;
        virtual EReimportResult::Type Reimport( UObject * Obj ) override;
};

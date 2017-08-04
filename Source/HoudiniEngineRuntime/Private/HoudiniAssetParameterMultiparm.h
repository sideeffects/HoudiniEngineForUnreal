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
*/

#pragma once

#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterMultiparm.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterMultiparm : public UHoudiniAssetParameter
{
    GENERATED_UCLASS_BODY()

    friend struct FHoudiniParameterDetails;

    public:

        /** Create instance of this class. **/
        static UHoudiniAssetParameterMultiparm * Create(
            UObject * InPrimaryObject,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo );

    public:

        /** Create this parameter from HAPI information. **/
        virtual bool CreateParameter(
            UObject * InPrimaryObject,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo ) override;

#if WITH_EDITOR

        /** Add multiparm instance. **/
        void AddMultiparmInstance( int32 ChildMultiparmInstanceIndex );

        /** Remove multiparm instance. **/
        void RemoveMultiparmInstance( int32 ChildMultiparmInstanceIndex );

#endif

        /** Upload parameter value to HAPI. **/
        virtual bool UploadParameterValue() override;

    /** UObject methods. **/
    public:

        virtual void Serialize( FArchive & Ar ) override;

#if WITH_EDITOR

        virtual void PostEditUndo() override;

#endif

        /** Get value of this property, used by Slate. **/
        TOptional< int32 > GetValue() const;

        /** Set value of this property, used by Slate. **/
        void SetValue( int32 InValue );

        /** Increment value, used by Slate. **/
        void AddElement( bool bTriggerModify = true, bool bRecordUndo = true );
        void AddElements( int32 NumElements, bool bTriggerModify = true, bool bRecordUndo = true );

        /** Decrement value, used by Slate. **/
        void RemoveElement( bool bTriggerModify = true, bool bRecordUndo = true );
        void RemoveElements( int32 NumElements, bool bTriggerModify = true, bool bRecordUndo = true );

    protected:

        /** Value of this property. **/
        int32 MultiparmValue;

    private:

        enum ModificationType
        {
            RegularValueChange,
            InstanceAdded,
            InstanceRemoved
        };

        /** Last modification type. **/
        ModificationType LastModificationType;

        /** Last remove/add instance index. **/
        int32 LastRemoveAddInstanceIndex;

};

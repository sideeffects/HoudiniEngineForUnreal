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
*      Damian Campeanu
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once

#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterMultiparm.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterMultiparm : public UHoudiniAssetParameter
{
    GENERATED_UCLASS_BODY()

    public:

        /** Destructor. **/
        virtual ~UHoudiniAssetParameterMultiparm();

    public:

        /** Create instance of this class. **/
        static UHoudiniAssetParameterMultiparm * Create(
            UHoudiniAssetComponent * InHoudiniAssetComponent,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo );

    public:

        /** Create this parameter from HAPI information. **/
        virtual bool CreateParameter(
            UHoudiniAssetComponent * InHoudiniAssetComponent,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo ) override;

#if WITH_EDITOR

        /** Create widget for this parameter and add it to a given category. **/
        virtual void CreateWidget( IDetailCategoryBuilder & DetailCategoryBuilder ) override;

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

    public:

#if WITH_EDITOR

        /** Set value of this property through commit action, used by Slate. **/
        void SetValueCommitted( int32 InValue, ETextCommit::Type CommitType );

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

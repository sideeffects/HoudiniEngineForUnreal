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
#include "HoudiniAssetParameter.generated.h"

class FArchive;
class FVariant;
class FReferenceCollector;

UCLASS( config = Editor )
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameter : public UObject
{
    GENERATED_UCLASS_BODY()

    friend struct FHoudiniParameterDetails;
    friend class UHoudiniAssetComponent;

    public:

        /** Create this parameter from HAPI information. **/
        virtual bool CreateParameter(
            UObject * InPrimaryObject,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId,
            const HAPI_ParmInfo & ParmInfo );

        /** Create a duplicate of this parameter for the given parent component, takes care of any subobjects */
        virtual UHoudiniAssetParameter * Duplicate( UObject* InOuter );

        /** Set component for this parameter. **/
        virtual void SetHoudiniAssetComponent( class UHoudiniAssetComponent * InComponent );

        /** Set parent parameter for this parameter. **/
        void SetParentParameter( UHoudiniAssetParameter * InParentParameter );

        /** Return parent parameter for this parameter, if there's one. **/
        UHoudiniAssetParameter * GetParentParameter() const;

        /** Upload parameter value to HAPI. **/
        virtual bool UploadParameterValue();

        /** Set parameter value. **/
        virtual bool SetParameterVariantValue(
            const FVariant & Variant,
            int32 Idx = 0,
            bool bTriggerModify = true,
            bool bRecordUndo = true );

        /** Notification from a child parameter about its change. **/
        virtual void NotifyChildParameterChanged( UHoudiniAssetParameter * HoudiniAssetParameter );

        /** Notification from component that all child parameters have been created. **/
        virtual void NotifyChildParametersCreated();

    public:

        /** Return true if this parameter has been changed. **/
        virtual bool HasChanged() const;

        /** Return hash value for this object, used when using this object as a key inside hashing containers. **/
        uint32 GetTypeHash() const;

        /** Return parameter id of this parameter. **/
        HAPI_ParmId GetParmId() const;

        /** Return parameter id of a parent of this parameter. **/
        HAPI_ParmId GetParmParentId() const;

        /** Return true if parent parameter exists for this parameter. **/
        bool IsChildParameter() const;

        /** Return parameter name. **/
        const FString & GetParameterName() const;

        /** Return label name. **/
        const FString & GetParameterLabel() const;

        /** Return parameter help **/
        const FString & GetParameterHelp() const;

        /** Update parameter's node id. This is necessary after parameter is loaded. **/
        void SetNodeId( HAPI_NodeId InNodeId );

        /** Mark this parameter as unchanged. **/
        void UnmarkChanged();

        /** Reset array containing all child parameters. **/
        void ResetChildParameters();

        /** Add a child parameter. **/
        void AddChildParameter( UHoudiniAssetParameter * HoudiniAssetParameter );

        /** Return true if given parameter is an active child parameter. **/
        bool IsActiveChildParameter( UHoudiniAssetParameter * ChildParameter ) const;

        /** Return true if this parameter contains child parameters. **/
        bool HasChildParameters() const;

        /** Return true if this is Substance parameter. **/
        bool IsSubstanceParameter() const;

        /** Return tuple size. **/
        int32 GetTupleSize() const;

    /** UObject methods. **/
    public:

        virtual void Serialize( FArchive & Ar ) override;
        static void AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector );

#if WITH_EDITOR

        virtual void PostEditUndo() override;

#endif // WITH_EDITOR

    protected:

        /** Set parameter and node ids. **/
        void SetNodeParmIds( HAPI_NodeId InNodeId, HAPI_ParmId InParmId );

        /** Return true if parameter and node ids are valid. **/
        bool HasValidNodeParmIds() const;

        /** Set name and label. If label does not exist, name will be used instead for label. If error occurs, false will be returned. **/
        bool SetNameAndLabel( const HAPI_ParmInfo & ParmInfo );

        /** Set name and label to be same value from string handle. **/
        bool SetNameAndLabel( HAPI_StringHandle StringHandle );

        /** Set name and label. **/
        bool SetNameAndLabel( const FString & Name );

        /** Set the parameter's help (tooltip)**/
        bool SetHelp( const HAPI_ParmInfo & ParmInfo );

        /** Check if parameter is visible. **/
        bool IsVisible( const HAPI_ParmInfo & ParmInfo ) const;

        /** Mark this parameter as pre-changed. This occurs when user modifies the value of this parameter through UI, but before it is saved. **/
        void MarkPreChanged( bool bMarkAndTriggerUpdate = true );

        /** Mark this parameter as changed. This occurs when user modifies the value of this parameter through UI. **/
        void MarkChanged( bool bMarkAndTriggerUpdate = true );

        /** Return true if this parameter is an array (has tuple size larger than one). **/
        bool IsArray() const;

        /** Sets internal value index used by this parameter. **/
        void SetValuesIndex( int32 InValuesIndex );

        /** Return index of active child parameter. **/
        int32 GetActiveChildParameter() const;

        /** Called when state of the parameter changes as side-effect of some action */
        void OnParamStateChanged();

        /** Return true if parameter is spare, that is, created by Houdini Engine only. **/
        bool IsSpare() const;

        /** Return true if parameter is disabled. **/
        bool IsDisabled() const;

    protected:

        /** Array containing all child parameters. **/
        TArray< UHoudiniAssetParameter * > ChildParameters;

        /** Owner component. **/
        UObject * PrimaryObject;

        /** Parent parameter. **/
        UHoudiniAssetParameter * ParentParameter;

        /** Name of this parameter. **/
        FString ParameterName;

        /** Label of this parameter. **/
        FString ParameterLabel;

        /** Node this parameter belongs to. **/
        HAPI_NodeId NodeId;

        /** Id of this parameter. **/
        HAPI_ParmId ParmId;

        /** Id of parent parameter, -1 if root is parent. **/
        HAPI_ParmId ParmParentId;

        /** Child index within its parent parameter. **/
        int32 ChildIndex;

        /** Tuple size - arrays. **/
        int32 TupleSize;

        /** Internal HAPI cached value index. **/
        int32 ValuesIndex;

        /** The multiparm instance index. **/
        int32 MultiparmInstanceIndex;

        /** Active child parameter. **/
        int32 ActiveChildParameter;

        /** The parameter's help, to be used as a tooltip **/
        FString ParameterHelp;

        /** Flags used by this parameter. **/
        union
        {
            struct
            {
                /** Is set to true if this parameter is spare, that is, created by Houdini Engine only. **/
                uint32 bIsSpare : 1;

                /** Is set to true if this parameter is disabled. **/
                uint32 bIsDisabled : 1;

                /** Is set to true if value of this parameter has been changed by user. **/
                uint32 bChanged : 1;

                /** Is set to true when parameter's slider (if it has one) is being dragged. Transient. **/
                uint32 bSliderDragged : 1;

                /** Is set to true if the parameter is a multiparm child parameter. **/
                uint32 bIsChildOfMultiparm : 1;

                /** Is set to true if this parameter is a Substance parameter. **/
                uint32 bIsSubstanceParameter : 1;

                /** Is set to true if this parameter is a multiparm **/
                uint32 bIsMultiparm : 1;
            };

            uint32 HoudiniAssetParameterFlagsPacked;
        };

        /** Temporary variable holding parameter serialization version. **/
        uint32 HoudiniAssetParameterVersion;
};


/** Function used by hasing containers to create a unique hash for this type of object. **/
uint32 GetTypeHash( const UHoudiniAssetParameter * HoudiniAssetParameter );

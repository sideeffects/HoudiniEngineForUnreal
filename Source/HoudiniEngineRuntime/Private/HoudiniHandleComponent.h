/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once
#include "HoudiniGeoPartObject.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniHandleComponent.generated.h"

class UHoudiniAssetInput;

UCLASS(config=Engine)
class HOUDINIENGINERUNTIME_API UHoudiniHandleComponent : public USceneComponent
{
	friend class UHoudiniAssetComponent;

#if WITH_EDITOR

	friend class FHoudiniHandleComponentVisualizer;

#endif

	GENERATED_UCLASS_BODY()

	virtual ~UHoudiniHandleComponent();

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR

	virtual void PostEditUndo() override;

#endif

public:
	bool Construct( HAPI_AssetId AssetId, int32 HandleIdx, const FString& HandleName,
					const HAPI_HandleInfo&, const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& );

	void ResolveDuplicatedParameters(const TMap<HAPI_ParmId, UHoudiniAssetParameter*>&);

	// Update HAPI transform handle parameters from the current ComponentToWorld Unreal transform
	void UpdateTransformParameters();

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

private:
	static HAPI_RSTOrder GetHapiRSTOrder(const TSharedPtr<FString>&);
	static HAPI_XYZOrder GetHapiXYZOrder(const TSharedPtr<FString>&);

	template <class ASSET_PARM>
	class THandleParameter
	{
	public:
		THandleParameter()
			: AssetParameter(nullptr)
			, TupleIdx(0)
		{}

		void AddReferencedObject(FReferenceCollector& Collector, const UObject* ReferencingObject)
		{
			if ( AssetParameter )
			{
				Collector.AddReferencedObject(AssetParameter, ReferencingObject);
			}
		}

		friend FArchive& operator<<(FArchive& Ar, THandleParameter& InThis)
		{
			Ar << InThis.AssetParameter;
			Ar << InThis.TupleIdx;

			return Ar;
		}

		template <typename VALUE>
		bool Bind(
			VALUE& OutValue,
			const char* CmpName,
			int32 InTupleIdx,
			const FString& HandleParmName,
			HAPI_ParmId AssetParamId,
			const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& Parameters )
		{			
			if (HandleParmName == CmpName)
			{
				if ( UHoudiniAssetParameter* const* FoundAbstractParm = Parameters.Find(AssetParamId) )
				{
					AssetParameter = Cast<ASSET_PARM>(*FoundAbstractParm);
					if ( AssetParameter )
					{
						auto Optional = AssetParameter->GetValue(InTupleIdx);
						if ( Optional.IsSet() )
						{
							TupleIdx = InTupleIdx;
							OutValue = static_cast<VALUE>( Optional.GetValue() );
							return true;
						}
					}
				}
			}

			return false;
		}

		void ResolveDuplicated(const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& NewParameters)
		{
			if (AssetParameter)
			{
				if ( UHoudiniAssetParameter* const* FoundNewParameter = NewParameters.Find(AssetParameter->GetParmId()) )
				{
					AssetParameter = Cast<ASSET_PARM>(*FoundNewParameter);
				}
				else
				{
					AssetParameter = nullptr;
				}
			}
		}

		template <typename VALUE>
		VALUE Get(VALUE DefaulValue) const
		{
			if ( AssetParameter )
			{
				auto Optional = AssetParameter->GetValue(TupleIdx);
				if ( Optional.IsSet() )
				{
					return static_cast<VALUE>(Optional.GetValue());
				}
			}

			return DefaulValue;
		}

		template <typename VALUE>
		THandleParameter& operator=(VALUE Value)
		{
			if ( AssetParameter )
			{
				AssetParameter->SetValue( Value, TupleIdx );
			}

			return *this;
		}
		
		ASSET_PARM* AssetParameter;
		int32 TupleIdx;
	};

	struct EXformParameter
	{
		enum Type
		{
			TX, TY, TZ,
			RX, RY, RZ,
			SX, SY, SZ,
			COUNT
		};		
	};

	typedef THandleParameter<UHoudiniAssetParameterFloat> FXformParameter;
	FXformParameter XformParms[EXformParameter::COUNT];

	THandleParameter<UHoudiniAssetParameterChoice> RSTParm, RotOrderParm;
};

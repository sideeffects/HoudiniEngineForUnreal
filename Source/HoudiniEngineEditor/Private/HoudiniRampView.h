/*
* Copyright (c) <2024> Side Effects Software Inc.
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

#include "CoreMinimal.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineDetails.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineRuntimeCommon.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAlgorithm.h"

#include "HoudiniParameterRamp.h"

/**
 * The purpose of the ramp view is to provide a nicer way to interface with an array of ramp
 * parameters, where the array represents a multi-selection of ramp parameters.
 */
template<typename Derived, typename InValueType, typename InParameterType, typename InPointType>
class THoudiniRampViewBase
{
public:

	using ValueType = InValueType;
	using ParameterType = InParameterType;
	using ParameterWeakPtr = TWeakObjectPtr<ParameterType>;
	using PointType = InPointType;

private:

	TArray<ParameterWeakPtr> Parameters;

public:

	explicit THoudiniRampViewBase(TArrayView<const ParameterWeakPtr> Parameters)
		: Parameters(Parameters)
	{
	}

	const TArray<ParameterWeakPtr>&
	GetParameters() const
	{
		return Parameters;
	}

	ParameterWeakPtr
	GetMainParameter() const
	{
		if (Parameters.IsEmpty())
		{
			return nullptr;
		}

		return Parameters[0];
	}

	PointType*
	GetRampPoint(const int32 Index) const
	{
		const ParameterWeakPtr MainParameter = GetMainParameter();
		if (!IsValidWeakPointer(MainParameter))
		{
			return nullptr;
		}

		const bool bIsCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

		const TArray<PointType*>& Points = (MainParameter->IsAutoUpdate() && bIsCookingEnabled)
			? MainParameter->Points
			: MainParameter->CachedPoints;

		return Points.IsValidIndex(Index) ? Points[Index] : nullptr;
	}

	/** Gets the number of points the ramp currently has. */
	int32
	GetPointCount() const
	{
		const bool bIsCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

		const ParameterWeakPtr MainParameter = GetMainParameter();
		if (!IsValidWeakPointer(MainParameter))
		{
			return 0;
		}

		if (MainParameter->IsAutoUpdate() && bIsCookingEnabled)
		{
			return MainParameter->Points.Num();
		}
		else
		{
			return MainParameter->CachedPoints.Num();
		}
	}

	bool
	InsertRampPoint(const int32 Index)
	{
		const ParameterWeakPtr MainParameter = GetMainParameter();
		if (!IsValidWeakPointer(MainParameter))
		{
			return false;
		}

		const bool bIsCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

		float InsertPosition = 0.f;
		ValueType InsertValue = Derived::DefaultInsertValue;
		auto InsertInterp = EHoudiniRampInterpolationType::LINEAR;

		float PrevPosition = 0.0f;
		float NextPosition = 1.0f;

		TArray<TObjectPtr<PointType>>& CurrentPoints = MainParameter->Points;
		TArray<TObjectPtr<PointType>>& CachedPoints = MainParameter->CachedPoints;

		const int32 NumPoints = GetPointCount();

		if (!MainParameter->IsAutoUpdate() || !bIsCookingEnabled)
		{
			MainParameter->SetCaching(true);
		}

		if (Index >= NumPoints)
		{
			// Insert at the end
			if (NumPoints > 0)
			{
				PointType* PrevPoint = nullptr;
				if (MainParameter->IsAutoUpdate() && bIsCookingEnabled)
					PrevPoint = CurrentPoints.Last();
				else
					PrevPoint = CachedPoints.Last();

				if (PrevPoint)
				{
					PrevPosition = PrevPoint->GetPosition();
					InsertInterp = PrevPoint->GetInterpolation();
				}
			}
		}
		else if (Index <= 0)
		{
			// Insert at the beginning
			if (NumPoints > 0)
			{
				PointType* NextPoint = nullptr;
				if (MainParameter->IsAutoUpdate() && bIsCookingEnabled)
					NextPoint = CurrentPoints[0];
				else
					NextPoint = CachedPoints[0];

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
					InsertInterp = NextPoint->GetInterpolation();
				}
			}
		}
		else
		{
			// Insert in the middle
			if (NumPoints > 1)
			{
				PointType* PrevPoint = nullptr;
				PointType* NextPoint = nullptr;

				if (MainParameter->IsAutoUpdate() && bIsCookingEnabled)
				{
					PrevPoint = CurrentPoints[Index - 1];
					NextPoint = CurrentPoints[Index];
				}
				else
				{
					PrevPoint = CachedPoints[Index - 1];
					NextPoint = CachedPoints[Index];
				}

				if (PrevPoint)
				{
					PrevPosition = PrevPoint->GetPosition();
					InsertInterp = PrevPoint->GetInterpolation();
				}

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
				}

				if (PrevPoint && NextPoint)
				{
					InsertValue = (PrevPoint->GetValue() + NextPoint->GetValue()) / 2.0;
				}
			}
		}

		InsertPosition = (PrevPosition + NextPosition) / 2.0f;

		return InsertRampPoint(Index, InsertPosition, InsertValue, InsertInterp);
	}

	bool
	InsertRampPoint(
		const int32 Index,
		const float Position,
		const ValueType Value,
		const EHoudiniRampInterpolationType InterpolationType)
	{
		const bool bIsCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

		ReplaceAllParameterPointsWithMainParameter();

		for (const ParameterWeakPtr Parameter : Parameters)
		{
			if (!IsValidWeakPointer(Parameter))
				continue;

			if (Parameter->IsAutoUpdate() && bIsCookingEnabled)
			{
				Parameter->CreateInsertEvent(Position, Value, InterpolationType);
				Parameter->MarkChanged(true);
			}
			else
			{
				PointType* NewCachedPoint = NewObject<PointType>(Parameter.Get(), PointType::StaticClass());
				NewCachedPoint->Position = Position;
				NewCachedPoint->Value = Value;
				NewCachedPoint->Interpolation = InterpolationType;

				Parameter->CachedPoints.Insert(NewCachedPoint, Index);
				Parameter->SetCaching(true);
				if (!bIsCookingEnabled)
				{
					// If cooking is not enabled, be sure to mark this parameter as changed
					// so that it triggers an update once cooking is enabled again.
					Parameter->MarkChanged(true);
				}
			}
		}

		return true;
	}

	bool
	DeleteRampPoints(const TArrayView<const int32> Indices)
	{
		const ParameterWeakPtr MainParameter = GetMainParameter();
		if (!IsValidWeakPointer(MainParameter))
		{
			return false;
		}

		const bool bIsCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

		ReplaceAllParameterPointsWithMainParameter();

		for (const ParameterWeakPtr Parameter : Parameters)
		{
			if (!IsValidWeakPointer(Parameter))
				continue;

			if (Parameter->IsAutoUpdate() && bIsCookingEnabled)
			{
				if (Parameter->Points.Num() == 0)
					return false;

				for (const int32 Index : Indices)
				{
					PointType* PointToDelete = nullptr;

					if (Index == -1)
						PointToDelete = Parameter->Points.Last();
					else if (Parameter->Points.IsValidIndex(Index))
						PointToDelete = Parameter->Points[Index];

					if (!PointToDelete)
						return false;

					const int32 InstanceIndexToDelete = PointToDelete->InstanceIndex;

					Parameter->CreateDeleteEvent(InstanceIndexToDelete);
					Parameter->MarkChanged(true);
				}
			}
			else
			{
				if (Parameter->CachedPoints.Num() == 0)
				{
					return false;
				}

				int32 OldPointCount = Parameter->CachedPoints.Num();

				if (Indices.Contains(-1))
				{
					Parameter->CachedPoints.Pop();
				}

				// The algorithm just shuffles the items to the front. Afterwards we need to remove
				// the end of the array - otherwise we have items in undefined state at the end.
				int32 NewPointCount = Algo::StableRemoveIfByIndex(
					Parameter->CachedPoints,
					[Indices](const int32 Index) -> bool { return Indices.Contains(Index); });

				// Trim the end of the array
				Parameter->CachedPoints.SetNum(NewPointCount);

				if (NewPointCount == OldPointCount)
				{
					return false;
				}

				Parameter->SetCaching(true);
				if (!bIsCookingEnabled)
					Parameter->MarkChanged(true);
			}
		}

		return true;
	}

	bool
	DeleteRampPoint(const int32 Index)
	{
		return DeleteRampPoints(MakeArrayView(&Index, 1));
	}

	/** @returns Position if successful, unset optional if unsuccessful. */
	TOptional<float>
	GetRampPointPosition(const int32 Index) const
	{
		if (PointType* const Point = GetRampPoint(Index))
		{
			return Point->Position;
		}

		return {};
	}

	/** @returns Value if successful, unset optional if unsuccessful. */
	TOptional<ValueType>
	GetRampPointValue(const int32 Index) const
	{
		if (PointType* const Point = GetRampPoint(Index))
		{
			return Point->Value;
		}

		return {};
	}

	/** @returns Interpolation if successful, unset optional if unsuccessful. */
	TOptional<EHoudiniRampInterpolationType>
	GetRampPointInterpolationType(const int32 Index) const
	{
		if (PointType* const Point = GetRampPoint(Index))
		{
			return Point->Interpolation;
		}

		return {};
	}

public:

	/**
	 * Sets position of individual ramp point. If setting multiple points, it is better to use
	 * @ref SetRampPoints as there is less overhead.
	 *
	 * @returns true if a change was made, false otherwise.
	 */
	bool
	SetRampPointPosition(const int32 Index, const float NewPosition)
	{
		return SetRampPoints(
			TArray({ Index }),
			TArray({ TOptional<float>(NewPosition) }),
			TArray({ TOptional<ValueType>() }),
			TArray({ TOptional<EHoudiniRampInterpolationType>() }));
	}

	/**
	 * Sets value of individual ramp point. If setting multiple points, it is better to use
	 * @ref SetRampPoints as there is less overhead.
	 *
	 * @returns true if a change was made, false otherwise.
	 */
	bool
	SetRampPointValue(const int32 Index, const ValueType NewValue)
	{
		return SetRampPoints(
			TArray({ Index }),
			TArray({ TOptional<float>() }),
			TArray({ TOptional<ValueType>(NewValue) }),
			TArray({ TOptional<EHoudiniRampInterpolationType>() }));
	}

	/**
	 * Sets interpolation of individual ramp point. If setting multiple points, it is better to use
	 * @ref SetRampPoints as there is less overhead.
	 *
	 * @returns true if a change was made, false otherwise.
	 */
	bool
	SetRampPointInterpolationType(
			const int32 Index, const EHoudiniRampInterpolationType NewInterpolationType)
	{
		return SetRampPoints(
			TArray({ Index }),
			TArray({ TOptional<float>() }),
			TArray({ TOptional<ValueType>() }),
			TArray({ TOptional<EHoudiniRampInterpolationType>(NewInterpolationType) }));
	}

	/**
	 * Sets data of multiple points. All input arrays must be of equal size. For each index, `i`, of
	 * the input arrays, the point at index `Indices[i]` will be updated with new position
	 * `NewPositions[i]`, new value `NewValues[i]`, and so on. Unset optional values are used to
	 * indicate that no change is desired.
	 *
	 * @returns true if at least one point was changed, false otherwise.
	 */
	bool
		SetRampPoints(
			const TArrayView<const int32> Indices,
			const TArrayView<const TOptional<float>> NewPositions,
			const TArrayView<const TOptional<ValueType>> NewValues,
			const TArrayView<const TOptional<EHoudiniRampInterpolationType>> NewInterpolationTypes)
	{
		// All arrays must have same size.
		if (Indices.Num() != NewPositions.Num()
			|| Indices.Num() != NewValues.Num()
			|| Indices.Num() != NewInterpolationTypes.Num())
		{
			return false;
		}

		if (Indices.IsEmpty())
		{
			return false;
		}

		const bool bIsCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

		bool bWasPointsChanged = false;

		ReplaceAllParameterPointsWithMainParameter();

		for (int32 i = 0; i < Indices.Num(); ++i)
		{
			PointType* const MainPoint = GetRampPoint(Indices[i]);

			if (!MainPoint)
			{
				continue;
			}

			// If nothing for this point has changed.
			if ((!NewPositions[i].IsSet() || MainPoint->GetPosition() == NewPositions[i].GetValue())
				&& (!NewValues[i].IsSet() || MainPoint->GetValue() == NewValues[i].GetValue())
				&& (!NewInterpolationTypes[i].IsSet()
					|| MainPoint->GetInterpolation() == NewInterpolationTypes[i].GetValue()))
			{
				continue;
			}

			bWasPointsChanged = true;

			for (const ParameterWeakPtr Parameter : Parameters)
			{
				if (!IsValidWeakPointer(Parameter))
				{
					continue;
				}

				if (Parameter->IsAutoUpdate() && bIsCookingEnabled)
				{
					if (Parameter->Points.IsValidIndex(Indices[i]))
					{
						PointType* const Point = Parameter->Points[Indices[i]];

						if (!Point)
						{
							continue;
						}

						if (NewPositions[i].IsSet())
						{
							if (!Point->PositionParentParm)
							{
								continue;
							}
							Point->SetPosition(NewPositions[i].GetValue());
							Point->PositionParentParm->MarkChanged(true);
						}
						if (NewValues[i].IsSet())
						{
							if (!Point->ValueParentParm)
							{
								continue;
							}
							Point->SetValue(NewValues[i].GetValue());
							Point->ValueParentParm->MarkChanged(true);
						}
						if (NewInterpolationTypes[i].IsSet())
						{
							if (!Point->InterpolationParentParm)
							{
								continue;
							}
							Point->SetInterpolation(NewInterpolationTypes[i].GetValue());
							Point->InterpolationParentParm->MarkChanged(true);
						}
					}
					else
					{
						int32 IdxInEventsArray = Indices[i] - Parameter->Points.Num();
						if (Parameter->ModificationEvents.IsValidIndex(IdxInEventsArray))
						{
							UHoudiniParameterRampModificationEvent* const Event =
								Parameter->ModificationEvents[IdxInEventsArray];

							if (!Event)
							{
								continue;
							}

							if (NewPositions[i].IsSet())
							{
								Event->SetPosition(NewPositions[i].GetValue());
							}
							if (NewValues[i].IsSet())
							{
								Event->SetValue(NewValues[i].GetValue());
							}
							if (NewInterpolationTypes[i].IsSet())
							{
								Event->SetInterpolation(NewInterpolationTypes[i].GetValue());
							}

						}
					}
				}
				else
				{
					if (Parameter->CachedPoints.IsValidIndex(Indices[i]))
					{
						PointType* const CachedPoint = Parameter->CachedPoints[Indices[i]];

						if (!CachedPoint)
						{
							continue;
						}

						if (NewPositions[i].IsSet())
						{
							CachedPoint->Position = NewPositions[i].GetValue();
						}
						if (NewValues[i].IsSet())
						{
							CachedPoint->Value = NewValues[i].GetValue();
						}
						if (NewInterpolationTypes[i].IsSet())
						{
							CachedPoint->Interpolation = NewInterpolationTypes[i].GetValue();
						}

						Parameter->SetCaching(true);
					}
				}
			}
		}
		return bWasPointsChanged;
	}

	void
	ReplaceAllParameterPointsWithMainParameter()
	{
		const ParameterWeakPtr MainParameter = GetMainParameter();

		if (!IsValidWeakPointer(MainParameter))
		{
			return;
		}

		if (FHoudiniEngineUtils::IsHoudiniCookableCooking(MainParameter.Get()))
		{
			return;
		}

		for (int32 Idx = 1; Idx < Parameters.Num(); ++Idx)
		{
			if (!IsValidWeakPointer(Parameters[Idx]))
			{
				continue;
			}

			ReplaceParameterPointsWithMainParameter(MainParameter.Get(), Parameters[Idx].Get());
		}
	}

private:

	static void
	ReplaceParameterPointsWithMainParameter(ParameterType* Param, ParameterType* MainParam)
	{
		if (!Param || !MainParam)
			return;

		if (FHoudiniEngineUtils::IsHoudiniCookableCooking(Param))
			return;

		const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

		// Use Synced points if the MainParam is on auto update mode
		// Use Cached points if the Mainparam is on manual update mode

		TArray<TObjectPtr<PointType>>& MainPoints = (MainParam->IsAutoUpdate() && bCookingEnabled)
			? MainParam->Points
			: MainParam->CachedPoints;

		if (Param->IsAutoUpdate() && bCookingEnabled)
		{
			TArray<TObjectPtr<PointType>>& Points = Param->Points;

			int32 PointIdx = 0;
			while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
			{
				TObjectPtr<PointType>& MainPoint = MainPoints[PointIdx];
				TObjectPtr<PointType>& Point = Points[PointIdx];

				if (!MainPoint || !Point)
					continue;

				if (MainPoint->GetPosition() != Point->GetPosition())
				{
					if (Point->PositionParentParm)
					{
						Point->SetPosition(MainPoint->GetPosition());
						Point->PositionParentParm->MarkChanged(true);
					}
				}

				if (MainPoint->GetValue() != Point->GetValue())
				{
					if (Point->ValueParentParm)
					{
						Point->SetValue(MainPoint->GetValue());
						Point->ValueParentParm->MarkChanged(true);
					}
				}

				if (MainPoint->GetInterpolation() != Point->GetInterpolation())
				{
					if (Point->InterpolationParentParm)
					{
						Point->SetInterpolation(MainPoint->GetInterpolation());
						Point->InterpolationParentParm->MarkChanged(true);
					}
				}

				PointIdx += 1;
			}

			int32 PointInsertIdx = PointIdx;
			int32 PointDeleteIdx = PointIdx;

			// skip the pending modification events
			for (auto& Event : Param->ModificationEvents)
			{
				if (!Event)
					continue;

				if (Event->IsInsertEvent())
					PointInsertIdx += 1;

				if (Event->IsDeleteEvent())
					PointDeleteIdx += 1;
			}

			// There are more points in MainPoints array
			for (; PointInsertIdx < MainPoints.Num(); ++PointInsertIdx)
			{
				TObjectPtr<PointType> & NextMainPoint = MainPoints[PointInsertIdx];

				if (!NextMainPoint)
					continue;

				Param->CreateInsertEvent(
					NextMainPoint->GetPosition(),
					NextMainPoint->GetValue(),
					NextMainPoint->GetInterpolation());

				Param->MarkChanged(true);
			}

			// There are more points in Points array
			for (; PointDeleteIdx < Points.Num(); ++PointDeleteIdx)
			{
				TObjectPtr<PointType>& NextPoint = Points[PointDeleteIdx];

				if (!NextPoint)
					continue;

				Param->CreateDeleteEvent(NextPoint->InstanceIndex);

				Param->MarkChanged(true);
			}

		}
		else
		{
			TArray<TObjectPtr<PointType>>& Points = Param->CachedPoints;

			int32 PointIdx = 0;
			while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
			{
				TObjectPtr<PointType>& MainPoint = MainPoints[PointIdx];
				TObjectPtr<PointType>& Point = Points[PointIdx];

				if (!MainPoint || !Point)
					continue;

				if (Point->Position != MainPoint->Position)
				{
					Point->Position = MainPoint->Position;
					Param->bCaching = true;
					if (!bCookingEnabled)
					{
						if (Point->InterpolationParentParm)
							Point->PositionParentParm->MarkChanged(true);
						Param->MarkChanged(true);
					}
				}

				if (Point->Value != MainPoint->Value)
				{
					Point->Value = MainPoint->Value;
					Param->bCaching = true;
					if (!bCookingEnabled)
					{
						if (Point->ValueParentParm)
							Point->ValueParentParm->MarkChanged(true);
						Param->MarkChanged(true);
					}
				}

				if (Point->Interpolation != MainPoint->Interpolation)
				{
					Point->Interpolation = MainPoint->Interpolation;
					Param->bCaching = true;
					if (!bCookingEnabled)
					{
						if (Point->InterpolationParentParm)
							Point->InterpolationParentParm->MarkChanged(true);
						Param->MarkChanged(true);
					}
				}

				PointIdx += 1;
			}

			// There are more points in MainPoints array
			for (int32 MainPointsLeftIdx = PointIdx; MainPointsLeftIdx < MainPoints.Num(); ++MainPointsLeftIdx)
			{
				TObjectPtr<PointType> NextMainPoint = MainPoints[MainPointsLeftIdx];

				if (!NextMainPoint)
					continue;

				PointType* NewCachedPoint = NewObject<PointType>(Param, PointType::StaticClass());

				if (!NewCachedPoint)
					continue;

				NewCachedPoint->Position = NextMainPoint->GetPosition();
				NewCachedPoint->Value = NextMainPoint->GetValue();
				NewCachedPoint->Interpolation = NextMainPoint->GetInterpolation();

				Points.Add(NewCachedPoint);

				Param->SetCaching(true);
			}

			// there are more points in Points array
			for (int32 PointsLeftIdx = PointIdx; PointIdx < Points.Num(); ++PointIdx)
			{
				Points.Pop();
				Param->SetCaching(true);
			}
		}
	}
};
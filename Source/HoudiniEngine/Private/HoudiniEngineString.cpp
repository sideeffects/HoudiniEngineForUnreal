/*
* Copyright (c) <2021> Side Effects Software Inc.
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

#include "HoudiniEngineString.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include <vector>

FHoudiniEngineString::FHoudiniEngineString()
	: StringId(-1)
{}

FHoudiniEngineString::FHoudiniEngineString(int32 InStringId)
	: StringId(InStringId)
{}

FHoudiniEngineString::FHoudiniEngineString(const FHoudiniEngineString& Other)
	: StringId(Other.StringId)
{}

FHoudiniEngineString &
FHoudiniEngineString::operator=(const FHoudiniEngineString& Other)
{
	if (this != &Other)
		StringId = Other.StringId;

	return *this;
}

bool
FHoudiniEngineString::operator==(const FHoudiniEngineString& Other) const
{
	return Other.StringId == StringId;
}

bool
FHoudiniEngineString::operator!=(const FHoudiniEngineString& Other) const
{
	return Other.StringId != StringId;
}

int32
FHoudiniEngineString::GetId() const
{
	return StringId;
}

bool
FHoudiniEngineString::HasValidId() const
{
	return StringId > 0;
}

bool
FHoudiniEngineString::ToStdString(std::string& String) const
{
	String = "";

	// Null string ID / zero should be considered invalid
	// (or we'd get the "null string, should never see this!" text)
	if (StringId <= 0)
	{
		return false;
	}		

	int32 NameLength = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetStringBufLength(
		FHoudiniEngine::Get().GetSession(), StringId, &NameLength))
	{
		return false;
	}
		
	if (NameLength <= 0)
		return false;
		
	std::vector<char> NameBuffer(NameLength, '\0');
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetString(
		FHoudiniEngine::Get().GetSession(),
		StringId, &NameBuffer[0], NameLength ) )
	{
		return false;
	}

	String = std::string(NameBuffer.begin(), NameBuffer.end());

	return true;
}

bool
FHoudiniEngineString::ToFName(FName& Name) const
{
	Name = NAME_None;
	FString NameString = TEXT("");
	if (ToFString(NameString))
	{
		Name = FName(*NameString);
		return true;
	}

	return false;
}

bool
FHoudiniEngineString::ToFString(FString& String) const
{
	String = TEXT("");
	std::string NamePlain = "";

	if (ToStdString(NamePlain))
	{
		String = UTF8_TO_TCHAR(NamePlain.c_str());
		return true;
	}

	return false;
}

bool
FHoudiniEngineString::ToFText(FText& Text) const
{
	Text = FText::GetEmpty();
	FString NameString = TEXT("");

	if (ToFString(NameString))
	{
		Text = FText::FromString(NameString);
		return true;
	}

	return false;
}

bool 
FHoudiniEngineString::ToStdString(const int32& InStringId, std::string& OutStdString)
{
	FHoudiniEngineString HAPIString(InStringId);
	return HAPIString.ToStdString(OutStdString);
}

bool
FHoudiniEngineString::ToFName(const int32& InStringId, FName& OutName)
{
	FHoudiniEngineString HAPIString(InStringId);
	return HAPIString.ToFName(OutName);
}

bool
FHoudiniEngineString::ToFString(const int32& InStringId, FString& OutString)
{
	FHoudiniEngineString HAPIString(InStringId);
	return HAPIString.ToFString(OutString);
}

bool
FHoudiniEngineString::ToFText(const int32& InStringId, FText& OutText)
{
	FHoudiniEngineString HAPIString(InStringId);
	return HAPIString.ToFText(OutText);
}

bool
FHoudiniEngineString::SHArrayToFStringArray(const TArray<int32>& InStringIdArray, TArray<FString>& OutStringArray)
{
	if (SHArrayToFStringArray_Batch(InStringIdArray, OutStringArray))
		return true;

	return SHArrayToFStringArray_Singles(InStringIdArray, OutStringArray);
}

bool
FHoudiniEngineString::SHArrayToFStringArray_Batch(const TArray<int32>& InStringIdArray, TArray<FString>& OutStringArray)
{
	bool bReturn = true;
	OutStringArray.SetNumZeroed(InStringIdArray.Num());

	TArray<int32> UniqueSH;
	for (const auto& CurrentSH : InStringIdArray)
	{
		UniqueSH.AddUnique(CurrentSH);
	}

	int32 BufferSize = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetStringBatchSize(
		FHoudiniEngine::Get().GetSession(), UniqueSH.GetData(), UniqueSH.Num(), &BufferSize))
		return false;

	if (BufferSize <= 0)
		return false;

	std::vector<char> Buffer(BufferSize, '\0');
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetStringBatch(
		FHoudiniEngine::Get().GetSession(), &Buffer[0], BufferSize))
		return false;

	// Parse the buffer to a string array
	TArray<FString> ConvertedString;
	std::vector<char>::iterator CurrentBegin = Buffer.begin();	
	for (std::vector<char>::iterator it = Buffer.begin(); it != Buffer.end(); it++)
	{
		if (*it != '\0')
			continue;

		std::string stdString = std::string(CurrentBegin, it);
		ConvertedString.Add(UTF8_TO_TCHAR(stdString.c_str()));

		CurrentBegin = it;
		CurrentBegin++;
	}

	if (ConvertedString.Num() != UniqueSH.Num())
		return false;

	// Build a map to map string handles to indices
	TMap<HAPI_StringHandle, int32> SHToIndexMap;
	for (int32 Idx = 0; Idx < UniqueSH.Num(); Idx++)
		SHToIndexMap.Add(UniqueSH[Idx], Idx);

	// Fill the output array using the map
	for (int32 IdxSH = 0; IdxSH < InStringIdArray.Num(); IdxSH++)
	{
		const int32* FoundIndex = SHToIndexMap.Find(InStringIdArray[IdxSH]);
		if (!FoundIndex || !ConvertedString.IsValidIndex(*FoundIndex))
			return false;

		// Already resolved earlier, copy the string instead of calling HAPI.
		OutStringArray[IdxSH] = ConvertedString[*FoundIndex];
	}

	return true;
}
bool
FHoudiniEngineString::SHArrayToFStringArray_Singles(const TArray<int32>& InStringIdArray, TArray<FString>& OutStringArray)
{
	bool bReturn = true;
	OutStringArray.SetNumZeroed(InStringIdArray.Num());

	// Avoid calling HAPI to resolve the same strings again and again
	TMap<HAPI_StringHandle, int32> ResolvedStrings;
	for (int32 IdxSH = 0; IdxSH < InStringIdArray.Num(); IdxSH++)
	{
		const int32* ResolvedString = ResolvedStrings.Find(InStringIdArray[IdxSH]);
		if (ResolvedString)
		{
			// Already resolved earlier, copy the string instead of calling HAPI.
			OutStringArray[IdxSH] = OutStringArray[*ResolvedString];
		}
		else
		{
			FString CurrentString = FString();
			if(!FHoudiniEngineString::ToFString(InStringIdArray[IdxSH], CurrentString))
				bReturn = false;

			OutStringArray[IdxSH] = CurrentString;
			ResolvedStrings.Add(InStringIdArray[IdxSH], IdxSH);
		}
	}

	return bReturn;
}
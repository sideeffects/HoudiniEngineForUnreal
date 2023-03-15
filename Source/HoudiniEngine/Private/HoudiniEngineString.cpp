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

    TSet<int32> UniqueSH;
    for (const auto& CurrentSH : InStringIdArray)
    {
        UniqueSH.Add(CurrentSH);
    }

    TArray<int32> UniqueSHArray = UniqueSH.Array();

    int32 BufferSize = 0;
    if (HAPI_RESULT_SUCCESS
        != FHoudiniApi::GetStringBatchSize(FHoudiniEngine::Get().GetSession(), UniqueSHArray.GetData(),
                                           UniqueSHArray.Num(), &BufferSize))
        return false;

    if (BufferSize <= 0)
        return false;

    TArray<char> Buffer;
    Buffer.SetNumZeroed(BufferSize);
    if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetStringBatch(FHoudiniEngine::Get().GetSession(), &Buffer[0], BufferSize))
        return false;

    // Parse the buffer to a string array
    TMap<int, FString> StringMap;
    int Index = 0;
    int StringOffset = 0;
    while (StringOffset < BufferSize)
    {
        // Add the current string to our dictionary.
        FString StringValue = UTF8_TO_TCHAR(& Buffer[StringOffset]);
        StringMap.Add(UniqueSHArray[Index], StringValue);

        // Move on to next indexed string
        Index++;
        while (Buffer[StringOffset] != 0 && StringOffset < BufferSize)
            StringOffset++;

        StringOffset++;
    }

    if (StringMap.Num() != UniqueSH.Num())
        return false;

    // Fill the output array using the map
    for (int32 IdxSH = 0; IdxSH < InStringIdArray.Num(); IdxSH++)
    {
        OutStringArray[IdxSH] = StringMap[InStringIdArray[IdxSH]];
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

const FString& FHoudiniEngineIndexedStringMap::GetStringForIndex(int Index) const
{
    StringId Id = Ids[Index];
    return Strings[Id];
}

void FHoudiniEngineIndexedStringMap::SetString(int Index, const FString& Value)
{
    StringId Id;
	const auto * Found = StringToId.Find(Value);
    if (Found != nullptr)
    {
        Id = *Found;
    }
    else
    {
        Id = Strings.Num();
        Strings.Add(Value);
        StringToId.Add(Value, Id);
    }

    if (Index >= Ids.Num())
        Ids.SetNum(Index + 1);

    Ids[Index] = Id;
}


FHoudiniEngineRawStrings FHoudiniEngineIndexedStringMap::GetRawStrings() const
{

    FHoudiniEngineRawStrings Results;
    Results.CreateRawStrings(Strings);
    return Results;
}

void FHoudiniEngineIndexedStringMap::Reset(int ExpectedStringCount, int ExpectedIndexCount)
{
    FHoudiniEngineIndexedStringMap Map;
    *this = Map;
    Ids.Reserve(ExpectedIndexCount);
	Strings.Reserve(ExpectedStringCount);
}

void FHoudiniEngineRawStrings::CreateRawStrings(const TArray<FString>& Strings)
{
    RawStrings.SetNumZeroed(Strings.Num());
    Buffer.SetNum(0);

    // Calculate buffer size up front.
    int BufferSize = 0;
    for (int Id = 0; Id < Strings.Num(); Id++)
    {
        const FString& Str = Strings[Id];

        const char* TempString = TCHAR_TO_UTF8(*Str);
        int TempStringLen = strlen(TempString);
        BufferSize += TempStringLen + 1;
    }

    Buffer.SetNum(BufferSize);
    int StringStart = 0;
    for (int Id = 0; Id < Strings.Num(); Id++)
    {
        const FString& Str = Strings[Id];

        const char* TempString = TCHAR_TO_UTF8(*Str);

        RawStrings[Id] = &Buffer[StringStart];
        int TempStringLen = strlen(TempString);
        FMemory::Memcpy(&Buffer[StringStart], TempString, TempStringLen + 1);
        StringStart += TempStringLen + 1;
    }
}

bool FHoudiniEngineIndexedStringMap::HasEntries()
{
	// If there are no entries, there are... no entries!
    if (Strings.Num() == 0)
	    return false;

	// If there is one entry and it is an empty string.. no entries.
	if (Strings.Num() == 1 && Strings[0].IsEmpty())
	    return false;

	return true;
}


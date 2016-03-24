/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"

#include <vector>


FHoudiniEngineString::FHoudiniEngineString() :
	StringId(-1)
{

}


FHoudiniEngineString::FHoudiniEngineString(int32 InStringId) :
	StringId(InStringId)
{

}


FHoudiniEngineString::FHoudiniEngineString(const FHoudiniEngineString& Other) :
	StringId(Other.StringId)
{

}


FHoudiniEngineString&
FHoudiniEngineString::operator=(const FHoudiniEngineString& Other)
{
	if(this != &Other)
	{
		StringId = Other.StringId;
	}

	return *this;
}



bool
FHoudiniEngineString::operator==(const FHoudiniEngineString& Other) const
{
	return StringId == Other.StringId;
}



bool
FHoudiniEngineString::operator!=(const FHoudiniEngineString& Other) const
{
	return StringId != Other.StringId;
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

	if(StringId >= 0)
	{
		int32 NameLength = 0;
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetStringBufLength(FHoudiniEngine::Get().GetSession(), StringId,
			&NameLength))
		{
			if(NameLength)
			{
				std::vector<char> NameBuffer(NameLength, '\0');
				if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetString(FHoudiniEngine::Get().GetSession(), StringId,
					&NameBuffer[0], NameLength))
				{
					String = std::string(NameBuffer.begin(), NameBuffer.end());
					return true;
				}
			}
		}
	}

	return false;
}


bool
FHoudiniEngineString::ToFName(FName& Name) const
{
	Name = NAME_None;
	FString NameString = TEXT("");
	if(ToFString(NameString))
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

	if(ToStdString(NamePlain))
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

	if(ToFString(NameString))
	{
		Text = FText::FromString(NameString);
		return true;
	}

	return false;
}


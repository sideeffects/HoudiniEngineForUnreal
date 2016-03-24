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

#pragma once


class FText;
class FString;
class FName;


#include <string>


class HOUDINIENGINERUNTIME_API FHoudiniEngineString
{
	public:

		FHoudiniEngineString();
		FHoudiniEngineString(int32 InStringId);
		FHoudiniEngineString(const FHoudiniEngineString& Other);

	public:

		FHoudiniEngineString& operator=(const FHoudiniEngineString& Other);

	public:

		bool operator==(const FHoudiniEngineString& Other) const;
		bool operator!=(const FHoudiniEngineString& Other) const;

	public:

		bool ToStdString(std::string& String) const;
		bool ToFName(FName& Name) const;
		bool ToFString(FString& String) const;
		bool ToFText(FText& Text) const;

	public:

		/** Return id of this string. **/
		int32 GetId() const;

		/** Return true if this string has a valid id. **/
		bool HasValidId() const;

	protected:

		/** Id of the underlying Houdini Engine string. **/
		int32 StringId;
};

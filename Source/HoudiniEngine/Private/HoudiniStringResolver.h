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

#pragma once

#include "CoreMinimal.h"

#include "HoudiniStringResolver.generated.h"

USTRUCT()
struct HOUDINIENGINE_API FHoudiniStringResolver
{

	GENERATED_USTRUCT_BODY();

protected:

	// Named arguments that will be substituted into attribute values upon retrieval.
	TMap<FString, FStringFormatArg> CachedTokens;


public:
	// ----------------------------------
	// Named argument accessors
	// ----------------------------------

	TMap<FString, FStringFormatArg>& GetCachedTokens() { return CachedTokens; }


	// Set a named argument that will be used for argument replacement during GetAttribute calls.
	void SetToken(const FString& InName, const FString& InValue);

	// Sanitize a token value. Currently only replaces { and } with __.
	static FString SanitizeTokenValue(const FString& InValue);

	void GetTokensAsStringMap(TMap<FString, FString>& OutTokens) const;

	void SetTokensFromStringMap(const TMap<FString, FString>& InValue, bool bClearTokens=true);

	// Resolve a string by substituting `Tokens` as named arguments during string formatting.
	FString ResolveString(const FString& InStr) const;

};


USTRUCT()
struct HOUDINIENGINE_API FHoudiniAttributeResolver : public FHoudiniStringResolver
{
	GENERATED_USTRUCT_BODY();

protected:
	TMap<FString, FString> CachedAttributes;

public:

	void SetCachedAttributes(const TMap<FString,FString>& Attributes);

	// Return a mutable reference to the cached attributes.
	TMap<FString, FString>& GetCachedAttributes() { return CachedAttributes; }

	// Return immutable reference to cached attributes.
	const TMap<FString, FString>& GetCachedAttributes() const { return CachedAttributes; }

	// Set an attribute with the given name and value in the attribute cache.
	void SetAttribute(const FString& InName, const FString& InValue);

	// Try to resolve an attribute with the given name. If the attribute could not be 
	// found, use DefaultValue as a fallback.
	FString ResolveAttribute(
		const FString& InAttrName,
		const FString& InDefaultValue,
		const bool bInUseDefaultIfAttrValueEmpty=false) const;

	// ----------------------------------
	// Helpers
	// ----------------------------------

	// Helper for resolving the `unreal_level_path` attribute.
	FString ResolveFullLevelPath() const;

	// Helper for resolver custom output name attributes.
	FString ResolveOutputName(bool bInForBake=false) const;

	// Helper for resolving the unreal_bake_folder attribute. Converts to an absolute path.
	FString ResolveBakeFolder() const;

	// Helper for resolving the unreal_temp_folder attribute.
	FString ResolveTempFolder() const;

	// ----------------------------------
	// Debug
	// ----------------------------------
	// Logs the resolver's cached attributes and tokens
	void LogCachedAttributesAndTokens() const;
};
/*
* Copyright (c) <2023> Side Effects Software Inc.
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
#include "UObject/Object.h"
#include "HoudiniToolTypes.h"

#include "HoudiniToolData.generated.h"

struct FImage;
enum class EAssetCommandResult : uint8;
struct FAssetCategoryPath;
struct FAssetOpenArgs;
class UHoudiniAsset;

// Mirrors the data from FImage
// We make our own struct so that we have control over any struct changes 
USTRUCT()
struct HOUDINIENGINERUNTIME_API FHImageData
{
	GENERATED_BODY()

	FHImageData();
	
	/** Width of the image. */
	UPROPERTY()
	int32 SizeX;
	
	/** Height of the image. */
	UPROPERTY()
	int32 SizeY;
	
	/** Number of image slices. */
	UPROPERTY()
	int32 NumSlices;
	
	/** Format in which the image is stored. (ERawImageFormat::Type)*/
	UPROPERTY()
	uint8 Format;
	
	/** The gamma space the image is stored in. (EGammaSpace)*/
	UPROPERTY()
	uint8 GammaSpace;

	UPROPERTY()
	TArray<uint8> RawData;

	/** MD5 hash of the RawData **/
	UPROPERTY()
	FString RawDataMD5;

	// Assignment overloads
	void FromImage(const FImage& InImage);
	void ToImage(FImage& OutImage) const;

};

// Houdini tool data that is cached on the Houdini Asset to allow the
// Houdini Tools to function independently of external tool packages.
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniToolData : public UObject
{
	GENERATED_BODY()

	/** Only valid in the editor */
	virtual bool IsEditorOnly() const override { return true; }

public:

	UHoudiniToolData();

	// Populate this object from the given JSON data.
	UFUNCTION(BlueprintCallable, Category="HoudiniToolData")
	bool PopulateFromJSONData(const FString& JSONData);

	// Read the content of a JSON file, then populate this object.
	UFUNCTION(BlueprintCallable, Category="HoudiniToolData")
	bool PopulateFromJSONFile(const FString& JsonFilePath);

	// Convert the data from this HoudiniTool into JSON.
	UFUNCTION(BlueprintCallable, Category="HoudiniToolData")
	bool ConvertToJSONData(FString& JSONData) const;

	// After converting the data from this object to JSON, save it to a file.
	UFUNCTION(BlueprintCallable, Category="HoudiniToolData")
	bool SaveToJSONFile(const FString& JsonFilePath);

#if WITH_EDITOR
	// Load an icon from the given path (on the filesystem)
	// and cache it in this UHoudiniToolData. This will also
	// cache it as a thumbnail for the owning package of this asset.
	void LoadIconFromPath(const FString& IconPath);

	// Clear the icon cached on this UHoudiniToolData instance and
	// clear the cached thumbnail for the owning package.
	void ClearCachedIcon();

	// Update the owning asset's thumbnail 
	void UpdateOwningAssetThumbnail();
#endif

	void CopyFrom(const UHoudiniToolData& Other);

	/** The name (label) to be displayed. Not to be confused with the operator type name or UAsset name */
	UPROPERTY()
	FString Name;

	/** The tooltip for this HDA. */
	UPROPERTY()
	FString ToolTip;

	/** Raw image data of the icon to be displayed */
	UPROPERTY()
	FHImageData IconImageData;

	/** Path from which the current icon was imported */
	UPROPERTY()
	FFilePath IconSourcePath;

	/** The help URL for this tool */
	UPROPERTY()
	FString HelpURL;

	/** The type of tool, this will change how the asset handles the current selection **/
	UPROPERTY()
	EHoudiniToolType Type;

	/** DEPRECATED: Indicate this is one of the default tools **/
	// TODO: Replace DefaultTool usages with the ReadOnlyTools property from the owning tools package instead.
	UPROPERTY()
	bool DefaultTool;

	/** Indicate what the tool should consider for selection **/
	UPROPERTY()
	EHoudiniToolSelectionType SelectionType;

	/** Path to the Asset used **/
	UPROPERTY()
	FFilePath SourceAssetPath;

	// /** Directory containing the tool **/
	// FHoudiniToolDirectory ToolDirectory;

	// NOTE: This should be inferred from the SourceAssetPath.
	/** Name of the JSON containing the tool's description **/
	// UPROPERTY()
	// FString JSONFile;

	/** Returns the file path to the JSON file containing the tool's description **/
	// FString GetJSonFilePath() { return ToolDirectory.Path.Path / JSONFile; };
	// TODO: Resolve the JSON file path based using the path relative to the Houdini Package.
	FString GetJSonFilePath() { return FString(); };

};

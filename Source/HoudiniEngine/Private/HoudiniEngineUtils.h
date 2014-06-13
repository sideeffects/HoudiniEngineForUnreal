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

struct FHoudiniEngineUtils
{
public:

	/** Return a string description of error from a given error code. **/
	static const FString GetErrorDescription(HAPI_Result Result);

	/** Return a string error description. **/
	static const FString GetErrorDescription();

	/** Return true if module has been properly initialized. **/
	static bool IsInitialized();
	
	/** Return name for a given asset. */
	static bool GetAssetName(int AssetName, std::string& AssetNameString);
	static bool GetAssetName(int AssetName, FString& AssetNameString);

	/** Extract geometry information for a given asset. **/
	static bool GetAssetGeometry(HAPI_AssetId AssetId, TArray<FHoudiniMeshTriangle>& Geometry, FBoxSphereBounds& SphereBounds);

	/** Extract Houdini logo temporary geometry. **/
	static void GetHoudiniLogoGeometry(TArray<FHoudiniMeshTriangle>& Geometry, FBoxSphereBounds& SphereBounds);
	
protected:
	
	/** Given current min and max extent vectors, update them from given position if necessary. **/
	static void UpdateBoundingVolumeExtent(const FVector& Vector, FVector& ExtentMin, FVector& ExtentMax);
};

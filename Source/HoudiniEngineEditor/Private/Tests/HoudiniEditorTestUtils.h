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

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

#include "HoudiniEditorTestUtils.generated.h"

class UHoudiniEditorTestObject;
class UHoudiniAsset;
class UHoudiniAssetComponent;
class UHoudiniPublicAPI;
class UHoudiniPublicAPIAssetWrapper;
class ULevel;
class SWindow;

#if WITH_DEV_AUTOMATION_TESTS

/*
#define DO_TEST_SETUP 0
#if DO_TEST_SETUP
	#define RUN_DIFF_TEST FHoudiniEditorTestUtils::SetupDifferentialTest
#else
	#define RUN_DIFF_TEST FHoudiniEditorTestUtils::RunDifferentialTest
#endif
*/


// Struct to override FAutomationTestBase behaviour as we see fit
struct FHoudiniAutomationTest : FAutomationTestBase
{
	FHoudiniAutomationTest( const FString& InName, const bool bInComplexTask )
        : FAutomationTestBase(InName, bInComplexTask)
	{
	}

	void AddWarning(const FString& InWarning, int32 StackOffset = 0) override;

	// Similar to FAutomationTestBase::AddExpectedError, except only for warnings, and there is no
	void AddSupressedWarning(const FString& InWarningPattern);

	// Overrides the default "AddExpectedError Behaviour to not require an exact number of warnings for the test to succeed
	// If false, use default behaviour
	bool UseSupressedWarnings = true;

	TSet<FString> SupressedWarnings;

	bool IsSupressedWarning(const FString& InWarning);
	
};

// Similar to IMPLEMENT_SIMPLE_AUTOMATION_TEST, But use Houdini class
#define IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, FHoudiniAutomationTest, PrettyName, TFlags, __FILE__, __LINE__) \
namespace\
{\
TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
}


/*
 *TBD;
 * FHoudiniMeshSocket:
 * AActor: test number and types of attached component, label, transform etc..
 * ALandscapeProxy: test size, num comoponents/components size, bounds and extents, num of paint layers, num of edit layers
 * UMaterialInterface: if unreal mat, should be the same, if Houdini generated mat, look for param/textures
 * UStaticMesh: Check bounds? num lods, num sockets, num materials...
 */
class FHoudiniEditorTestUtils
{
public:
	enum EEditorScreenshotType
	{
		ENTIRE_EDITOR,
		ACTIVE_WINDOW, // Gets the active window. Probably never use this.
		DETAILS_WINDOW,
		VIEWPORT
	};

	/** Function signature for HDA Pre-instantiation callbacks on test functions. */
	typedef TFunction<void(UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)> FPreInstantiationCallback;

	/** Function signature for HDA Post-instantiation callbacks, specifically for the differential tests */
	// bSaveAssets is true if done in a temp folder, or if it is the first time creating it in the saved folder, and false otherwise.
	// DefaultCookFolder represents the folder that you are cooking in. Generally, you do not want to modify TestSaved, but you do for TestTemp.
	typedef TFunction<void(UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueTest, const bool bSaveAssets, const FString DefaultCookFolder)> FPostDiffTestHDAInstantiationCallback;

	typedef TFunction<void(UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool bIsSuccessful)> FPostTestHDAInstantiationCallback;

	static void InitializeTests(FHoudiniAutomationTest* Test, const TFunction<void()>& OnFinish);

	static UObject* FindAssetUObject(FHoudiniAutomationTest* Test, const FName AssetUObjectPath);

	/**
	 * Instantiate an HDA from the uasset with path AssetUObjectPath. Calls on OnFinishInstantiate after the first
	 * cook is complete.
	 * @param Test The test calling this function.
	 * @param AssetUObjectPath The package path of the HDA uasset to instantiate.
	 * @param OnFinishInstantiate Function to call either on failure to instantiate/cook or after processing outputs.
	 * @param ErrorOnFail If true, add an error to the test if cooking fails. Defaults to true.
	 * @param DefaultCookFolder Sets the TemporaryCookFolder on the instantiated HDA's UHoudiniAssetComponent. Defaults to "".
	 * @param OnPreInstantiation Function to call when the asset enters the pre-instantiation phase (via the public API). Defaults to nullptr.
	 * @return The UHoudiniAssetComponent of the asset.
	 */
	static void InstantiateAsset(
		FHoudiniAutomationTest* Test,
		const FName AssetUObjectPath,
		const FPostTestHDAInstantiationCallback & OnFinishInstantiate,
		const bool ErrorOnFail = true, const FString DefaultCookFolder = "",
		const FPreInstantiationCallback& OnPreInstantiation=nullptr);

	static void TakeScreenshotEditor(FHoudiniAutomationTest* Test, const FString ScreenshotName, const EEditorScreenshotType EditorScreenshotType, const FVector2D Size);

	static void TakeScreenshotViewport(FHoudiniAutomationTest* Test, const FString ScreenshotName);

	static void SetUseLessCPUInTheBackground();

	static TSharedPtr<SWindow> GetMainFrameWindow();

	static TSharedPtr<SWindow> GetActiveTopLevelWindow();

	static TSharedPtr<SWindow> CreateNewDetailsWindow();

	static TSharedPtr<SWindow> CreateViewportWindow();

	static const FVector2D GDefaultEditorSize;

	static UHoudiniAssetComponent* GetAssetComponentWithName(const FString Name);

	/**
	 * Checks that InAssetWrapper and InTestObject are valid.
	 * @param InTest The test.
	 * @param InAssetWrapper The asset wrapper to check for validity.
	 * @param InTestObject The test object to check for validity.
	 * @param bInAddTestErrors If true, use InTest->AddError to add appropriate errors if InAssetWrapper and/or
	 * InTestObject is invalid.
	 * @return false if either InAssetWrapper or InTestObject is valid. 
	 */
	static bool CheckAssetWrapperAndTestObject(
		FHoudiniAutomationTest* InTest, UHoudiniPublicAPIAssetWrapper* const InAssetWrapper,
		UHoudiniEditorTestObject* const InTestObject, const bool bInAddTestErrors=true);

	/**
	 * Checks that InAssetWrapper, InTestObject and the HAC of InAssetWrapper are valid. Sets OutHAC to the HAC
	 * wrapped by InAssetWrapper if its valid.
	 * @param InTest The test.
	 * @param InAssetWrapper The asset wrapper to check for validity.
	 * @param InTestObject The test object to check for validity.
	 * @param OutHAC The HAC from asset wrapper, if it is valid.
	 * @param bInAddTestErrors If true, use InTest->AddError to add appropriate errors if InAssetWrapper, InTestObject
	 * and/or the HAC is invalid.
	 * @return false if InAssetWrapper, InTestObject or the HAC from InAssetWrapper is invalid. 
	 */
	static bool CheckAssetWrapperTestObjectAndGetValidHAC(
		FHoudiniAutomationTest* InTest, UHoudiniPublicAPIAssetWrapper* const InAssetWrapper,
		UHoudiniEditorTestObject* const InTestObject, UHoudiniAssetComponent*& OutHAC, const bool bInAddTestErrors=true);

	/**
	 * Add an error if setting a paramater value via the API failed. Includes the parameter tuple name, type, value, index
	 * and the error message returned by the API.
	 * @param InTest The FHoudiniAutomationTest instance of the test where the error happened.
	 * @param InAssetWrapper The UHoudiniPublicAPIAssetWrapper instance.
	 * @param InParamType The parameter type as a string
	 * @param InParamTupleName The parameter tuple name
	 * @param InValueAsString The parameter value as a string
	 * @param InIndex The parameter index.
	 */
	static void LogSetParameterValueViaAPIFailure(
		FHoudiniAutomationTest* const InTest,
		UHoudiniPublicAPIAssetWrapper const * const InAssetWrapper,
		const FString& InParamType,
		const FName& InParamTupleName,
		const FString& InValueAsString,
		const int32 InIndex);

	/*
	* Runs or setup the differential test
	* based on the value of the HoudiniEngine.SetupRegTests Cvar
	*/
	static void RunOrSetupDifferentialTest(
		FHoudiniAutomationTest* Test,
		const FString MapName,
		const FString HDAAssetPath,
		const FString ActorName,
		const TFunction<void(bool)>& OnFinishedCallback,
		const FPreInstantiationCallback& OnPreInstantiation = nullptr,
		const FPostDiffTestHDAInstantiationCallback & OnPostInstantiation = nullptr);

	/*
	* Runs the differential test: Requires the MapName, HDAAssetPath, ActorName
	* Additionally, you can add a callback, which is called at the end of the test.
	* Do not call this function twice. Chain it with the callback (See: HoudiniEditorRandomEquivalenceTest)
	* How it works:
	* - Does the map with TestSaved/MapName exist?
	*   - If no, create the map, instantiate the asset in the map, save and return.
	*    - If yes,
	*	     - Load the map at TestSaved/Map
	*          - Does the actor named ActorName exist?
	*            - If no, instantiate the asset in the map, save, and return.
	*            - If yes, continue
	*      - Create a map in TestTemp/ and load it (This is so you have a duplicate version if you want to update the Saved version)
	*        - If ActorName exists, delete it
	*        - If ActorName doesn't exist, instantiate that
	*      - Load the map in TestSaved, instantiate the asset, and compare
	* If the comparison is false, then an output message will appear, telling you to DELETE TestSaved/ and MOVE TestTemp/ to the previous location, fix redirectors, and commit to dev_reg
	* Note: The reason why we do this is because we can't use the map "Save As" functionality, because it counts as duplicating
	* Note: If you need to update any test HDAs / Saved scenes or assets, please commit it to: dev_reg/src/tests/hapi/unreal/CopyFiles
	*/
	static void SetupDifferentialTest(
		FHoudiniAutomationTest* Test,
		const FString MapName,
		const FString HDAAssetPath,
		const FString ActorName,
		const TFunction<void(bool)>& OnFinishedCallback,
		const FPreInstantiationCallback& OnPreInstantiation=nullptr,
		const FPostDiffTestHDAInstantiationCallback & OnPostInstantiation = nullptr);

	/*
	* Runs the differential test: Requires the MapName, HDAAssetPath, ActorName
	* Additionally, you can add a callback, which is called at the end of the test.
	* Do not call this function twice. Chain it with the callback (See: HoudiniEditorRandomEquivalenceTest)
	* How it works:
	* - Does the map with TestSaved/MapName exist?
	*   - If no, fail and return
	*    - If yes,
	*	     - Load the map at TestSaved/Map
	*          - Does the actor named ActorName exist?
	*            - If no, fail and return
	*            - If yes, continue
	*      - Load the map in TestSaved, instantiate the asset, and compare
	*          - if the comparison is true, success!
	*		   - if not, then the call a temp version of the map that can potentially be used to update the test
	* If the comparison is false, then an output message will appear, telling you to DELETE TestSaved/ and MOVE TestTemp/ to the previous location, fix redirectors, and commit to dev_reg
	* Note: The reason why we do this is because we can't use the map "Save As" functionality, because it counts as duplicating
	* Note: If you need to update any test HDAs / Saved scenes or assets, please commit it to: dev_reg/src/tests/hapi/unreal/CopyFiles
	*/
	static void RunDifferentialTest(
		FHoudiniAutomationTest* Test,
		const FString MapName,
		const FString HDAAssetPath,
		const FString ActorName,
		const TFunction<void(bool)>& OnFinishedCallback,
		const FPreInstantiationCallback& OnPreInstantiationCallback = nullptr,
		const FPostDiffTestHDAInstantiationCallback& OnPostInstantiationCallback = nullptr);

	/*
	* Creates the map in TestTemp that can be used to update the differential test's results:
	*  -Create a map in TestTemp / and load it(This is so you have a duplicate version if you want to update the Saved version)
	*        -If ActorName exists, delete it
	*        -If ActorName doesn't exist, instantiate that
	* Note: The reason why we do this is because we can't use the map "Save As" functionality, because it counts as duplicating
	* Note: If you need to update any test HDAs / Saved scenes or assets, please commit it to: dev_reg/src/tests/hapi/unreal/CopyFiles
	*/
	static void CreateTestTempLevel(
		FHoudiniAutomationTest* Test,
		const FString MapName,
		const FString HDAAssetPath,
		const FString ActorName,
		const TFunction<void(bool)>& OnFinishedCallback,
		const FPreInstantiationCallback& OnPreInstantiationCallback = nullptr,
		const FPostDiffTestHDAInstantiationCallback& OnPostInstantiationCallback = nullptr);

	static bool CreateAndLoadNewLevelWithAsset(
		FHoudiniAutomationTest* Test,
		const FString MapAssetPath,
		const FString HDAAssetPath, 
		const FString ActorName,
		const FPostTestHDAInstantiationCallback & OnFinishInstantiate,
		FString DefaultCookFolder = TEXT(""),
		const FPreInstantiationCallback& OnPreInstantiation=nullptr,
		const FPostDiffTestHDAInstantiationCallback & OnPostInstantiation = nullptr);

	static void LoadMap(FHoudiniAutomationTest* Test, const FString Path, const TFunction<void()>& OnLoadedMap);

	static void DeleteHACWithNameInLevelAndSave(const FString Name);

	static bool SaveCurrentLevel();

	// Similar to InstantiateAsset, but for an existing object.
	static void RecookAndWait(FHoudiniAutomationTest* Test, UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const FPostTestHDAInstantiationCallback& OnCookFinished, const FString DefaultCookFolder = TEXT(""));

private:
	static void WaitForScreenshotAndCopy(FHoudiniAutomationTest* Test, FString BaseName, const TFunction<void(FHoudiniAutomationTest*, FString)>& OnScreenshotGenerated);

	static void CopyScreenshotToTestFolder(FHoudiniAutomationTest* Test, FString BaseName);

	static FString GetTestDirectory();

	static FString GetUnrealTestDirectory();

	static FString FormatScreenshotOutputName(FString BaseName);

	static void ForceRefreshViewport();

	static void SetDefaultCookFolder(const FString Folder);

	static FString GetSavedLevelPath(const FString MapName);
	static FString GetSavedAssetsPath(const FString MapName, const FString ActorName);

	static FString GetTempLevelPath(const FString MapName);
	static FString GetTempAssetsPath(const FString MapName, const FString ActorName);

	static const FString SavedPathFolder;
	static const FString TempPathFolder;
	static const float TimeoutTime;

	static UHoudiniEditorTestObject* GetTestObject();

};


#endif

// UObject is needed to add a dynamic delegate to the object. 
// Note: Would it be beneficial to move everything to here?
UCLASS()
class  UHoudiniEditorTestObject : public UObject
{
	GENERATED_BODY()

public:

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHoudiniAssetStateChange, UHoudiniPublicAPIAssetWrapper* /*, InAssetWrapper*/, UHoudiniEditorTestObject* /*, InTestObject*/);

	UHoudiniEditorTestObject();

	/** 
	 * Bound to UHoudiniPublicAPIAssetWrapper::OnPreInstantiationDelegate of an asset wrapper. Broadcasts
	 * ThisClass::OnPreInstantiationDelegate (if bound) when called, and unbinds from InAssetWrapper->OnPreInstantiationDelegate.
	 */
	UFUNCTION()
	void PreInstantiationCallback(UHoudiniPublicAPIAssetWrapper* InAssetWrapper);
	
	UFUNCTION()
	void PostInstantiationCallback(UHoudiniPublicAPIAssetWrapper* AssetWrapper);

	UFUNCTION()
	void PostCookCallback(UHoudiniPublicAPIAssetWrapper* AssetWrapper, const bool bInCookSuccess);
	
	/** 
	 * Bound to UHoudiniPublicAPIAssetWrapper::OnPostProcessingDelegate of an asset wrapper. Broadcasts
	 * ThisClass::OnPostProcessingDelegate (if bound) when called, and unbinds from InAssetWrapper->OnPostProcessingDelegate.
	 */
	UFUNCTION()
	void PostProcessingCallback(UHoudiniPublicAPIAssetWrapper* InAssetWrapper);

	void ResetFields();

	/** Returns true if the asset has cooked the expected number of times. */
	FORCEINLINE
	bool HasReachedExpectedCookCount() const { return this->CookCount >= this->ExpectedCookCount; }

	UPROPERTY()
	TArray<UHoudiniPublicAPIAssetWrapper*> InAssetWrappers;

	FOnHoudiniAssetStateChange OnPreInstantiationDelegate;
	FOnHoudiniAssetStateChange OnPostProcessingDelegate;

	bool IsInstantiating = false;
	int32 CookCount = 0;
	int32 ExpectedCookCount = 1;
	bool InCookSuccess = false;
};

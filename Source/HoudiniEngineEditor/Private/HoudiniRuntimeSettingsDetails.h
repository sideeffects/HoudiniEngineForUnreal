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


class FHoudiniRuntimeSettingsDetails : public IDetailCustomization
{
public:

	/** Constructor. **/
	FHoudiniRuntimeSettingsDetails();

	/** Destructor. **/
	virtual ~FHoudiniRuntimeSettingsDetails();

/** IDetailCustomization methods. **/
public:

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:

	/** Create an instance of this detail layout class. **/
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:

	/** Used to create Houdini version entry. **/
	void CreateHoudiniEntry(const FText& EntryName, IDetailCategoryBuilder& DetailCategoryBuilder,
		int32 VersionMajor, int32 VersionMinor, int32 VersionBuild, int32 VersionPatch);

	/** Used to create Houdini Engine version entry. **/
	void CreateHoudiniEngineEntry(const FText& EntryName, IDetailCategoryBuilder& DetailCategoryBuilder,
		int32 VersionMajor, int32 VersionMinor, int32 VersionApi);

	/** Used to create libHAPI dynamic library path entry. **/
	void CreateHAPILibraryPathEntry(const FString& LibHAPIPath, IDetailCategoryBuilder& DetailCategoryBuilder);

	/** Used to create libHAPI license information entry. **/
	void CreateHAPILicenseEntry(const FString& LibHAPILicense, IDetailCategoryBuilder& DetailCategoryBuilder);
};

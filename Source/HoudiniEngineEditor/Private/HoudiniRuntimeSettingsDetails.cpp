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


#include "HoudiniRuntimeSettingsDetails.h"

#include "HoudiniApi.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "HAPI/HAPI_Version.h"

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "Internationalization/Internationalization.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

TSharedRef< IDetailCustomization >
FHoudiniRuntimeSettingsDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniRuntimeSettingsDetails);
}

FHoudiniRuntimeSettingsDetails::FHoudiniRuntimeSettingsDetails()
{}

FHoudiniRuntimeSettingsDetails::~FHoudiniRuntimeSettingsDetails()
{}

void
FHoudiniRuntimeSettingsDetails::CustomizeDetails(IDetailLayoutBuilder & DetailBuilder)
{
	// Create basic categories.
	DetailBuilder.EditCategory("Session", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Instantiating", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Cooking", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Parameters", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("CollisionGeneration", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("GeometryMarshalling", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("GeometryScalingAndImport", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Static Mesh", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("GeneratedStaticMeshSettings", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("StaticMeshBuildSettings", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("PDG Settings", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("HoudiniLocation", FText::GetEmpty(), ECategoryPriority::Important);

	// Create Plugin Information category.
	{
		static const FName InformationCategory = TEXT("Plugin Information");
		IDetailCategoryBuilder & InformationCategoryBuilder = DetailBuilder.EditCategory(InformationCategory);

		// Add built Houdini version.
		CreateHoudiniEntry(
			LOCTEXT("HInformationBuilt", "Built against Houdini"),
			InformationCategoryBuilder, HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR,
			HAPI_VERSION_HOUDINI_BUILD, HAPI_VERSION_HOUDINI_PATCH);

		// Add built against Houdini Engine version.
		CreateHoudiniEngineEntry(
			LOCTEXT("HEngineInformationBuilt", "Built against Houdini Engine"),
			InformationCategoryBuilder, HAPI_VERSION_HOUDINI_ENGINE_MAJOR,
			HAPI_VERSION_HOUDINI_ENGINE_MINOR, HAPI_VERSION_HOUDINI_ENGINE_API);

		// Add running against Houdini version.
		{
			int32 RunningMajor = 0;
			int32 RunningMinor = 0;
			int32 RunningBuild = 0;
			int32 RunningPatch = 0;

			if (FHoudiniApi::IsHAPIInitialized())
			{
				const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();
				// Retrieve version numbers for running Houdini.
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_MAJOR, &RunningMajor);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_MINOR, &RunningMinor);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_BUILD, &RunningBuild);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_PATCH, &RunningPatch);
			}

			CreateHoudiniEntry(
				LOCTEXT("HInformationRunning", "Running against Houdini"),
				InformationCategoryBuilder, RunningMajor, RunningMinor, RunningBuild, RunningPatch);
		}

		// Add running against Houdini Engine version.
		{
			int32 RunningEngineMajor = 0;
			int32 RunningEngineMinor = 0;
			int32 RunningEngineApi = 0;

			if (FHoudiniApi::IsHAPIInitialized())
			{
				const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();
				// Retrieve version numbers for running Houdini Engine.
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi);
			}

			CreateHoudiniEngineEntry(
				LOCTEXT("HEngineInformationRunning", "Running against Houdini Engine"),
				InformationCategoryBuilder, RunningEngineMajor, RunningEngineMinor, RunningEngineApi);
		}

		// Add path of libHAPI.
		{
			FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
			if (LibHAPILocation.IsEmpty())
				LibHAPILocation = TEXT("Not Found");

			CreateHAPILibraryPathEntry(LibHAPILocation, InformationCategoryBuilder);
		}

		// Add licensing info.
		{
			FString HAPILicenseType = TEXT("");
			if (!FHoudiniEngineUtils::GetLicenseType(HAPILicenseType))
				HAPILicenseType = TEXT("Unknown");

			CreateHAPILicenseEntry(HAPILicenseType, InformationCategoryBuilder);
		}
	}	
}

void
FHoudiniRuntimeSettingsDetails::CreateHoudiniEntry(
	const FText & EntryName,
	IDetailCategoryBuilder & DetailCategoryBuilder,
	int32 VersionMajor, int32 VersionMinor, int32 VersionBuild,
	int32 VersionPatch)
{
	FDetailWidgetRow & Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	Row.NameWidget.Widget =
		SNew(STextBlock)
		.Text(EntryName)
		.Font(IDetailLayoutBuilder::GetDetailFont());

	TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);

	{
		TSharedRef< SNumericEntryBox< int32 > > NumericEntryBox = SNew(SNumericEntryBox< int32 >);
		HorizontalBox->AddSlot().Padding(0, 0, 5, 0)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(false)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Value(VersionMajor)
			];
		NumericEntryBox->SetEnabled(false);
	}

	{
		TSharedRef< SNumericEntryBox< int32 > > NumericEntryBox = SNew(SNumericEntryBox< int32 >);
		HorizontalBox->AddSlot().Padding(0, 0, 5, 0)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(false)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Value(VersionMinor)
			];
		NumericEntryBox->SetEnabled(false);
	}

	{
		TSharedRef< SNumericEntryBox< int32 > > NumericEntryBox = SNew(SNumericEntryBox< int32 >);
		HorizontalBox->AddSlot().Padding(0, 0, 5, 0)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(false)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Value(VersionBuild)
			];
		NumericEntryBox->SetEnabled(false);
	}

	{
		TSharedRef< SNumericEntryBox< int32 > > NumericEntryBox = SNew(SNumericEntryBox< int32 >);
		HorizontalBox->AddSlot().Padding(0, 0, 5, 0)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(false)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Value(VersionPatch)
			];
		NumericEntryBox->SetEnabled(false);
	}

	Row.ValueWidget.Widget = HorizontalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_SETTINGS_ROW_VALUE_WIDGET_WIDTH);
}

void
FHoudiniRuntimeSettingsDetails::CreateHoudiniEngineEntry(
	const FText & EntryName,
	IDetailCategoryBuilder & DetailCategoryBuilder,
	int32 VersionMajor, int32 VersionMinor, int32 VersionApi)
{
	FDetailWidgetRow & Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	Row.NameWidget.Widget =
		SNew(STextBlock)
		.Text(EntryName)
		.Font(IDetailLayoutBuilder::GetDetailFont());

	TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);

	{
		TSharedRef< SNumericEntryBox< int32 > > NumericEntryBox = SNew(SNumericEntryBox< int32 >);
		HorizontalBox->AddSlot().Padding(0, 0, 5, 0)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(false)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Value(VersionMajor)
			];
		NumericEntryBox->SetEnabled(false);
	}

	{
		TSharedRef< SNumericEntryBox< int32 > > NumericEntryBox = SNew(SNumericEntryBox< int32 >);
		HorizontalBox->AddSlot().Padding(0, 0, 5, 0)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(false)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Value(VersionMinor)
			];
		NumericEntryBox->SetEnabled(false);
	}

	{
		TSharedRef< SNumericEntryBox< int32 > > NumericEntryBox = SNew(SNumericEntryBox< int32 >);
		HorizontalBox->AddSlot().Padding(0, 0, 5, 0)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(false)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Value(VersionApi)
			];
		NumericEntryBox->SetEnabled(false);
	}

	Row.ValueWidget.Widget = HorizontalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_SETTINGS_ROW_VALUE_WIDGET_WIDTH);
}

void
FHoudiniRuntimeSettingsDetails::CreateHAPILibraryPathEntry(
	const FString & LibHAPIPath,
	IDetailCategoryBuilder & DetailCategoryBuilder)
{
	FDetailWidgetRow & Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	FString LibHAPIName = FString::Printf(TEXT("Location of %s"), *FHoudiniEngineRuntimeUtils::GetLibHAPIName());

	Row.NameWidget.Widget =
		SNew(STextBlock)
		.Text(FText::FromString(LibHAPIName))
		.Font(IDetailLayoutBuilder::GetDetailFont());

	TSharedRef<STextBlock> TextBlock =
		SNew(STextBlock)
		.Text(FText::FromString(LibHAPIPath))
		.Font(IDetailLayoutBuilder::GetDetailFont());

	TextBlock->SetEnabled(false);
	Row.ValueWidget.Widget = TextBlock;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_SETTINGS_ROW_VALUE_WIDGET_WIDTH);
}

void
FHoudiniRuntimeSettingsDetails::CreateHAPILicenseEntry(
	const FString & LibHAPILicense,
	IDetailCategoryBuilder & DetailCategoryBuilder)
{
	FDetailWidgetRow & Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	FString LibHAPILicenseTypeText = TEXT("Acquired License Type");

	Row.NameWidget.Widget = SNew(STextBlock)
		.Text(FText::FromString(LibHAPILicenseTypeText))
		.Font(IDetailLayoutBuilder::GetDetailFont());

	TSharedRef<STextBlock> TextBlock = SNew(STextBlock)
		.Text(FText::FromString(LibHAPILicense))
		.Font(IDetailLayoutBuilder::GetDetailFont());

	TextBlock->SetEnabled(false);
	Row.ValueWidget.Widget = TextBlock;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_SETTINGS_ROW_VALUE_WIDGET_WIDTH);
}

#undef LOCTEXT_NAMESPACE
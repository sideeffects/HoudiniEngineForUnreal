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
#include "HoudiniAssetParameterFile.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineString.h"


UHoudiniAssetParameterFile::UHoudiniAssetParameterFile(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	Filters(TEXT(""))
{
	Values.Add(TEXT(""));
}


UHoudiniAssetParameterFile::~UHoudiniAssetParameterFile()
{

}


UHoudiniAssetParameterFile*
UHoudiniAssetParameterFile::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	UObject* Outer = InHoudiniAssetComponent;
	if(!Outer)
	{
		Outer = InParentParameter;
		if(!Outer)
		{
			// Must have either component or parent not null.
			check(false);
		}
	}

	UHoudiniAssetParameterFile* HoudiniAssetParameterFile = NewObject<UHoudiniAssetParameterFile>(Outer,
		UHoudiniAssetParameterFile::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterFile->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterFile;
}


bool
UHoudiniAssetParameterFile::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle file types.
	switch(ParmInfo.type)
	{
		case HAPI_PARMTYPE_PATH_FILE:
		{
			break;
		}

		case HAPI_PARMTYPE_PATH_FILE_GEO:
		{
			ParameterLabel += TEXT(" (geo)");
			break;
		}

		case HAPI_PARMTYPE_PATH_FILE_IMAGE:
		{
			ParameterLabel += TEXT(" (img)");
			break;
		}

		default:
		{
			return false;
		}
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.stringValuesIndex);

	// Get the actual value for this property.
	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.SetNum(TupleSize);
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmStringValues(FHoudiniEngine::Get().GetSession(), InNodeId, false,
		&StringHandles[0], ValuesIndex, TupleSize))
	{
		return false;
	}

	// Convert HAPI string handles to Unreal strings.
	Values.SetNum(TupleSize);
	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		FString ValueString = TEXT("");
		FHoudiniEngineString HoudiniEngineString(StringHandles[Idx]);
		HoudiniEngineString.ToFString(ValueString);

		// Detect and update relative paths.
		Values[Idx] = UpdateCheckRelativePath(ValueString);
	}

	// Retrieve filters for this file.
	if(ParmInfo.typeInfoSH > 0)
	{
		FHoudiniEngineString HoudiniEngineString(ParmInfo.typeInfoSH);
		if(HoudiniEngineString.ToFString(Filters))
		{
			if(!Filters.IsEmpty())
			{
				ParameterLabel = FString::Printf(TEXT("%s (%s)"), *ParameterLabel, *Filters);
			}
		}
	}

	return true;
}


void
UHoudiniAssetParameterFile::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << Values;
	Ar << Filters;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterFile::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	// Create the standard parameter name widget.
	CreateNameWidget(Row, true);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	FString FileTypeWidgetFilter = ComputeFiletypeFilter(Filters);
	FString BrowseWidgetDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		FString FileWidgetPath = Values[Idx];
		FString FileWidgetBrowsePath = BrowseWidgetDirectory;

		if(!FileWidgetPath.IsEmpty())
		{
			FString FileWidgetDirPath = FPaths::GetPath(FileWidgetPath);
			if(!FileWidgetDirPath.IsEmpty())
			{
				FileWidgetBrowsePath = FileWidgetDirPath;
			}
		}

		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SFilePathPicker)
			.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.BrowseButtonToolTip(LOCTEXT("FileButtonToolTipText", "Choose a file"))
			.BrowseDirectory(FileWidgetBrowsePath)
			.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
			.FilePath(FileWidgetPath)
			.FileTypeFilter(FileTypeWidgetFilter)
			.OnPathPicked(FOnPathPicked::CreateUObject(this,
				&UHoudiniAssetParameterFile::HandleFilePathPickerPathPicked, Idx))
		];
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

#endif


bool
UHoudiniAssetParameterFile::UploadParameterValue()
{
	for(int32 Idx = 0; Idx < Values.Num(); ++Idx)
	{
		std::string ConvertedString = TCHAR_TO_UTF8(*(Values[Idx]));
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmStringValue(
			FHoudiniEngine::Get().GetSession(), NodeId, ConvertedString.c_str(), ParmId, Idx))
		{
			return false;
		}
	}

	return Super::UploadParameterValue();
}


bool
UHoudiniAssetParameterFile::SetParameterVariantValue(const FVariant& Variant, int32 Idx, bool bTriggerModify,
	bool bRecordUndo)
{
	int32 VariantType = Variant.GetType();
	if(EVariantTypes::String == VariantType)
	{
		if(Idx >= 0 && Idx < Values.Num())
		{
			FString VariantStringValue = Variant.GetValue<FString>();

#if WITH_EDITOR

			FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniAssetParameterFileChange", "Houdini Parameter File: Changing a value"),
					HoudiniAssetComponent);

			if(!bRecordUndo)
			{
				Transaction.Cancel();
			}

			Modify();

#endif

			// Detect and fix relative paths.
			VariantStringValue = UpdateCheckRelativePath(VariantStringValue);

			MarkPreChanged(bTriggerModify);
			Values[Idx] = VariantStringValue;
			MarkChanged(bTriggerModify);

			return true;
		}
	}

	return false;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterFile::HandleFilePathPickerPathPicked(const FString& PickedPath, int32 Idx)
{
	if(Values[Idx] != PickedPath)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniAssetParameterFileChange", "Houdini Parameter File: Changing a value"),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		Values[Idx] = UpdateCheckRelativePath(PickedPath);

		// Mark this parameter as changed.
		MarkChanged();
	}
}

#endif


FString
UHoudiniAssetParameterFile::UpdateCheckRelativePath(const FString& PickedPath) const
{
	if(HoudiniAssetComponent && !PickedPath.IsEmpty() && FPaths::IsRelative(PickedPath))
	{
		const UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
		if(HoudiniAsset)
		{
			FString AssetFilePath = FPaths::GetPath(HoudiniAsset->AssetFileName);
			if(FPaths::FileExists(AssetFilePath))
			{
				FString UpdatedFileWidgetPath = FPaths::Combine(*AssetFilePath, *PickedPath);
				if(FPaths::FileExists(UpdatedFileWidgetPath))
				{
					return UpdatedFileWidgetPath;
				}
			}
		}
	}

	return PickedPath;
}


FString
UHoudiniAssetParameterFile::ComputeFiletypeFilter(const FString& FilterList) const
{
	FString FileTypeFilter = TEXT("All files (*.*)|*.*");

	if(!FilterList.IsEmpty())
	{
		FileTypeFilter = FString::Printf(TEXT("%s files (*.%s)|*.%s"), *FilterList, *FilterList, *FilterList);
	}

	return FileTypeFilter;
}


const FString&
UHoudiniAssetParameterFile::GetParameterValue(int32 Idx, const FString& DefaultValue) const
{
	if(Idx < Values.Num())
	{
		return Values[Idx];
	}

	return DefaultValue;
}


void
UHoudiniAssetParameterFile::SetParameterValue(const FString& InValue, int32 Idx, bool bTriggerModify, bool bRecordUndo)
{
	if(Idx >= Values.Num())
	{
		return;
	}

	if(Values[Idx] != InValue)
	{

#if WITH_EDITOR

		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniAssetParameterFileChange", "Houdini Parameter File: Changing a value"),
				HoudiniAssetComponent);

		Modify();

		if(!bRecordUndo)
		{
			Transaction.Cancel();
		}

#endif

		MarkPreChanged(bTriggerModify);
		Values[Idx] = InValue;
		MarkChanged(bTriggerModify);
	}
}

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"

DECLARE_DELEGATE_TwoParams( FOnLandscapeSelectionChanged, const FString&, AActor* )

class SLandscapeComboBox : public SComboBox<TSharedPtr<FString>>
{
	typedef TMap<FString,AActor*> LandscapeOptionsType;
	
	SLATE_BEGIN_ARGS(SLandscapeComboBox)
		: _Content()
	{}

	/** Slot for this button's content (optional) */
	SLATE_DEFAULT_SLOT( FArguments, Content )
	
	// SLATE_ARGUMENT( const TArray< TSharedPtr<FString> >&, LandscapeOptions )
	// Customized slate argument to deal with conversions of landscape options into combobox source options.
	SLATE_PRIVATE_ARGUMENT_VARIABLE( LandscapeOptionsType, LandscapeOptions );

	// SLATE_PRIVATE_ARGUMENT_FUNCTION ( const TArray< TSharedPtr<FString> >&, LandscapeOptions )
	WidgetArgsType& LandscapeOptions( const LandscapeOptionsType& LandscapeOptions)
	{
		// Cache the strings locally
		_LandscapeOptions = LandscapeOptions;
		return static_cast<WidgetArgsType*>(this)->Me();
	}

	/** The option that should be selected when the combo box is first created */
	SLATE_ARGUMENT( FString, CurrentSelection )

	SLATE_EVENT( FOnLandscapeSelectionChanged, OnLandscapeSelectionChanged )
	SLATE_EVENT( FOnGenerateWidget, OnGenerateWidget )

	// TODO: implement any additional slate args here that needs to be forwarded to the
	// underlying ComboBox slate widget.
	
	SLATE_END_ARGS()

	/**
	 * Construct the widget from a declaration
	 *
	 * @param InArgs   Declaration from which to construct the combo box
	 */
	void Construct( const FArguments& InArgs )
	{
		Options = InArgs._LandscapeOptions;
		Labels.Empty();
		
		// Create labels from the landscape names
		TArray<FString> Keys;
		TSharedPtr<FString> CurrentSelection;
		
		Options.GetKeys(Keys);
		for( const FString& Name : Keys)
		{
			Labels.Add(MakeShareable(new FString(Name)));
			if (InArgs._CurrentSelection == Name)
			{
				CurrentSelection = Labels.Last();
			}
		}

		// If we don't have labels or we don't have a current selection, initialize the string
		// with a custom message.
		if (Labels.Num() == 0 )
		{
			CurrentSelection = MakeShareable(new FString("No landscapes found"));
		}
		else if (!CurrentSelection.IsValid())
		{
			CurrentSelection = MakeShareable(new FString("No landscape selected"));
		}

		SelectedItemText = SNew( STextBlock )
			.Font( FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([=]() -> FText
			{
				return FText::FromString(*CurrentSelection.Get());
			})
		;

		this->OnGenerateWidget = InArgs._OnGenerateWidget;
		this->OnLandscapeSelectionChanged = InArgs._OnLandscapeSelectionChanged;
		
		SComboBox<TSharedPtr<FString>>::Construct(
			SComboBox<TSharedPtr<FString>>::FArguments()
			.OptionsSource(&Labels)
			.InitiallySelectedItem(CurrentSelection)
			.OnGenerateWidget(OnGenerateWidget)
			.Content()
			[
				SelectedItemText.ToSharedRef()
			]
			.OnSelectionChanged_Lambda([=](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
			{
				if (NewChoice.IsValid())
				{
					SelectedItemText->SetText( FText::FromString(*NewChoice) );
					// Find the actor that corresponds to the choice
					AActor* NewProxy = nullptr;
					if (Options.Contains(*NewChoice))
					{
						NewProxy = Options.FindChecked(*NewChoice);
					}
					OnLandscapeSelectionChanged.ExecuteIfBound(*NewChoice, NewProxy);
				}
			})
		);
	}

protected:
	LandscapeOptionsType Options;
	TArray<TSharedPtr<FString>> Labels;

	// Delegate that is invoked when the selected (landscape) item in the combo box changes
	FOnLandscapeSelectionChanged OnLandscapeSelectionChanged;

	// Delegate to invoke when we need to visualize an option as a widget.
	FOnGenerateWidget OnGenerateWidget;
	
	// Widget for the currently selected item 
	TSharedPtr<STextBlock> SelectedItemText;
};

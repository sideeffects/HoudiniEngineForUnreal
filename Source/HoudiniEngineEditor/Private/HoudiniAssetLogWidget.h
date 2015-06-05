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


class SWindow;

class SHoudiniAssetLogWidget : public SCompoundWidget
{
	public:

		SLATE_BEGIN_ARGS(SHoudiniAssetLogWidget)
			: _WidgetWindow(), _LogText(TEXT(""))
		{}

		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(FString, LogText)
		SLATE_END_ARGS()

	public:

		SHoudiniAssetLogWidget();

	public:

		/** Get log text. **/
		FText GetLogText() const;

	public:

		/** Widget construct. **/
		void Construct(const FArguments& InArgs);

	public:

		/** Called when Ok button is pressed. **/
		FReply OnButtonOk();

		/** Called when copy to clipboard button is pressed. **/
		FReply OnButtonCopyToClipboard();

	public:

		/** Parent widget window. **/
		TSharedPtr<SWindow> WidgetWindow;

		/** Text of the log. **/
		FString LogText;
};

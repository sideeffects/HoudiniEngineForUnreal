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

class SHoudiniAssetLogWidget : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS( SHoudiniAssetLogWidget )
        : _LogText( TEXT( "" ) )
    {}

    SLATE_ARGUMENT( FString, LogText )
        SLATE_END_ARGS()

    /** Widget construct. **/
    void Construct( const FArguments & InArgs );
};

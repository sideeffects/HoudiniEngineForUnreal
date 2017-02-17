/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
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

#include "HoudiniApi.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"

#include <vector>

FHoudiniEngineString::FHoudiniEngineString()
    : StringId( -1 )
{}

FHoudiniEngineString::FHoudiniEngineString( int32 InStringId )
    : StringId( InStringId )
{}

FHoudiniEngineString::FHoudiniEngineString( const FHoudiniEngineString & Other )
    : StringId( Other.StringId )
{}

FHoudiniEngineString &
FHoudiniEngineString::operator=( const FHoudiniEngineString & Other )
{
    if ( this != &Other )
        StringId = Other.StringId;

    return *this;
}

bool
FHoudiniEngineString::operator==( const FHoudiniEngineString & Other ) const
{
    return Other.StringId == StringId;
}

bool
FHoudiniEngineString::operator!=( const FHoudiniEngineString & Other ) const
{
    return Other.StringId != StringId;
}

int32
FHoudiniEngineString::GetId() const
{
    return StringId;
}

bool
FHoudiniEngineString::HasValidId() const
{
    return StringId > 0;
}

bool
FHoudiniEngineString::ToStdString( std::string & String ) const
{
    String = "";

    if ( StringId >= 0 )
    {
        int32 NameLength = 0;
        if ( FHoudiniApi::GetStringBufLength(
            FHoudiniEngine::Get().GetSession(), StringId, &NameLength ) == HAPI_RESULT_SUCCESS )
        {
            if ( NameLength )
            {
                std::vector< char > NameBuffer( NameLength, '\0' );
                if ( FHoudiniApi::GetString(
                    FHoudiniEngine::Get().GetSession(), StringId,
                    &NameBuffer[ 0 ], NameLength ) == HAPI_RESULT_SUCCESS )
                {
                    String = std::string( NameBuffer.begin(), NameBuffer.end() );
                    return true;
                }
            }
        }
    }

    return false;
}

bool
FHoudiniEngineString::ToFName( FName & Name ) const
{
    Name = NAME_None;
    FString NameString = TEXT( "" );
    if ( ToFString( NameString ) )
    {
        Name = FName( *NameString );
        return true;
    }

    return false;
}

bool
FHoudiniEngineString::ToFString( FString & String ) const
{
    String = TEXT( "" );
    std::string NamePlain = "";

    if ( ToStdString( NamePlain ) )
    {
        String = UTF8_TO_TCHAR( NamePlain.c_str() );
        return true;
    }

    return false;
}

bool
FHoudiniEngineString::ToFText( FText & Text ) const
{
    Text = FText::GetEmpty();
    FString NameString = TEXT( "" );

    if ( ToFString( NameString ) )
    {
        Text = FText::FromString( NameString );
        return true;
    }

    return false;
}

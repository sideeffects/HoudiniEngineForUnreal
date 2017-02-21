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

        virtual void CustomizeDetails( IDetailLayoutBuilder & DetailBuilder ) override;

    public:

        /** Create an instance of this detail layout class. **/
        static TSharedRef< IDetailCustomization > MakeInstance();

    protected:

        /** Used to create Houdini version entry. **/
        void CreateHoudiniEntry(
            const FText & EntryName, IDetailCategoryBuilder & DetailCategoryBuilder,
            int32 VersionMajor, int32 VersionMinor, int32 VersionBuild, int32 VersionPatch );

        /** Used to create Houdini Engine version entry. **/
        void CreateHoudiniEngineEntry(
            const FText & EntryName, IDetailCategoryBuilder & DetailCategoryBuilder,
            int32 VersionMajor, int32 VersionMinor, int32 VersionApi );

        /** Used to create libHAPI dynamic library path entry. **/
        void CreateHAPILibraryPathEntry(
            const FString & LibHAPIPath, IDetailCategoryBuilder & DetailCategoryBuilder );

        /** Used to create libHAPI license information entry. **/
        void CreateHAPILicenseEntry(
            const FString & LibHAPILicense, IDetailCategoryBuilder & DetailCategoryBuilder );
};

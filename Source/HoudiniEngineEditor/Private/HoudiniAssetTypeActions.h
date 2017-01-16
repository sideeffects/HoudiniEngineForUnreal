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

class UClass;
class UObject;
class UHoudiniAsset;
class UThumbnailInfo;

class FHoudiniAssetTypeActions : public FAssetTypeActions_Base
{
    /** FAssetTypeActions_Base methods. **/
    public:

        virtual FText GetName() const override;
        virtual FColor GetTypeColor() const override;
        virtual UClass* GetSupportedClass() const override;
        virtual uint32 GetCategories() override;
        virtual UThumbnailInfo * GetThumbnailInfo( UObject * Asset) const override;
        virtual bool HasActions(const TArray< UObject * > & InObjects) const override;
        virtual void GetActions(const TArray< UObject * > & InObjects, class FMenuBuilder & MenuBuilder ) override;

    protected:

        /** Handler for reimport option. **/
        void ExecuteReimport( TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets );

        /** Handler for rebuild all option **/
        void ExecuteRebuildAllInstances( TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets );

        /** Handler for find in explorer option */
        void ExecuteFindInExplorer( TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets );

        /** Handler for the open in Houdini option */
        void ExecuteOpenInHoudini( TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets );
};

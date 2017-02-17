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
#include "HoudiniEngineSubstance.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniMaterialObject.h"
#include "HoudiniParameterObject.h"
#include "HoudiniEngineString.h"

bool
FHoudiniEngineSubstance::GetSubstanceMaterialName(
    const HAPI_MaterialInfo & MaterialInfo,
    FString & SubstanceMaterialName )
{
    SubstanceMaterialName = TEXT( "" );
    FHoudiniMaterialObject HoudiniMaterialObject( MaterialInfo );
    
    if ( !HoudiniMaterialObject.IsSubstance() )
        return false;

    FHoudiniParameterObject HoudiniParameterObject;
    if ( !HoudiniMaterialObject.HapiLocateParameterByName( HAPI_UNREAL_PARAM_SUBSTANCE_FILENAME, HoudiniParameterObject ) )
        return false;

    if ( !HoudiniParameterObject.HapiCheckParmCategoryPath() )
        return false;

    FHoudiniEngineString HoudiniEngineString;
    if ( !HoudiniParameterObject.HapiGetValue( HoudiniEngineString ) )
        return false;

    if ( !HoudiniEngineString.ToFString( SubstanceMaterialName ) )
        return false;

    return true;
}

#if WITH_EDITOR

UObject *
FHoudiniEngineSubstance::LoadSubstanceInstanceFactory(
    UClass * InstanceFactoryClass,
    const FString & SubstanceMaterialName )
{
    UObject * SubstanceInstanceFactory = nullptr;
    TArray< FAssetData > SubstanceInstaceFactories;

    FAssetRegistryModule & AssetRegistryModule =
        FModuleManager::LoadModuleChecked< FAssetRegistryModule >( TEXT( "AssetRegistry" ) );
    AssetRegistryModule.Get().GetAssetsByClass( InstanceFactoryClass->GetFName(), SubstanceInstaceFactories );

    for ( int32 FactoryIdx = 0, FactoryNum = SubstanceInstaceFactories.Num(); FactoryIdx < FactoryNum; ++FactoryIdx )
    {
        const FAssetData & AssetData = SubstanceInstaceFactories[ FactoryIdx ];
        const FString & AssetName = AssetData.AssetName.ToString();
        if ( AssetName.Equals( SubstanceMaterialName ) )
        {
            const FString & AssetObjectPath = AssetData.ObjectPath.ToString();
            SubstanceInstanceFactory = StaticLoadObject(
                InstanceFactoryClass, nullptr, *AssetObjectPath, nullptr,
                LOAD_None, nullptr );
            if ( SubstanceInstanceFactory )
                break;
        }
    }

    return SubstanceInstanceFactory;
}

UObject *
FHoudiniEngineSubstance::LoadSubstanceGraphInstance( UClass * GraphInstanceClass, UObject * InstanceFactory )
{
    UObject * MatchedSubstanceGraphInstance = nullptr;
    TArray< FAssetData > SubstanceGraphInstances;

    // Get Substance instance factory pointer property.
    UObjectProperty * FactoryParentProperty =
        Cast< UObjectProperty >( GraphInstanceClass->FindPropertyByName( HAPI_UNREAL_SUBSTANCE_PROPERTY_FACTORY_PARENT ) );
    if ( !FactoryParentProperty )
        return nullptr;

    FAssetRegistryModule & AssetRegistryModule =
        FModuleManager::LoadModuleChecked< FAssetRegistryModule >( TEXT( "AssetRegistry" ) );
    AssetRegistryModule.Get().GetAssetsByClass( GraphInstanceClass->GetFName(), SubstanceGraphInstances );

    for ( int32 GraphInstanceIdx = 0, GraphInstanceNum = SubstanceGraphInstances.Num();
        GraphInstanceIdx < GraphInstanceNum; ++GraphInstanceIdx )
    {
        const FAssetData & AssetData = SubstanceGraphInstances[GraphInstanceIdx];
        const FString & AssetObjectPath = AssetData.ObjectPath.ToString();
        UObject * GraphInstanceObject =
            StaticLoadObject( GraphInstanceClass, nullptr, *AssetObjectPath, nullptr, LOAD_None, nullptr );
        if ( GraphInstanceObject )
        {
            void * FactoryParentPropertyValue = FactoryParentProperty->ContainerPtrToValuePtr< void >( GraphInstanceObject );
            UObject * InstanceFactoryObject = FactoryParentProperty->GetObjectPropertyValue( FactoryParentPropertyValue );
            if ( InstanceFactory == InstanceFactoryObject )
            {
                MatchedSubstanceGraphInstance = GraphInstanceObject;
                break;
            }
        }
    }

    return MatchedSubstanceGraphInstance;
}

#endif

bool
FHoudiniEngineSubstance::RetrieveSubstanceRTTIClasses(
    UClass *& InstanceFactoryClass,
    UClass *& GraphInstanceClass,
    UClass *& UtilityClass )
{
    InstanceFactoryClass = nullptr;
    GraphInstanceClass = nullptr;
    UtilityClass = nullptr;

    UClass * SubstanceInstaceFactoryClass = FindObject< UClass >(
        ANY_PACKAGE, HAPI_UNREAL_SUBSTANCE_CLASS_INSTANCE_FACTORY );
    if ( !SubstanceInstaceFactoryClass )
        return false;

    UClass * SubstanceGraphInstanceClass =
        FindObject< UClass >( ANY_PACKAGE, HAPI_UNREAL_SUBSTANCE_CLASS_GRAPH_INSTANCE );
    if ( !SubstanceGraphInstanceClass )
        return false;

    UClass * SubstanceUtilityClass = FindObject< UClass >( ANY_PACKAGE, HAPI_UNREAL_SUBSTANCE_CLASS_UTILITY );
    if ( !SubstanceUtilityClass )
        return false;

    InstanceFactoryClass = SubstanceInstaceFactoryClass;
    GraphInstanceClass = SubstanceGraphInstanceClass;
    UtilityClass = SubstanceUtilityClass;

    return true;
}

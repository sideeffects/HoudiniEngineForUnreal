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
class FString;
class UObject;
struct HAPI_MaterialInfo;

struct HOUDINIENGINERUNTIME_API FHoudiniEngineSubstance
{
#if WITH_EDITOR

    public:

        /** Used to locate and load (if found) Substance instance factory object. **/
        static UObject * LoadSubstanceInstanceFactory( UClass * InstanceFactoryClass, const FString & SubstanceMaterialName );

        /** Used to locate and load (if found) Substance graph instance object. **/
        static UObject * LoadSubstanceGraphInstance( UClass * GraphInstanceClass, UObject * InstanceFactory );

#endif // WITH_EDITOR

        /** Retrieve Substance RTTI classes we are interested in. **/
        static bool RetrieveSubstanceRTTIClasses(
            UClass *& InstanceFactoryClass,
            UClass *& GraphInstanceClass,
            UClass *& UtilityClass );

        /** HAPI: Check if material is a Substance material. If it is, return its name by reference. **/
        static bool GetSubstanceMaterialName( const HAPI_MaterialInfo & MaterialInfo, FString & SubstanceMaterialName );
};

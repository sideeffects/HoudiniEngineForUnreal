/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 * COMMENTS:
 */

#ifndef __HAPI_HELPERS_h__
#define __HAPI_HELPERS_h__

#include "HAPI_API.h"
#include "HAPI_Common.h"

// TIME ---------------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_TimelineOptions_Init( HAPI_TimelineOptions * in );
HAPI_DECL_RETURN( HAPI_TimelineOptions )
    HAPI_TimelineOptions_Create();

// ASSETS -------------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_AssetInfo_Init( HAPI_AssetInfo * in );
HAPI_DECL_RETURN( HAPI_AssetInfo )
    HAPI_AssetInfo_Create();

HAPI_DECL_RETURN( void )
    HAPI_CookOptions_Init( HAPI_CookOptions * in );
HAPI_DECL_RETURN( HAPI_CookOptions )
    HAPI_CookOptions_Create();
HAPI_DECL_RETURN( HAPI_Bool )
    HAPI_CookOptions_AreEqual(
        const HAPI_CookOptions * left,
        const HAPI_CookOptions * right );

// NODES --------------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_NodeInfo_Init( HAPI_NodeInfo * in );
HAPI_DECL_RETURN( HAPI_NodeInfo )
    HAPI_NodeInfo_Create();

// PARAMETERS ---------------------------------------------------------------

/// Clears the struct to default values.
HAPI_DECL_RETURN( void )
    HAPI_ParmInfo_Init( HAPI_ParmInfo * in );

/// Creates a struct with default values and returns it.
HAPI_DECL_RETURN( HAPI_ParmInfo )
    HAPI_ParmInfo_Create();

/// Convenience function that checks on the value of the ::HAPI_ParmInfo::type
/// field to tell you the underlying data type.
/// @{
HAPI_DECL_RETURN( HAPI_Bool )
    HAPI_ParmInfo_IsInt( const HAPI_ParmInfo * in );
HAPI_DECL_RETURN( HAPI_Bool )
    HAPI_ParmInfo_IsFloat( const HAPI_ParmInfo * in );
HAPI_DECL_RETURN( HAPI_Bool )
    HAPI_ParmInfo_IsString( const HAPI_ParmInfo * in );
HAPI_DECL_RETURN( HAPI_Bool )
    HAPI_ParmInfo_IsPath( const HAPI_ParmInfo * in );
HAPI_DECL_RETURN( HAPI_Bool )
    HAPI_ParmInfo_IsNode( const HAPI_ParmInfo * in );
/// @}

/// Parameter has no underlying No data type. Examples of this are UI items
/// such as folder lists and separators.
HAPI_DECL_RETURN( HAPI_Bool )
    HAPI_ParmInfo_IsNonValue( const HAPI_ParmInfo * in );

/// Convenience function. If the parameter can be represented by this data
/// type, it returns ::HAPI_ParmInfo::size, and zero otherwise.
/// @{
HAPI_DECL_RETURN( int )
    HAPI_ParmInfo_GetIntValueCount( const HAPI_ParmInfo * in );
HAPI_DECL_RETURN( int )
    HAPI_ParmInfo_GetFloatValueCount( const HAPI_ParmInfo * in );
HAPI_DECL_RETURN( int )
    HAPI_ParmInfo_GetStringValueCount( const HAPI_ParmInfo* in );
/// @}

HAPI_DECL_RETURN( void )
    HAPI_ParmChoiceInfo_Init( HAPI_ParmChoiceInfo * in );
HAPI_DECL_RETURN( HAPI_ParmChoiceInfo )
    HAPI_ParmChoiceInfo_Create();

// HANDLES ------------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_HandleInfo_Init( HAPI_HandleInfo * in );
HAPI_DECL_RETURN( HAPI_HandleInfo )
    HAPI_HandleInfo_Create();

HAPI_DECL_RETURN( void )
    HAPI_HandleBindingInfo_Init( HAPI_HandleBindingInfo * in );
HAPI_DECL_RETURN( HAPI_HandleBindingInfo )
    HAPI_HandleBindingInfo_Create();

// OBJECTS ------------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_ObjectInfo_Init( HAPI_ObjectInfo * in );
HAPI_DECL_RETURN( HAPI_ObjectInfo )
    HAPI_ObjectInfo_Create();

// GEOMETRY -----------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_GeoInfo_Init( HAPI_GeoInfo * in );
HAPI_DECL_RETURN( HAPI_GeoInfo )
    HAPI_GeoInfo_Create();
HAPI_DECL_RETURN( int )
    HAPI_GeoInfo_GetGroupCountByType(
        HAPI_GeoInfo * in, HAPI_GroupType type );

HAPI_DECL_RETURN( void )
    HAPI_GeoInputInfo_Init( HAPI_GeoInputInfo * in );
HAPI_DECL_RETURN( HAPI_GeoInputInfo )
    HAPI_GeoInputInfo_Create();

HAPI_DECL_RETURN( void )
    HAPI_PartInfo_Init( HAPI_PartInfo * in );
HAPI_DECL_RETURN( HAPI_PartInfo )
    HAPI_PartInfo_Create();
HAPI_DECL_RETURN( int )
    HAPI_PartInfo_GetElementCountByAttributeOwner(
        HAPI_PartInfo * in, HAPI_AttributeOwner owner );
HAPI_DECL_RETURN( int )
    HAPI_PartInfo_GetElementCountByGroupType(
        HAPI_PartInfo * in, HAPI_GroupType type );
HAPI_DECL_RETURN( int )
    HAPI_PartInfo_GetAttributeCountByOwner(
        HAPI_PartInfo * in, HAPI_AttributeOwner owner );

HAPI_DECL_RETURN( void )
    HAPI_AttributeInfo_Init( HAPI_AttributeInfo * in );
HAPI_DECL_RETURN( HAPI_AttributeInfo )
    HAPI_AttributeInfo_Create();

// MATERIALS ----------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_MaterialInfo_Init( HAPI_MaterialInfo * in );
HAPI_DECL_RETURN( HAPI_MaterialInfo )
    HAPI_MaterialInfo_Create();

HAPI_DECL_RETURN( void )
    HAPI_ImageFileFormat_Init( HAPI_ImageFileFormat *in );
HAPI_DECL_RETURN( HAPI_ImageFileFormat )
    HAPI_ImageFileFormat_Create();

HAPI_DECL_RETURN( void )
    HAPI_ImageInfo_Init( HAPI_ImageInfo * in );
HAPI_DECL_RETURN( HAPI_ImageInfo )
    HAPI_ImageInfo_Create();

// ANIMATION ----------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_Keyframe_Init( HAPI_Keyframe * in );
HAPI_DECL_RETURN( HAPI_Keyframe )
    HAPI_Keyframe_Create();

// VOLUMES ------------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_VolumeInfo_Init( HAPI_VolumeInfo * in );
HAPI_DECL_RETURN( HAPI_VolumeInfo )
    HAPI_VolumeInfo_Create();

HAPI_DECL_RETURN( void )
    HAPI_VolumeTileInfo_Init( HAPI_VolumeTileInfo * in );
HAPI_DECL_RETURN( HAPI_VolumeTileInfo )
    HAPI_VolumeTileInfo_Create();

// CURVES -------------------------------------------------------------------

HAPI_DECL_RETURN( void )
    HAPI_CurveInfo_Init( HAPI_CurveInfo * in );
HAPI_DECL_RETURN( HAPI_CurveInfo )
    HAPI_CurveInfo_Create();

#endif // __HAPI_HELPERS_h__

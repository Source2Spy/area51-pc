//=========================================================================
//
//  GeometryAsset.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Model/GeometryAsset.hpp"

#include <algorithm>

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void geometry_asset::Clear( void )
{
    Source.Clear();
    Render.Clear();
    Materials.Clear();
    Skeleton.Clear();
    LODs.Clear();
    Collision.Clear();
    Physics.Clear();
    EngineMetadata.Clear();
}

//=========================================================================

void geometry_asset::CleanWeights( s32 MaxNumWeights, f32 MinWeightValue )
{
    for( geometry_vertex& Vertex : Render.Vertices )
    {
        std::stable_sort( Vertex.Weight.begin(), Vertex.Weight.end(),
                          []( const geometry_weight& A, const geometry_weight& B ) { return A.Weight > B.Weight; } );

        if( MaxNumWeights > 0 && (s32)Vertex.Weight.size() > MaxNumWeights )
            Vertex.Weight.resize( MaxNumWeights );

        Vertex.Weight.erase( std::remove_if( Vertex.Weight.begin(), Vertex.Weight.end(),
                                             [MinWeightValue]( const geometry_weight& Weight )
                                             { return Weight.Weight < MinWeightValue; } ),
                             Vertex.Weight.end() );

        if( Vertex.Weight.empty() )
        {
            Vertex.Weight.push_back( geometry_weight() );
            Vertex.Weight[0].iBone = 0;
            Vertex.Weight[0].Weight = 1.0f;
            continue;
        }

        f32 TotalWeight = 0.0f;
        for( const geometry_weight& Weight : Vertex.Weight )
            TotalWeight += Weight.Weight;
        if( TotalWeight <= 0.0f )
        {
            Vertex.Weight.resize( 1 );
            Vertex.Weight[0].iBone = 0;
            Vertex.Weight[0].Weight = 1.0f;
            continue;
        }
        for( geometry_weight& Weight : Vertex.Weight )
            Weight.Weight /= TotalWeight;
    }
}

//=========================================================================

bbox geometry_asset::GetBBox( void ) const
{
    bbox Result;
    Result.Clear();
    for( const geometry_vertex& Vertex : Render.Vertices )
        Result += Vertex.Position;
    return Result;
}

//=========================================================================

s32 geometry_asset::GetBoneIDFromName( const char* pBoneName, xbool bAnywhere ) const
{
    if( !pBoneName )
        return -1;
    for( s32 i = 0; i < (s32)Skeleton.Bones.size(); ++i )
    {
        if( bAnywhere )
        {
            if( x_stristr( Skeleton.Bones[i].Name.c_str(), pBoneName ) )
                return i;
        }
        else if( x_stricmp( Skeleton.Bones[i].Name.c_str(), pBoneName ) == 0 )
        {
            return i;
        }
    }
    return -1;
}

//=========================================================================

s32 geometry_asset::GetRigidBodyIDFromName( const char* pRigidBodyName, xbool bAnywhere ) const
{
    if( !pRigidBodyName )
        return -1;
    for( s32 i = 0; i < (s32)Physics.RigidBodies.size(); ++i )
    {
        if( bAnywhere )
        {
            if( x_stristr( Physics.RigidBodies[i].Name.c_str(), pRigidBodyName ) )
                return i;
        }
        else if( x_stricmp( Physics.RigidBodies[i].Name.c_str(), pRigidBodyName ) == 0 )
        {
            return i;
        }
    }
    return -1;
}

//=========================================================================

void geometry_asset::PrintHierarchy( void ) const
{
    x_printf( "\nHierarchy is %d Bones:\n", (s32)Skeleton.Bones.size() );
    for( s32 i = 0; i < (s32)Skeleton.Bones.size(); ++i )
    {
        s32 Parent = Skeleton.Bones[i].iParent;
        while( Parent != -1 )
        {
            x_printf( " " );
            Parent = ( Parent >= 0 && Parent < (s32)Skeleton.Bones.size() ) ? Skeleton.Bones[Parent].iParent : -1;
        }
        if( Skeleton.Bones[i].iParent >= 0 && Skeleton.Bones[i].iParent < (s32)Skeleton.Bones.size() )
        {
            x_printf( "%3d: \"%s\" (Parent=\"%s\", LODGroup=%d)\n", i, Skeleton.Bones[i].Name.c_str(),
                      Skeleton.Bones[Skeleton.Bones[i].iParent].Name.c_str(), Skeleton.Bones[i].LODGroup );
        }
        else
        {
            x_printf( "%3d: \"%s\" (LODGroup=%d)\n", i, Skeleton.Bones[i].Name.c_str(), Skeleton.Bones[i].LODGroup );
        }
    }
    x_printf( "\n" );
}

//=========================================================================

void geometry_asset::PrintRigidBodies( void ) const
{
    x_printf( "\n%d Rigid bodies found:\n", (s32)Physics.RigidBodies.size() );
    for( s32 i = 0; i < (s32)Physics.RigidBodies.size(); ++i )
    {
        x_printf( "%3d: \"%s\"\n", i, Physics.RigidBodies[i].Name.c_str() );
    }
    x_printf( "\n" );
}

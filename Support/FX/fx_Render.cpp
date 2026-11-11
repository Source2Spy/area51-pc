//=============================================================================
//
//  FX Render Extraction
//
//=============================================================================

#include "fx_Render.hpp"

#include "Entropy/e_ScratchMem.hpp"
#include "x_debug.hpp"

//=============================================================================

namespace
{
    enum serialized_fx_combine_mode
    {
        FX_COMBINE_SUBTRACTIVE       = -1,
        FX_COMBINE_ALPHA             =  0,
        FX_COMBINE_ADDITIVE          =  1,
        FX_COMBINE_GLOW_SUBTRACTIVE  =  9,
        FX_COMBINE_GLOW_ALPHA        = 10,
        FX_COMBINE_GLOW_ADDITIVE     = 11,
        FX_COMBINE_DISTORTION        = 20
    };

    static xbool AppendTriangle( u16*         pIndices,
                                 s32          IndexCapacity,
                                 s32&         IndexCount,
                                 s16          Index0,
                                 s16          Index1,
                                 s16          Index2,
                                 s32          nVertices )
    {
        if( (Index0 < 0) || (Index1 < 0) || (Index2 < 0) ||
            (Index0 >= nVertices) || (Index1 >= nVertices) || (Index2 >= nVertices) )
        {
            return FALSE;
        }

        if( (Index0 == Index1) || (Index1 == Index2) || (Index2 == Index0) )
            return TRUE;

        if( (IndexCount + 3) > IndexCapacity )
            return FALSE;

        pIndices[IndexCount++] = (u16)Index0;
        pIndices[IndexCount++] = (u16)Index1;
        pIndices[IndexCount++] = (u16)Index2;
        return TRUE;
    }

    static xbool ConvertTriangleList( u16*         pOut,
                                      s32          OutCapacity,
                                      s32&         OutCount,
                                      const s16*   pIndices,
                                      s32          nIndices,
                                      s32          nVertices )
    {
        if( (nIndices % 3) != 0 )
            return FALSE;

        for( s32 i = 0; i < nIndices; i += 3 )
        {
            if( !AppendTriangle( pOut, OutCapacity, OutCount,
                                 pIndices[i + 0],
                                 pIndices[i + 1],
                                 pIndices[i + 2],
                                 nVertices ) )
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    static xbool ConvertTriangleStrips( u16*         pOut,
                                        s32          OutCapacity,
                                        s32&         OutCount,
                                        const s16*   pIndices,
                                        s32          nIndices,
                                        s32          nVertices )
    {
        s16 Window[2] = { -1, -1 };
        s32 StripVertex = 0;

        for( s32 i = 0; i < nIndices; i++ )
        {
            const s16 Index = pIndices[i];
            if( Index == -1 )
            {
                Window[0]   = -1;
                Window[1]   = -1;
                StripVertex = 0;
                continue;
            }

            if( (Index < 0) || (Index >= nVertices) )
                return FALSE;

            if( StripVertex >= 2 )
            {
                const xbool Odd    = (StripVertex & 1) != 0;
                const s16   Index0 = Odd ? Window[1] : Window[0];
                const s16   Index1 = Odd ? Window[0] : Window[1];

                if( !AppendTriangle( pOut, OutCapacity, OutCount,
                                     Index0, Index1, Index, nVertices ) )
                    return FALSE;
            }

            Window[0] = Window[1];
            Window[1] = Index;
            StripVertex++;
        }

        return TRUE;
    }
}

//=============================================================================

render::primitive_draw_desc fx_BuildMaterial( s32            CombineMode,
                                                xbool          ReadZ,
                                                render::primitive_sampler_mode Sampler )
{
    render::primitive_blend_mode Blend = render::PRIMITIVE_BLEND_ALPHA;
    render::primitive_output_mode Output = render::PRIMITIVE_OUTPUT_COUNT;

    switch( CombineMode )
    {
        case FX_COMBINE_SUBTRACTIVE:
            Blend  = render::PRIMITIVE_BLEND_SUBTRACTIVE;
            Output = render::PRIMITIVE_OUTPUT_COLOR;
            break;

        case FX_COMBINE_ALPHA:
            Blend  = render::PRIMITIVE_BLEND_ALPHA;
            Output = render::PRIMITIVE_OUTPUT_COLOR;
            break;

        case FX_COMBINE_ADDITIVE:
            Blend  = render::PRIMITIVE_BLEND_ADDITIVE;
            Output = render::PRIMITIVE_OUTPUT_COLOR;
            break;

        case FX_COMBINE_GLOW_SUBTRACTIVE:
            Blend  = render::PRIMITIVE_BLEND_SUBTRACTIVE;
            Output = render::PRIMITIVE_OUTPUT_GLOW;
            break;

        case FX_COMBINE_GLOW_ALPHA:
            Blend  = render::PRIMITIVE_BLEND_ALPHA;
            Output = render::PRIMITIVE_OUTPUT_GLOW;
            break;

        case FX_COMBINE_GLOW_ADDITIVE:
            Blend  = render::PRIMITIVE_BLEND_ADDITIVE;
            Output = render::PRIMITIVE_OUTPUT_GLOW;
            break;

        case FX_COMBINE_DISTORTION:
            Blend  = render::PRIMITIVE_BLEND_ALPHA;
            Output = render::PRIMITIVE_OUTPUT_DISTORTION;
            break;

        default:
            ASSERTS( FALSE, "Unknown FX combine mode" );
            break;
    }

    return render::primitive_draw_desc( NULL,
                                        render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                        Blend,
                                        ReadZ ? render::PRIMITIVE_DEPTH_READ_ONLY : render::PRIMITIVE_DEPTH_DISABLED,
                                        render::PRIMITIVE_RASTER_SOLID_NO_CULL,
                                        Sampler,
                                        Output );
}

//=============================================================================

render::primitive_draw_desc fx_CreateMaterial( const texture& Texture, s32 CombineMode,
                                                xbool ReadZ, render::primitive_sampler_mode Sampler )
{
    render::primitive_draw_desc Desc = fx_BuildMaterial( CombineMode, ReadZ, Sampler );
    Desc.pTexture = &Texture;
    return Desc;
}

//=============================================================================

xbool fx_SubmitMesh( const render::primitive_draw_desc& Material,
                     const matrix4&                         LocalToWorld,
                     const vector3*                         pPositions,
                     const vector2*                         pUVs,
                     const xcolor*                          pColors,
                     s32                                    nVertices,
                     const s16*                             pIndices,
                     s32                                    nIndices,
                     fx_mesh_topology                       Topology )
{
    if( !pPositions || !pUVs || !pColors || !pIndices ||
        (nVertices <= 0) || (nVertices > render::MAX_PRIMITIVE_VERTICES) ||
        (nIndices <= 0) )
    {
        return FALSE;
    }

    if( Material.Topology != render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST )
        return FALSE;

    const s32 IndexCapacity =
        (Topology == FX_MESH_TRIANGLE_LIST) ? nIndices : (nIndices * 3);
    if( (Topology == FX_MESH_TRIANGLE_STRIP) && (nIndices > (S32_MAX / 3)) )
        return FALSE;

    smem_StackPushMarker();
    render::primitive_vertex* pVertices =
        (render::primitive_vertex*)smem_StackAlloc( nVertices * sizeof(render::primitive_vertex) );
    u16* pConvertedIndices = (u16*)smem_StackAlloc( IndexCapacity * sizeof(u16) );
    if( !pVertices || !pConvertedIndices )
    {
        smem_StackPopToMarker();
        return FALSE;
    }

    for( s32 i = 0; i < nVertices; i++ )
        pVertices[i] = render::primitive_vertex( pPositions[i], pUVs[i], pColors[i] );

    s32 ConvertedIndexCount = 0;
    xbool Converted = FALSE;
    switch( Topology )
    {
        case FX_MESH_TRIANGLE_LIST:
            Converted = ConvertTriangleList( pConvertedIndices,
                                             IndexCapacity,
                                             ConvertedIndexCount,
                                             pIndices,
                                             nIndices,
                                             nVertices );
            break;

        case FX_MESH_TRIANGLE_STRIP:
            Converted = ConvertTriangleStrips( pConvertedIndices,
                                               IndexCapacity,
                                               ConvertedIndexCount,
                                               pIndices,
                                               nIndices,
                                               nVertices );
            break;

        default:
            smem_StackPopToMarker();
            return FALSE;
    }

    if( !Converted )
    {
        smem_StackPopToMarker();
        return FALSE;
    }

    if( ConvertedIndexCount == 0 )
    {
        smem_StackPopToMarker();
        return TRUE;
    }

    const xbool Submitted = render::SubmitPrimitives( Material,
                                                      LocalToWorld,
                                                      pVertices,
                                                      nVertices,
                                                      pConvertedIndices,
                                                      ConvertedIndexCount );
    smem_StackPopToMarker();
    return Submitted;
}

//=============================================================================

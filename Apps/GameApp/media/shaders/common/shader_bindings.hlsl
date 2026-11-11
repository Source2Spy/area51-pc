//==============================================================================
//
//  shader_bindings.hlsl
//
//  Shader resource binding declarations.
//
//==============================================================================

#ifndef SHADER_BINDINGS_HLSL
#define SHADER_BINDINGS_HLSL

//==============================================================================
//  DEFINES
//==============================================================================

#define A51_JOIN_TOKENS_INNER(A, B) A##B
#define A51_JOIN_TOKENS(A, B)       A51_JOIN_TOKENS_INNER(A, B)
#define A51_REGISTER_T(N)           A51_JOIN_TOKENS(t, N)
#define A51_REGISTER_S(N)           A51_JOIN_TOKENS(s, N)
#define A51_REGISTER_B(N)           A51_JOIN_TOKENS(b, N)
#define A51_REGISTER_U(N)           A51_JOIN_TOKENS(u, N)
#define A51_REGISTER_SPACE(N)       A51_JOIN_TOKENS(space, N)
#define A51_READONLY_STORAGE_TEXTURE2D(T) Texture2D<T>

#if defined(__spirv__)
    #define A51_VK_BINDING(B, S)        [[vk::binding(B, S)]]
    #define A51_VK_COMBINED_SAMPLER     [[vk::combinedImageSampler]]
#else
    #define A51_VK_BINDING(B, S)
    #define A51_VK_COMBINED_SAMPLER
#endif

#define A51_RESOURCE_SLOT(N) A51_JOIN_TOKENS(A51_RESOURCE_SLOT_, N)
#define A51_UNIFORM_SLOT(N) A51_JOIN_TOKENS(A51_UNIFORM_SLOT_, N)

#if defined(A51_SHADER_BINDING_SDL)

    #if defined(A51_SHADER_STAGE_VERTEX)
        #define A51_SDL_RESOURCE_SET 0
        #define A51_SDL_UNIFORM_SET  1
    #elif defined(A51_SHADER_STAGE_PIXEL)
        #define A51_SDL_RESOURCE_SET 2
        #define A51_SDL_UNIFORM_SET  3
    #elif defined(A51_SHADER_STAGE_COMPUTE)
        #define A51_SDL_RESOURCE_SET 0
        #define A51_SDL_UNIFORM_SET  2
    #else
        #error "SDL bindings require a vertex, pixel or compute shader stage"
    #endif

    #define A51_SAMPLED_TEXTURE_ATTR(NATIVE, SDL) \
        A51_VK_COMBINED_SAMPLER A51_VK_BINDING(A51_RESOURCE_SLOT(SDL), A51_SDL_RESOURCE_SET)
    #define A51_SAMPLED_TEXTURE_BIND(NATIVE, SDL) \
        : register(A51_REGISTER_T(A51_RESOURCE_SLOT(SDL)), A51_REGISTER_SPACE(A51_SDL_RESOURCE_SET))

    #define A51_SAMPLER_ATTR(NATIVE, SDL) \
        A51_VK_COMBINED_SAMPLER A51_VK_BINDING(A51_RESOURCE_SLOT(SDL), A51_SDL_RESOURCE_SET)
    #define A51_SAMPLER_BIND(NATIVE, SDL) \
        : register(A51_REGISTER_S(A51_RESOURCE_SLOT(SDL)), A51_REGISTER_SPACE(A51_SDL_RESOURCE_SET))

    #define A51_STORAGE_TEXTURE_ATTR(NATIVE, SDL) \
        A51_VK_BINDING(A51_RESOURCE_SLOT(SDL), A51_SDL_RESOURCE_SET)
    #if defined(__spirv__)
        #define A51_STORAGE_TEXTURE_BIND(NATIVE, SDL)
    #else
        #define A51_STORAGE_TEXTURE_BIND(NATIVE, SDL) \
            : register(A51_REGISTER_T(A51_RESOURCE_SLOT(SDL)), A51_REGISTER_SPACE(A51_SDL_RESOURCE_SET))
    #endif

    #define A51_STORAGE_BUFFER_ATTR(NATIVE, SDL) \
        A51_VK_BINDING(A51_RESOURCE_SLOT(SDL), A51_SDL_RESOURCE_SET)
    #define A51_STORAGE_BUFFER_BIND(NATIVE, SDL) \
        : register(A51_REGISTER_T(A51_RESOURCE_SLOT(SDL)), A51_REGISTER_SPACE(A51_SDL_RESOURCE_SET))

    #define A51_CBUFFER_ATTR(NATIVE, SDL) \
        A51_VK_BINDING(A51_UNIFORM_SLOT(SDL), A51_SDL_UNIFORM_SET)
    #define A51_CBUFFER_BIND(NATIVE, SDL) \
        : register(A51_REGISTER_B(A51_UNIFORM_SLOT(SDL)), A51_REGISTER_SPACE(A51_SDL_UNIFORM_SET))

#else

    #if defined(__spirv__)
        #define A51_SAMPLED_TEXTURE_ATTR(NATIVE, SDL) A51_VK_BINDING(NATIVE, 1)
        #define A51_SAMPLER_ATTR(NATIVE, SDL)         A51_VK_BINDING(NATIVE, 2)
        #define A51_STORAGE_TEXTURE_ATTR(NATIVE, SDL) A51_VK_BINDING(NATIVE, 3)
        #define A51_STORAGE_BUFFER_ATTR(NATIVE, SDL)  A51_VK_BINDING(NATIVE, 4)
        #define A51_CBUFFER_ATTR(NATIVE, SDL)          A51_VK_BINDING(NATIVE, 0)
    #else
        #define A51_SAMPLED_TEXTURE_ATTR(NATIVE, SDL)
        #define A51_SAMPLER_ATTR(NATIVE, SDL)
        #define A51_STORAGE_TEXTURE_ATTR(NATIVE, SDL)
        #define A51_STORAGE_BUFFER_ATTR(NATIVE, SDL)
        #define A51_CBUFFER_ATTR(NATIVE, SDL)
    #endif

    #define A51_SAMPLED_TEXTURE_BIND(NATIVE, SDL) : register(A51_REGISTER_T(NATIVE))
    #define A51_SAMPLER_BIND(NATIVE, SDL)         : register(A51_REGISTER_S(NATIVE))
    #if defined(__spirv__)
        #define A51_STORAGE_TEXTURE_BIND(NATIVE, SDL)
    #else
        #define A51_STORAGE_TEXTURE_BIND(NATIVE, SDL) : register(A51_REGISTER_T(NATIVE))
    #endif
    #define A51_STORAGE_BUFFER_BIND(NATIVE, SDL)  : register(A51_REGISTER_T(NATIVE))
    #define A51_CBUFFER_BIND(NATIVE, SDL)          : register(A51_REGISTER_B(NATIVE))

#endif

//==============================================================================
#endif // SHADER_BINDINGS_HLSL
//==============================================================================

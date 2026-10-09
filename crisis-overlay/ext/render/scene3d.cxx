#include <deps/includes.h>
#include "scene3d.hxx"
#include "mesh_loader.hxx"
#include <cmath>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>

#pragma comment( lib, "d3dcompiler.lib" )

namespace render {

	using namespace DirectX;

	// Constant buffer layout — matches cbuffer register(b0) in the shader.
	//
	// The world transform is SPLIT so the VS can inject a per-vertex
	// head-yaw rotation between the two halves:
	//   model_pre  = fit_matrix (raw mesh → normalised [-1..+1] centred)
	//   model_post = breath * rot (whole-body rotation from bypass +
	//                the breathing Y-scale from feet)
	// head_yaw is applied per-vertex in [-1..+1] space, weighted by
	// smoothstep(head_neck_y, head_top_y, y) so only the head area
	// actually rotates (no skinning rig — this is the closest we can
	// get to a "head only" rotation with a static mesh).
	//
	// tint is now consumed by the PS (used as the diffuse for
	// materials without a baseColorTexture so untextured props stay
	// visible in the caller's accent colour).
	struct scene_cb_t {
		XMFLOAT4X4 model_pre;         // fit_matrix
		XMFLOAT4X4 model_post;        // rot (breathing is now per-vertex in VS, not a matrix)
		XMFLOAT4X4 view_proj;
		XMFLOAT4   light_dir;
		XMFLOAT4   tint;              // used by PS for untextured materials + as the emissive tint
		XMFLOAT4   base_color;        // material baseColorFactor
		XMFLOAT4   emissive_factor;   // material emissiveFactor × KHR_materials_emissive_strength
		float      use_texture;       // 1 = sample diffuse_tex, 0 = tint only
		float      alpha_mode;        // 0=OPAQUE, 1=MASK, 2=BLEND (float for cbuffer align)
		float      alpha_cutoff;      // discard threshold when alpha_mode==MASK
		float      unlit;             // 1 = KHR_materials_unlit (no Lambert term)
		float      head_yaw;          // radians, world Y axis
		float      head_neck_y;       // post-fit Y where head-rotation blending starts
		float      head_top_y;        // post-fit Y where head-rotation blending is full
		float      has_emissive_tex;  // 1 = sample emissive_tex at t1, 0 = ignore
		float      breath_amount;     // world-units Y displacement to add to the chest at peak inhale
		float      chest_bottom_y;    // post-fit Y where per-vertex breath weight starts to ramp
		float      chest_top_y;       // post-fit Y where breath weight reaches 1.0
		float      _pad;              // 16-byte alignment
	};

	// HLSL — compiled at runtime via D3DCompile. Textured pass honours
	// glTF alphaMode/alphaCutoff/unlit:
	//   • alpha_mode==0 (OPAQUE): output alpha forced to 1, no discard
	//   • alpha_mode==1 (MASK): discard where sampled alpha < alpha_cutoff,
	//     output alpha forced to 1
	//   • alpha_mode==2 (BLEND): output premultiplied colour × alpha, no
	//     discard — composites correctly against everything drawn below
	// If unlit is set (KHR_materials_unlit), the Lambert term is skipped
	// so baked-lit face decals / logos keep their authored appearance.
	static const char* kSceneVs = R"(
cbuffer scene_cb : register(b0) {
    float4x4 model_pre;
    float4x4 model_post;
    float4x4 view_proj;
    float4   light_dir;
    float4   tint;
    float4   base_color;
    float4   emissive_factor;
    float    use_texture;
    float    alpha_mode;
    float    alpha_cutoff;
    float    unlit;
    float    head_yaw;
    float    head_neck_y;
    float    head_top_y;
    float    has_emissive_tex;
    float    breath_amount;
    float    chest_bottom_y;
    float    chest_top_y;
    float    _pad;
};
struct vs_in  { float3 pos : POSITION; float3 nrm : NORMAL; float2 uv : TEXCOORD0; };
struct vs_out {
    float4 pos     : SV_POSITION;
    float3 nrm     : NORMAL;
    float2 uv      : TEXCOORD0;
};
vs_out main( vs_in i ) {
    vs_out o;

    // Stage 1: fit — raw mesh coordinates → normalised [-1..+1] space
    // centred on origin. head_neck_y / head_top_y are in this space.
    float4 p_local = mul( float4( i.pos, 1.0f ), model_pre );
    float3 n_local = mul( float4( i.nrm, 0.0f ), model_pre ).xyz;

    // Stage 1b: per-vertex breathing — chest expansion only. Weighted
    // by smoothstep(chest_bottom_y, chest_top_y, y) so vertices below
    // the hip stay put (feet planted, legs still) and vertices at
    // upper-chest-and-above get the full breath_amount Y displacement.
    // In between the smoothstep fakes the compression a real diaphragm
    // would drive. This replaces the earlier Y-scale-from-feet matrix,
    // which scaled the WHOLE body (legs got taller with the breath —
    // wrong for chest breathing).
    float breath_w = smoothstep( chest_bottom_y, chest_top_y, p_local.y );
    p_local.y += breath_amount * breath_w;

    // (Arm droop removed 2026-08-25 — per-vertex rotation with a
    // smoothstep boundary is fundamentally lossy: triangles that
    // straddle the shoulder line have some vertices rotate fully and
    // some not at all, which stretches those triangles into visible
    // skin-coloured smears from the old fingertip position to the new
    // one. Proper arm posing needs skeletal skinning — parsing the
    // glTF `skins` block + JOINTS_0/WEIGHTS_0 vertex attributes and
    // baking the skin at load time with a modified shoulder rotation.
    // Deferred until the user explicitly asks; the character now
    // renders in its authored bind pose.)

    // Stage 2: per-vertex head-yaw. Weighted by post-fit Y so
    // shoulders/torso (below head_neck_y) don't rotate at all and the
    // head (above head_top_y) rotates by the full head_yaw. The
    // smoothstep in between fakes the flexing that a real neck bone
    // would drive — a mild elongation is visually preferable to a
    // hard cut across the neck. Rotation is about world +Y with the
    // pivot at (0, head_neck_y, 0).
    float w = smoothstep( head_neck_y, head_top_y, p_local.y );
    float angle = head_yaw * w;
    float c = cos( angle );
    float s = sin( angle );
    float3 pivot = float3( 0.0f, head_neck_y, 0.0f );
    float3 rel = p_local.xyz - pivot;
    float3 rot_pos = float3( c * rel.x + s * rel.z, rel.y, -s * rel.x + c * rel.z );
    p_local.xyz = rot_pos + pivot;
    // Rotate the normal by the same per-vertex angle so lighting on
    // the turned head still shades correctly (only the head-region
    // normals actually change because s/c are near 0/1 for w≈0).
    n_local = float3( c * n_local.x + s * n_local.z, n_local.y, -s * n_local.x + c * n_local.z );

    // Stage 3: breath + whole-body rot → world → projection.
    float4 world_pos = mul( p_local, model_post );
    o.pos = mul( world_pos, view_proj );
    o.nrm = normalize( mul( float4( n_local, 0.0f ), model_post ).xyz );
    o.uv  = i.uv;
    return o;
}
)";

	static const char* kScenePs = R"(
Texture2D    diffuse_tex  : register(t0);
Texture2D    emissive_tex : register(t1);
SamplerState diffuse_smp  : register(s0);
cbuffer scene_cb : register(b0) {
    float4x4 model_pre;
    float4x4 model_post;
    float4x4 view_proj;
    float4   light_dir;
    float4   tint;
    float4   base_color;
    float4   emissive_factor;
    float    use_texture;
    float    alpha_mode;
    float    alpha_cutoff;
    float    unlit;
    float    head_yaw;
    float    head_neck_y;
    float    head_top_y;
    float    has_emissive_tex;
    float    breath_amount;
    float    chest_bottom_y;
    float    chest_top_y;
    float    _pad;
};
struct vs_out {
    float4 pos     : SV_POSITION;
    float3 nrm     : NORMAL;
    float2 uv      : TEXCOORD0;
};

float4 main( vs_out i ) : SV_TARGET {
    float4 sampled = diffuse_tex.Sample( diffuse_smp, i.uv );

    // Textured path: use the sampled diffuse × base_color as before.
    // Untextured path: paint with the caller's tint DIRECTLY, ignoring
    // base_color_factor. Some .glb exports (the current gun ships 39
    // materials, 0 images) author dark or zero baseColorFactors when
    // they expected a texture — respecting those factors renders the
    // whole prop as flat black. Using pure tint is both a robust
    // debug/preview colour AND matches the procedural humanoid's
    // original "tint is authoritative when there's no texture" design.
    float4 textured   = sampled * base_color;
    float4 untextured = tint;
    float4 albedo     = lerp( untextured, textured, use_texture );

    // MASK mode: hard cutout per glTF spec.
    if ( alpha_mode > 0.5f && alpha_mode < 1.5f ) {
        if ( albedo.a < alpha_cutoff ) discard;
    }

    // Shading model: two directional lights (a strong "key" and a
    // softer "fill" from the opposite side) plus a Blinn-Phong-ish
    // specular highlight. That gives the model real form falloff and
    // a moving highlight — the two ingredients most missing when a
    // textured render "reads as 2D". Ambient is lower than before
    // (0.18 vs 0.32) so shadowed sides actually darken.
    float3 col;
    if ( unlit > 0.5f ) {
        col = albedo.rgb;   // KHR_materials_unlit — texture is authoritative
    } else {
        float3 n        = normalize( i.nrm );
        float3 key_dir  = normalize( light_dir.xyz );
        // Fill from the mirror-ish direction, slightly cooler weight.
        float3 fill_dir = normalize( float3( -key_dir.x * 0.6f, key_dir.y * 0.35f, -key_dir.z * 0.6f ) );

        // Half-Lambert wrap keeps the shadow side legible without
        // washing out the light side.
        float ndl_key  = saturate( dot( n, key_dir  ) * 0.5f + 0.5f );
        float ndl_fill = saturate( dot( n, fill_dir ) * 0.5f + 0.5f );

        float ambient   = 0.18f;
        float key_w     = 0.72f;
        float fill_w    = 0.22f;
        float shade     = ambient + ndl_key * key_w + ndl_fill * fill_w;

        // Blinn specular for the key light. Camera in scene3d looks
        // roughly along +Z from (0,0.9,-3.6), so view direction is
        // approximately (0,0,-1) in world space — good enough for a
        // preview without pushing the actual view vector through the
        // constant buffer. Exponent 48 gives a small crisp highlight;
        // 0.25 intensity keeps skin/plastic surfaces from looking wet.
        float3 view_dir = float3( 0.0f, 0.0f, -1.0f );
        float3 half_dir = normalize( key_dir - view_dir );
        float  spec     = pow( saturate( dot( n, half_dir ) ), 48.0f ) * 0.25f;

        col = albedo.rgb * shade + spec.xxx;
    }

    // Emissive tint — glTF materials that authored an emissive texture
    // or a non-zero emissiveFactor light up in the caller's tint (menu
    // accent colour) instead of the authored emissive colour. Reads as
    // eye glow / weapon glow / chest reactor picking up the current
    // theme accent — additive over the lit body colour, so it never
    // darkens what's underneath.
    //
    // glTF spec: final_emissive = emissiveFactor * sample(emissiveTex).
    // Multiply, NOT add — an earlier revision added them and made
    // every pixel bright on materials like Carbide where the factor
    // is (1,1,1) (intended as a "full-strength" multiplier of the
    // texture, not a base level), which tinted the ENTIRE body
    // instead of just the glow lines. With the correct multiply, the
    // texture's black pixels zero out the factor and only the bright
    // authored glow regions contribute.
    //
    // Intensity is `max(r,g,b)` of the emissive sample so any coloured
    // emissive channel activates the glow with the correct strength
    // (a red-emissive tex + accent colour = accent-coloured glow at
    // red's brightness).
    {
        float3 emiss;
        if ( has_emissive_tex > 0.5f )
            emiss = emissive_tex.Sample( diffuse_smp, i.uv ).rgb * emissive_factor.rgb;
        else
            emiss = emissive_factor.rgb;
        float emiss_intensity = max( max( emiss.r, emiss.g ), emiss.b );
        // 2x boost so the glow reads as glow rather than a subtle wash
        // over the diffuse; saturate caps at 1.0 so the base body
        // colour is never fully overwritten even on the hottest pixel.
        col += tint.rgb * saturate( emiss_intensity * 2.0f );
    }

    // BLEND: premultiplied-alpha output for translucent overlays.
    if ( alpha_mode > 1.5f ) {
        float a = albedo.a;
        return float4( col * a, a );
    }

    // OPAQUE / MASK: fully opaque output regardless of texture alpha.
    return float4( col, 1.0f );
}
)";

	// ── Procedural humanoid — same shape as v1 but now with placeholder
	// UVs (0,0) so it can flow through the textured shader path. Rendered
	// with use_texture=0 so the 1×1 white SRV sampler call is skipped in
	// the base-colour lerp — the model just picks up the caller's tint.
	static void push_box(
		std::vector< mesh_vertex_t >& verts,
		std::vector< std::uint16_t >& indices,
		float cx, float cy, float cz,
		float hx, float hy, float hz )
	{
		struct face_t { float nx, ny, nz; int corners[ 4 ][ 3 ]; };
		static const face_t faces[ 6 ] = {
			{  0,  0,  1, { { -1,-1, 1 }, {  1,-1, 1 }, {  1, 1, 1 }, { -1, 1, 1 } } },
			{  0,  0, -1, { {  1,-1,-1 }, { -1,-1,-1 }, { -1, 1,-1 }, {  1, 1,-1 } } },
			{  1,  0,  0, { {  1,-1, 1 }, {  1,-1,-1 }, {  1, 1,-1 }, {  1, 1, 1 } } },
			{ -1,  0,  0, { { -1,-1,-1 }, { -1,-1, 1 }, { -1, 1, 1 }, { -1, 1,-1 } } },
			{  0,  1,  0, { { -1, 1, 1 }, {  1, 1, 1 }, {  1, 1,-1 }, { -1, 1,-1 } } },
			{  0, -1,  0, { { -1,-1,-1 }, {  1,-1,-1 }, {  1,-1, 1 }, { -1,-1, 1 } } },
		};
		for ( const auto& f : faces ) {
			const auto base = static_cast< std::uint16_t >( verts.size( ) );
			for ( int k = 0; k < 4; ++k ) {
				mesh_vertex_t v {};
				v.px = cx + f.corners[ k ][ 0 ] * hx;
				v.py = cy + f.corners[ k ][ 1 ] * hy;
				v.pz = cz + f.corners[ k ][ 2 ] * hz;
				v.nx = f.nx; v.ny = f.ny; v.nz = f.nz;
				v.u = 0.0f; v.v = 0.0f;
				verts.push_back( v );
			}
			indices.push_back( base + 0 );
			indices.push_back( base + 1 );
			indices.push_back( base + 2 );
			indices.push_back( base + 0 );
			indices.push_back( base + 2 );
			indices.push_back( base + 3 );
		}
	}

	bool c_scene3d::initialize( c_device& device ) {
		if ( m_device ) return true;
		if ( !device.d3d_device( ) || !device.d3d_context( ) || !device.context( ) )
			return false;

		m_device = &device;

		// 1024×2048 — deliberately 2× taller than wide so the vertical
		// framing has room for the whole model even when the on-screen
		// display rect is enlarged for a "bigger" preview. The
		// projection matrix uses (m_width / m_height) as aspect so it
		// automatically adapts to the taller target. The menu draws
		// this bitmap into a matching 1:2-aspect rect (no stretching);
		// the character then reads 2× the on-screen height it did when
		// the target was square, WITHOUT bringing the camera closer
		// and WITHOUT clipping head/feet.
		//
		// Memory footprint: 1024×2048×4 × (4 MSAA colour + 4 MSAA depth
		// + 1 resolve) ≈ 80 MB per instance — still trivial.
		constexpr int kTargetW = 1024;
		constexpr int kTargetH = 2048;

		if ( !build_target( kTargetW, kTargetH ) )    { shutdown( ); return false; }
		if ( !build_shaders( ) )                       { shutdown( ); return false; }
		if ( !build_procedural_mesh( ) )               { shutdown( ); return false; }

		auto* d3d = device.d3d_device( );

		D3D11_RASTERIZER_DESC rd {};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_BACK;
		// glTF winds triangles CCW when viewed from outside; my push_box
		// convention matches. Set front=CCW so both mesh sources render
		// without swapping index order per source.
		rd.FrontCounterClockwise = TRUE;
		rd.DepthClipEnable = TRUE;
		// Enable MSAA rasterization — the RTV is 4×MSAA, so the
		// rasterizer needs this flag or triangles get single-sample
		// rasterization and defeat the whole point of the MS target.
		rd.MultisampleEnable = TRUE;
		rd.AntialiasedLineEnable = TRUE;
		if ( FAILED( d3d->CreateRasterizerState( &rd, m_raster.GetAddressOf( ) ) ) )
			{ shutdown( ); return false; }

		// No-cull variant — used per-primitive when the glTF material
		// has doubleSided=true. Same fill/depth-clip settings as the
		// default rasterizer; only CullMode changes so pipeline swaps
		// stay cheap.
		D3D11_RASTERIZER_DESC rd_double = rd;
		rd_double.CullMode = D3D11_CULL_NONE;
		if ( FAILED( d3d->CreateRasterizerState( &rd_double, m_raster_double.GetAddressOf( ) ) ) )
			{ shutdown( ); return false; }

		D3D11_DEPTH_STENCIL_DESC dsd {};
		dsd.DepthEnable    = TRUE;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsd.DepthFunc      = D3D11_COMPARISON_LESS;
		if ( FAILED( d3d->CreateDepthStencilState( &dsd, m_ds_state.GetAddressOf( ) ) ) )
			{ shutdown( ); return false; }

		D3D11_BLEND_DESC bd {};
		bd.RenderTarget[ 0 ].BlendEnable    = TRUE;
		bd.RenderTarget[ 0 ].SrcBlend       = D3D11_BLEND_ONE;
		bd.RenderTarget[ 0 ].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[ 0 ].BlendOp        = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[ 0 ].SrcBlendAlpha  = D3D11_BLEND_ONE;
		bd.RenderTarget[ 0 ].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[ 0 ].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		if ( FAILED( d3d->CreateBlendState( &bd, m_blend.GetAddressOf( ) ) ) )
			{ shutdown( ); return false; }

		D3D11_SAMPLER_DESC sd {};
		// Anisotropic filtering keeps textures sharp at grazing angles
		// (helpful for characters where a lot of surface area is not
		// facing the camera head-on). 16× is the max D3D11 supports;
		// modern GPUs run it essentially free.
		sd.Filter        = D3D11_FILTER_ANISOTROPIC;
		sd.MaxAnisotropy = 16;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.MaxLOD   = D3D11_FLOAT32_MAX;
		if ( FAILED( d3d->CreateSamplerState( &sd, m_sampler.GetAddressOf( ) ) ) )
			{ shutdown( ); return false; }

		// 1×1 white SRV — used as the diffuse binding for primitives whose
		// materials don't have a baseColorTexture. The shader's use_texture
		// flag skips it in the lerp so the actual sample value doesn't
		// matter, but D3D still complains if the slot is null in some
		// debug layers.
		{
			const std::uint32_t white_px = 0xFFFFFFFFu;
			D3D11_TEXTURE2D_DESC td {};
			td.Width = td.Height = 1;
			td.MipLevels = td.ArraySize = 1;
			td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_IMMUTABLE;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA init { &white_px, 4, 0 };
			Microsoft::WRL::ComPtr< ID3D11Texture2D > tex;
			if ( FAILED( d3d->CreateTexture2D( &td, &init, tex.GetAddressOf( ) ) ) )
				{ shutdown( ); return false; }
			if ( FAILED( d3d->CreateShaderResourceView( tex.Get( ), nullptr, m_white_srv.GetAddressOf( ) ) ) )
				{ shutdown( ); return false; }
		}

		// 1×1 black SRV for the emissive slot (t1) when a material has
		// no emissiveTexture. Sampled colour is (0,0,0) so the tinted
		// glow term collapses to just emissive_factor.
		{
			const std::uint32_t black_px = 0xFF000000u;   // BGRA: opaque black
			D3D11_TEXTURE2D_DESC td {};
			td.Width = td.Height = 1;
			td.MipLevels = td.ArraySize = 1;
			td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_IMMUTABLE;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA init { &black_px, 4, 0 };
			Microsoft::WRL::ComPtr< ID3D11Texture2D > tex;
			if ( FAILED( d3d->CreateTexture2D( &td, &init, tex.GetAddressOf( ) ) ) )
				{ shutdown( ); return false; }
			if ( FAILED( d3d->CreateShaderResourceView( tex.Get( ), nullptr, m_black_srv.GetAddressOf( ) ) ) )
				{ shutdown( ); return false; }
		}

		// Start breathing at phase 0 (fully exhaled) on the first frame
		// after init / device rebuild. Set here rather than in a member
		// default-initialiser so it plays right after every reinit too.
		m_start_time = std::chrono::steady_clock::now( );

		// Head-turn state — begin centred, seed the PRNG from the
		// start-time low bits (steady_clock is monotonic; the low
		// nanoseconds bits vary across runs enough for a nice random
		// walk) and open with a first HOLD phase so the head doesn't
		// snap into a turn on frame 0.
		m_head_current_angle = 0.0f;
		m_head_target_angle  = 0.0f;
		m_head_phase_start_s = 0.0f;
		m_head_phase_dur_s   = 1.5f;   // brief initial settle
		m_head_phase_is_turn = false;
		{
			const auto ns = m_start_time.time_since_epoch( ).count( );
			m_head_rng_state = static_cast< std::uint32_t >( ns ) ^ 0x9E3779B9u;
			if ( m_head_rng_state == 0u ) m_head_rng_state = 0xDEADBEEFu;   // xorshift breaks on 0
		}


		return true;
	}

	void c_scene3d::shutdown( ) {
		m_mesh = loaded_mesh_t {};
		m_black_srv.Reset( );
		m_white_srv.Reset( );
		m_sampler.Reset( );
		m_blend.Reset( );
		m_ds_state.Reset( );
		m_raster_double.Reset( );
		m_raster.Reset( );
		m_cb.Reset( );
		m_input_layout.Reset( );
		m_ps.Reset( );
		m_vs.Reset( );
		m_d2d_target.Reset( );
		m_dsv.Reset( );
		m_depth.Reset( );
		m_rtv_ms.Reset( );
		m_target_ms.Reset( );
		m_target.Reset( );
		m_width = m_height = 0;
		m_device = nullptr;
	}

	void c_scene3d::set_mesh( loaded_mesh_t&& mesh ) {
		m_mesh = std::move( mesh );
	}

	bool c_scene3d::build_target( int width, int height ) {
		if ( !m_device ) return false;
		auto* d3d = m_device->d3d_device( );
		auto* d2d = m_device->context( );
		if ( !d3d || !d2d ) return false;

		m_width  = width;
		m_height = height;

		// MSAA quality picking — 4x is universally supported on every
		// D3D11 GPU. Query max quality level for BGRA at 4x samples so
		// we set SampleDesc.Quality correctly (usually 0 for GPU-native
		// 4x, higher for driver-emulated variants).
		constexpr UINT kSampleCount = 4;
		UINT ms_quality = 0;
		if ( FAILED( d3d->CheckMultisampleQualityLevels( DXGI_FORMAT_B8G8R8A8_UNORM,
				kSampleCount, &ms_quality ) ) || ms_quality == 0 )
			ms_quality = 1;

		// MSAA colour target — this is what the 3D pass writes to.
		D3D11_TEXTURE2D_DESC td_ms {};
		td_ms.Width  = static_cast< UINT >( width );
		td_ms.Height = static_cast< UINT >( height );
		td_ms.MipLevels        = 1;
		td_ms.ArraySize        = 1;
		td_ms.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
		td_ms.SampleDesc.Count   = kSampleCount;
		td_ms.SampleDesc.Quality = ms_quality - 1;
		td_ms.Usage            = D3D11_USAGE_DEFAULT;
		td_ms.BindFlags        = D3D11_BIND_RENDER_TARGET;
		if ( FAILED( d3d->CreateTexture2D( &td_ms, nullptr, m_target_ms.GetAddressOf( ) ) ) )
			return false;
		if ( FAILED( d3d->CreateRenderTargetView( m_target_ms.Get( ), nullptr, m_rtv_ms.GetAddressOf( ) ) ) )
			return false;

		// Resolve destination — single-sample copy of the MSAA target,
		// wrapped as the D2D bitmap. ResolveSubresource downsamples the
		// 4 MSAA samples to one at end-of-frame.
		D3D11_TEXTURE2D_DESC td {};
		td.Width  = static_cast< UINT >( width );
		td.Height = static_cast< UINT >( height );
		td.MipLevels        = 1;
		td.ArraySize        = 1;
		td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage            = D3D11_USAGE_DEFAULT;
		td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		if ( FAILED( d3d->CreateTexture2D( &td, nullptr, m_target.GetAddressOf( ) ) ) )
			return false;

		// MSAA-matched depth buffer. If sample counts don't match the
		// colour target D3D11 rejects the OMSetRenderTargets call, so
		// SampleDesc has to mirror the colour side.
		D3D11_TEXTURE2D_DESC dd {};
		dd.Width  = static_cast< UINT >( width );
		dd.Height = static_cast< UINT >( height );
		dd.MipLevels        = 1;
		dd.ArraySize        = 1;
		dd.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dd.SampleDesc.Count   = kSampleCount;
		dd.SampleDesc.Quality = ms_quality - 1;
		dd.Usage            = D3D11_USAGE_DEFAULT;
		dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL;
		if ( FAILED( d3d->CreateTexture2D( &dd, nullptr, m_depth.GetAddressOf( ) ) ) )
			return false;
		if ( FAILED( d3d->CreateDepthStencilView( m_depth.Get( ), nullptr, m_dsv.GetAddressOf( ) ) ) )
			return false;

		Microsoft::WRL::ComPtr< IDXGISurface > surface;
		if ( FAILED( m_target.As( &surface ) ) )
			return false;
		auto props = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_NONE,
			D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED )
		);
		if ( FAILED( d2d->CreateBitmapFromDxgiSurface( surface.Get( ), &props, m_d2d_target.GetAddressOf( ) ) ) )
			return false;

		return true;
	}

	bool c_scene3d::build_shaders( ) {
		if ( !m_device ) return false;
		auto* d3d = m_device->d3d_device( );
		if ( !d3d ) return false;

		Microsoft::WRL::ComPtr< ID3DBlob > vs_blob, ps_blob, err;
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

		if ( FAILED( D3DCompile( kSceneVs, std::strlen( kSceneVs ), "scene_vs",
				nullptr, nullptr, "main", "vs_4_0", flags, 0,
				vs_blob.GetAddressOf( ), err.GetAddressOf( ) ) ) )
			return false;
		err.Reset( );

		if ( FAILED( D3DCompile( kScenePs, std::strlen( kScenePs ), "scene_ps",
				nullptr, nullptr, "main", "ps_4_0", flags, 0,
				ps_blob.GetAddressOf( ), err.GetAddressOf( ) ) ) )
			return false;
		err.Reset( );

		if ( FAILED( d3d->CreateVertexShader( vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ),
				nullptr, m_vs.GetAddressOf( ) ) ) )
			return false;
		if ( FAILED( d3d->CreatePixelShader( ps_blob->GetBufferPointer( ), ps_blob->GetBufferSize( ),
				nullptr, m_ps.GetAddressOf( ) ) ) )
			return false;

		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if ( FAILED( d3d->CreateInputLayout( layout, 3,
				vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ),
				m_input_layout.GetAddressOf( ) ) ) )
			return false;

		D3D11_BUFFER_DESC cbd {};
		cbd.ByteWidth      = ( sizeof( scene_cb_t ) + 15 ) & ~15u;
		cbd.Usage          = D3D11_USAGE_DYNAMIC;
		cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if ( FAILED( d3d->CreateBuffer( &cbd, nullptr, m_cb.GetAddressOf( ) ) ) )
			return false;

		return true;
	}

	bool c_scene3d::build_procedural_mesh( ) {
		if ( !m_device ) return false;
		auto* d3d = m_device->d3d_device( );
		if ( !d3d ) return false;

		std::vector< mesh_vertex_t > verts;
		std::vector< std::uint16_t > indices;

		// Blocky humanoid — Fortnite-ish proportions, feet on ground.
		push_box( verts, indices,  0.00f,  1.70f, 0.00f,  0.22f, 0.22f, 0.22f );
		push_box( verts, indices,  0.00f,  1.10f, 0.00f,  0.32f, 0.36f, 0.18f );
		push_box( verts, indices,  0.00f,  0.70f, 0.00f,  0.30f, 0.10f, 0.18f );
		push_box( verts, indices, -0.42f,  1.15f, 0.00f,  0.10f, 0.30f, 0.12f );
		push_box( verts, indices,  0.42f,  1.15f, 0.00f,  0.10f, 0.30f, 0.12f );
		push_box( verts, indices, -0.42f,  0.63f, 0.00f,  0.09f, 0.24f, 0.11f );
		push_box( verts, indices,  0.42f,  0.63f, 0.00f,  0.09f, 0.24f, 0.11f );
		push_box( verts, indices, -0.16f,  0.42f, 0.00f,  0.12f, 0.24f, 0.14f );
		push_box( verts, indices,  0.16f,  0.42f, 0.00f,  0.12f, 0.24f, 0.14f );
		push_box( verts, indices, -0.16f, -0.05f, 0.00f,  0.11f, 0.24f, 0.13f );
		push_box( verts, indices,  0.16f, -0.05f, 0.00f,  0.11f, 0.24f, 0.13f );

		loaded_primitive_t lp {};
		lp.index_count   = static_cast< std::uint32_t >( indices.size( ) );
		lp.vertex_stride = static_cast< std::uint32_t >( sizeof( mesh_vertex_t ) );
		lp.index_format  = DXGI_FORMAT_R16_UINT;
		lp.material      = -1;

		D3D11_BUFFER_DESC vbd {};
		vbd.ByteWidth = static_cast< UINT >( verts.size( ) * sizeof( mesh_vertex_t ) );
		vbd.Usage     = D3D11_USAGE_IMMUTABLE;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA vinit { verts.data( ), 0, 0 };
		if ( FAILED( d3d->CreateBuffer( &vbd, &vinit, lp.vb.GetAddressOf( ) ) ) )
			return false;

		D3D11_BUFFER_DESC ibd {};
		ibd.ByteWidth = static_cast< UINT >( indices.size( ) * sizeof( std::uint16_t ) );
		ibd.Usage     = D3D11_USAGE_IMMUTABLE;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA iinit { indices.data( ), 0, 0 };
		if ( FAILED( d3d->CreateBuffer( &ibd, &iinit, lp.ib.GetAddressOf( ) ) ) )
			return false;

		loaded_mesh_t built {};
		built.primitives.push_back( std::move( lp ) );
		// No materials for the procedural mesh — the render loop passes -1
		// material and use_texture=0 so the tint carries the whole colour.
		//
		// Fit matrix — the box positions above put feet ≈ y=-0.29 and head
		// top ≈ y=+1.92 (centre ≈ y=+0.815, total height ≈ 2.21). The
		// camera aims at the origin, so we translate the centre to y=0
		// and scale to a 2-unit height so the procedural fallback occupies
		// the same frustum region as a properly-fit .glb.
		constexpr float proc_center_y = 0.815f;
		constexpr float proc_height   = 2.21f;
		constexpr float proc_scale    = 2.0f / proc_height;   // → ≈ 0.905
		XMStoreFloat4x4( &built.fit_matrix,
			XMMatrixTranslation( 0.0f, -proc_center_y, 0.0f ) *
			XMMatrixScaling( proc_scale, proc_scale, proc_scale ) );
		m_mesh = std::move( built );
		return true;
	}

	bool c_scene3d::render_frame( float rot_x, float rot_y, float rot_z, color_t tint ) {
		if ( !m_device || !m_rtv_ms || !m_dsv || !m_vs || !m_ps || !m_input_layout ) return false;
		if ( m_mesh.empty( ) ) return false;
		auto* d3d = m_device->d3d_device( );
		auto* dc  = m_device->d3d_context( );
		if ( !d3d || !dc ) return false;

		const float clear_rgba[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
		// 3D pass writes into the MSAA render target; we resolve to the
		// single-sample m_target at the end so the D2D bitmap sees the
		// anti-aliased result.
		dc->OMSetRenderTargets( 1, m_rtv_ms.GetAddressOf( ), m_dsv.Get( ) );
		dc->ClearRenderTargetView( m_rtv_ms.Get( ), clear_rgba );
		dc->ClearDepthStencilView( m_dsv.Get( ), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

		D3D11_VIEWPORT vp {};
		vp.Width    = static_cast< FLOAT >( m_width );
		vp.Height   = static_cast< FLOAT >( m_height );
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		dc->RSSetViewports( 1, &vp );

		const XMMATRIX rot = XMMatrixRotationRollPitchYaw( rot_x, rot_y, rot_z );

		// Single "now" for both breathing and head-turn so their phases
		// line up predictably (helps a lot when a rebuild moves a
		// step-boundary — both animations restart together).
		const auto  now       = std::chrono::steady_clock::now( );
		const float elapsed_s = std::chrono::duration< float >( now - m_start_time ).count( );

		// Breathing — now a per-vertex Y displacement applied in the
		// VS, weighted by smoothstep(chest_bottom_y, chest_top_y, y)
		// so feet + legs stay planted and only the upper body rises.
		// The old feet-pivot Y-scale scaled the WHOLE model uniformly
		// (legs stretched with the breath) — wrong for chest breath.
		// Compute just the amplitude here; the VS does the shaping.
		//
		// Tunables: cycle_s ≈ resting breathing rate; breath_amp is
		// the world-Y displacement (in fitted [-1..+1] space) that the
		// chest+head get at peak inhale — 0.022 ≈ 1.1 % of body height,
		// visible but not exaggerated.
		float breath_amount = 0.0f;
		{
			constexpr float cycle_s    = 3.5f;
			constexpr float breath_amp = 0.022f;
			const float phase = elapsed_s * ( XM_2PI / cycle_s );
			const float wave  = ( std::sin( phase ) + 1.0f ) * 0.5f;   // 0 (exhale) .. 1 (inhale)
			breath_amount = wave * breath_amp;
		}

		// Head turn — alternating HOLD / TURN phases with random targets.
		//
		//   HOLD  — head sits at current_angle for hold_dur_s (2..5 s)
		//   TURN  — smoothstep-eased interpolation from current_angle
		//           to a new target over turn_dur_s (0.6..1.0 s)
		//
		// Target distribution favours returning to centre so the model
		// spends most of its time roughly facing the viewer, with
		// occasional glances left/right of up to ±20°. This produces
		// the "idle character occasionally looking around" feel the
		// user asked for without ever cranking the head so far it looks
		// broken (a real neck can't yaw more than ~70° without shoulder
		// help, and we'd expose the lack of neck-skinning at anything
		// past ~25°).
		//
		// xorshift32 for the RNG — no dependency on std::rand or
		// std::random_device, both of which are effectively broken in
		// a manually-mapped DLL context (rand's seed lives in the
		// per-instance CRT that we don't fully wire up, and
		// random_device pulls in more machinery than we want here).
		auto xorshift = [ this ] ( ) -> std::uint32_t {
			std::uint32_t x = m_head_rng_state;
			x ^= x << 13;
			x ^= x >> 17;
			x ^= x <<  5;
			m_head_rng_state = x;
			return x;
		};
		auto rand01 = [ & ] ( ) -> float {
			return static_cast< float >( xorshift( ) & 0x00FFFFFFu ) / static_cast< float >( 0x01000000u );
		};

		if ( elapsed_s - m_head_phase_start_s >= m_head_phase_dur_s ) {
			m_head_phase_start_s = elapsed_s;
			if ( m_head_phase_is_turn ) {
				// Finished a turn → snap current to target and hold.
				m_head_current_angle = m_head_target_angle;
				m_head_phase_is_turn = false;
				m_head_phase_dur_s   = 2.5f + rand01( ) * 3.5f;   // hold 2.5..6 s
			}
			else {
				// Finished holding → pick a new target and start turn.
				// 40 % centre, 30 % left, 30 % right; magnitude 8..22°.
				const float r   = rand01( );
				const float mag = XMConvertToRadians( 8.0f + rand01( ) * 14.0f );
				if      ( r < 0.40f ) m_head_target_angle = 0.0f;
				else if ( r < 0.70f ) m_head_target_angle = -mag;
				else                  m_head_target_angle = +mag;
				m_head_phase_is_turn = true;
				m_head_phase_dur_s   = 0.95f + rand01( ) * 0.55f;   // turn 0.95..1.5 s
			}
		}

		// Head-yaw compositing:
		//   base   — the eased interpolation between start & target
		//            during a TURN, or the held target during a HOLD.
		//   jitter — a slow tri-frequency sine wave (~0.3°) added on
		//            top so a "still" head still micro-shifts the way
		//            a real person's does. The frequencies are
		//            incommensurate so the sum never repeats visibly.
		float head_yaw = m_head_current_angle;
		if ( m_head_phase_is_turn ) {
			float t = ( elapsed_s - m_head_phase_start_s ) / m_head_phase_dur_s;
			t = t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );
			// Ease-out cubic: fast start, slow settle — reads more
			// like a real head coming to rest than smoothstep, which
			// eases symmetrically into and out of the motion.
			const float ease = 1.0f - ( 1.0f - t ) * ( 1.0f - t ) * ( 1.0f - t );
			head_yaw = m_head_current_angle + ( m_head_target_angle - m_head_current_angle ) * ease;
		}
		{
			constexpr float jitter_amp_deg = 0.35f;
			const float j =
				std::sin( elapsed_s * 0.83f ) * 0.55f +
				std::sin( elapsed_s * 1.71f ) * 0.30f +
				std::sin( elapsed_s * 2.39f ) * 0.15f;
			head_yaw += j * XMConvertToRadians( jitter_amp_deg );
		}

		// Camera framing — mesh loader normalises max-dim to 2 units
		// centred on origin, so the primary model spans y=-1..+1.
		//
		// Distance 4.15 · aim origin · FOV 34° → visible half-height
		// ≈ 1.27. Model at ±1 fills ~79 % of the frame — another 7 %
		// smaller than the 3.85 pass so a small header can sit on top
		// of the box AND the text stack has more breathing room above
		// / below the character within the card boundary.
		const XMMATRIX view      = XMMatrixLookAtLH(
			XMVectorSet( 0.0f, 0.0f, -4.15f, 0.0f ),
			XMVectorSet( 0.0f, 0.0f,  0.0f, 0.0f ),
			XMVectorSet( 0.0f, 1.0f,  0.0f, 0.0f ) );
		const float aspect = static_cast< float >( m_width ) / static_cast< float >( m_height );
		const XMMATRIX proj      = XMMatrixPerspectiveFovLH( XMConvertToRadians( 34.0f ), aspect, 0.1f, 50.0f );
		const XMMATRIX view_proj = view * proj;

		// Pipeline binds that don't change per primitive.
		dc->IASetInputLayout( m_input_layout.Get( ) );
		dc->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		dc->VSSetShader( m_vs.Get( ), nullptr, 0 );
		dc->PSSetShader( m_ps.Get( ), nullptr, 0 );
		dc->VSSetConstantBuffers( 0, 1, m_cb.GetAddressOf( ) );
		dc->PSSetConstantBuffers( 0, 1, m_cb.GetAddressOf( ) );
		dc->PSSetSamplers( 0, 1, m_sampler.GetAddressOf( ) );
		dc->OMSetDepthStencilState( m_ds_state.Get( ), 0 );
		const float blend_factor[ 4 ] = { 0, 0, 0, 0 };
		dc->OMSetBlendState( m_blend.Get( ), blend_factor, 0xFFFFFFFF );

		// Single textured pass — per-primitive material state selects
		// alpha discard / blend behaviour + backface cull vs no-cull.
		// Rasterizer is set inside the loop so doubleSided materials
		// don't leak their no-cull state to the next primitive.
		//
		// Split world matrix: model_pre puts the raw mesh into [-1..+1]
		// space centred on origin; the VS then applies per-vertex
		// breathing displacement + head yaw in that space; model_post
		// applies whole-body rot from the caller.
		const XMMATRIX fit  = XMLoadFloat4x4( &m_mesh.fit_matrix );
		const XMMATRIX post = rot;

		// Head-turn Y band, in post-fit ([-1..+1]) space. Fortnite-style
		// characters have shoulders at roughly y=+0.55 and head crown at
		// y ≈ +0.95, so blending from 0.55 → 0.95 keeps the shoulders
		// locked and lets the head yaw the full amount. If a future
		// character puts these landmarks somewhere else, tweak here.
		constexpr float kHeadNeckY = 0.55f;
		constexpr float kHeadTopY  = 0.95f;

		// Per-vertex breathing zone (post-fit space). Below chest_bottom
		// nothing moves; above chest_top the full breath_amount is
		// applied; in between smoothstep blends. For a character fitted
		// to y=-1..+1 with feet at -1 and head at +1, hip ≈ 0, chest
		// ≈ +0.35, upper-chest ≈ +0.55 — these thresholds keep the
		// legs planted and localise the motion to torso-and-above.
		constexpr float kChestBottomY = 0.10f;
		constexpr float kChestTopY    = 0.50f;

		ID3D11RasterizerState* last_raster = nullptr;
		for ( const auto& prim : m_mesh.primitives ) {
			if ( !prim.vb || !prim.ib ) continue;
			const loaded_material_t* mat = ( prim.material >= 0 &&
				prim.material < static_cast< int >( m_mesh.materials.size( ) ) )
					? &m_mesh.materials[ prim.material ] : nullptr;

			// Pick the rasterizer for this primitive. Swapping is cheap
			// but we still avoid redundant sets for the common case
			// (most primitives share a single-sided material).
			ID3D11RasterizerState* want_raster = ( mat && mat->double_sided )
				? m_raster_double.Get( ) : m_raster.Get( );
			if ( want_raster != last_raster ) {
				dc->RSSetState( want_raster );
				last_raster = want_raster;
			}

			D3D11_MAPPED_SUBRESOURCE mapped {};
			if ( FAILED( dc->Map( m_cb.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
				continue;
			{
				auto* cb = static_cast< scene_cb_t* >( mapped.pData );
				XMStoreFloat4x4( &cb->model_pre,  XMMatrixTranspose( fit       ) );
				XMStoreFloat4x4( &cb->model_post, XMMatrixTranspose( post      ) );
				XMStoreFloat4x4( &cb->view_proj,  XMMatrixTranspose( view_proj ) );
				cb->light_dir  = XMFLOAT4( 0.45f, 0.75f, -0.55f, 0.0f );
				cb->tint       = XMFLOAT4( tint.r, tint.g, tint.b, tint.a );
				cb->base_color = mat ? mat->base_color_factor : XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f );
				cb->use_texture  = ( mat && mat->has_diffuse ) ? 1.0f : 0.0f;
				cb->alpha_mode   = mat ? static_cast< float >( mat->alpha_mode ) : 0.0f;
				cb->alpha_cutoff = mat ? mat->alpha_cutoff : 0.5f;
				cb->unlit        = ( mat && mat->unlit ) ? 1.0f : 0.0f;
				cb->emissive_factor  = mat ? mat->emissive_factor : XMFLOAT4( 0.0f, 0.0f, 0.0f, 0.0f );
				cb->head_yaw         = head_yaw;
				cb->head_neck_y      = kHeadNeckY;
				cb->head_top_y       = kHeadTopY;
				cb->has_emissive_tex = ( mat && mat->has_emissive_tex ) ? 1.0f : 0.0f;
				cb->breath_amount    = breath_amount;
				cb->chest_bottom_y   = kChestBottomY;
				cb->chest_top_y      = kChestTopY;
				cb->_pad             = 0.0f;
			}
			dc->Unmap( m_cb.Get( ), 0 );

			ID3D11ShaderResourceView* srv_diff = ( mat && mat->has_diffuse && mat->diffuse_srv )
				? mat->diffuse_srv.Get( )
				: m_white_srv.Get( );
			ID3D11ShaderResourceView* srv_emiss = ( mat && mat->has_emissive_tex && mat->emissive_srv )
				? mat->emissive_srv.Get( )
				: m_black_srv.Get( );
			ID3D11ShaderResourceView* srvs[ 2 ] = { srv_diff, srv_emiss };
			dc->PSSetShaderResources( 0, 2, srvs );

			UINT stride = prim.vertex_stride ? prim.vertex_stride : sizeof( mesh_vertex_t );
			UINT offset = 0;
			ID3D11Buffer* vb = prim.vb.Get( );
			dc->IASetVertexBuffers( 0, 1, &vb, &stride, &offset );
			dc->IASetIndexBuffer( prim.ib.Get( ), prim.index_format, 0 );

			dc->DrawIndexed( prim.index_count, 0, 0 );
		}

		// Unbind so the D2D pass can freely touch the RTV as an ID2D1Bitmap.
		ID3D11RenderTargetView* null_rtv = nullptr;
		dc->OMSetRenderTargets( 1, &null_rtv, nullptr );
		ID3D11ShaderResourceView* null_srvs[ 2 ] = { nullptr, nullptr };
		dc->PSSetShaderResources( 0, 2, null_srvs );

		// Resolve MSAA colour samples down to the single-sample
		// m_target — that's the texture the D2D bitmap wraps, so
		// DrawBitmap picks up the anti-aliased result rather than
		// undefined data from the MSAA surface directly.
		dc->ResolveSubresource(
			m_target.Get( ),    0,
			m_target_ms.Get( ), 0,
			DXGI_FORMAT_B8G8R8A8_UNORM );
		return true;
	}
}

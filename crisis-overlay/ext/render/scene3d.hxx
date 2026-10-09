#pragma once

#include "device.hxx"
#include "mesh_loader.hxx"
#include <chrono>
#include <cstdint>
#include <d2d1_1.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

namespace render {
	// ── Off-screen 3D scene renderer ─────────────────────────────────────────
	//
	// Two mesh sources:
	//   1. Built-in procedural humanoid (11 boxes) — created on init,
	//      used until set_mesh() swaps in a real asset.
	//   2. loaded_mesh_t from mesh_loader::load_glb() — Fortnite / any
	//      glTF character with baseColorTexture materials. Multi-primitive
	//      draw, one DrawIndexed per material.
	//
	// The scene renders into a fixed 512×512 BGRA texture that's exposed
	// as an ID2D1Bitmap for the D2D composite pass — same interop trick as
	// v1, unchanged.
	// ─────────────────────────────────────────────────────────────────────────

	class c_scene3d {
	public:
		c_scene3d( )  = default;
		~c_scene3d( ) { shutdown( ); }

		c_scene3d( const c_scene3d& ) = delete;
		c_scene3d& operator=( const c_scene3d& ) = delete;

		bool initialize( c_device& device );
		void shutdown( );

		// Swap the drawn mesh out for a loaded one. Ownership transferred.
		// After this call the built-in procedural humanoid is dropped and
		// every render_frame() draws `mesh` instead.
		void set_mesh( loaded_mesh_t&& mesh );

		// Render the current mesh into the off-screen target with the
		// given Euler rotation (radians) and solid-tint colour. Fails
		// silently if the device is gone.
		bool render_frame( float rot_x, float rot_y, float rot_z, color_t tint );

		// D2D bitmap wrapper of the off-screen render target. Only valid
		// after a successful initialize(). Safe to pass to
		// ID2D1DeviceContext::DrawBitmap in the D2D pass.
		ID2D1Bitmap1* bitmap( ) const { return m_d2d_target.Get( ); }

		int width( )  const { return m_width; }
		int height( ) const { return m_height; }

	private:
		bool build_procedural_mesh( );
		bool build_shaders( );
		bool build_target( int width, int height );

		c_device* m_device = nullptr;
		int       m_width  = 0;
		int       m_height = 0;

		// The 3D pass writes into an MSAA multisample colour target
		// (m_target_ms + m_rtv_ms). After the last draw, the MSAA
		// pixels are resolved into m_target — a single-sample copy
		// wrapped as m_d2d_target so the D2D composite pass can
		// DrawBitmap it just like any other image. Depth buffer is
		// MSAA-matched so early-z / stencil work correctly during
		// the multisampled render.
		Microsoft::WRL::ComPtr< ID3D11Texture2D >         m_target;      // resolve destination (single-sample)
		Microsoft::WRL::ComPtr< ID3D11Texture2D >         m_target_ms;   // 4×MSAA colour RTV
		Microsoft::WRL::ComPtr< ID3D11RenderTargetView >  m_rtv_ms;
		Microsoft::WRL::ComPtr< ID3D11Texture2D >         m_depth;       // 4×MSAA depth
		Microsoft::WRL::ComPtr< ID3D11DepthStencilView >  m_dsv;
		Microsoft::WRL::ComPtr< ID2D1Bitmap1 >            m_d2d_target;  // wraps m_target for the D2D pass

		Microsoft::WRL::ComPtr< ID3D11VertexShader > m_vs;
		Microsoft::WRL::ComPtr< ID3D11PixelShader  > m_ps;
		Microsoft::WRL::ComPtr< ID3D11InputLayout  > m_input_layout;
		Microsoft::WRL::ComPtr< ID3D11Buffer       > m_cb;
		Microsoft::WRL::ComPtr< ID3D11RasterizerState  > m_raster;
		// No-cull rasterizer for materials flagged doubleSided in glTF.
		// Fortnite face planes / eyelashes / print decals are authored
		// as single-triangle quads that would disappear from one side
		// under back-face culling.
		Microsoft::WRL::ComPtr< ID3D11RasterizerState  > m_raster_double;
		Microsoft::WRL::ComPtr< ID3D11DepthStencilState > m_ds_state;
		Microsoft::WRL::ComPtr< ID3D11BlendState        > m_blend;
		Microsoft::WRL::ComPtr< ID3D11SamplerState      > m_sampler;

		// A 1×1 white texture used when a primitive's material has no
		// baseColorTexture — keeps the pixel shader's Sample() call valid
		// without a per-primitive branch.
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView > m_white_srv;

		// 1×1 black SRV — bound at t1 (emissive_tex) when a material
		// has no emissiveTexture. Sampled colour is (0,0,0) so the
		// glow contribution collapses to just `emissive_factor` (which
		// is also zero for non-emissive materials, so nothing lights).
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView > m_black_srv;

		// Currently-active mesh. Either built by build_procedural_mesh()
		// or handed in via set_mesh(). Vertex format matches mesh_vertex_t
		// in both cases (position + normal + UV).
		loaded_mesh_t m_mesh;

		// Breathing animation clock. Set on initialize() so the very
		// first frame starts at phase 0 (fully exhaled). Used by
		// render_frame to build a Y-scale-from-feet cycle that reads as
		// the character breathing — the best whole-body approximation
		// available while the loader has no skeletal skinning path.
		std::chrono::steady_clock::time_point m_start_time;

		// Head-turn state machine. Alternates between HOLD (head still
		// at the current angle) and TURN (smoothly rotating toward a
		// freshly-randomised target). Values are in radians about the
		// world Y axis and get applied per-vertex in the vertex shader,
		// weighted by post-fit Y so only the head area actually rotates
		// while shoulders/torso stay put (no skinning rig, so a per-Y
		// blend is the closest approximation we have to "head turn
		// only"). Seeded on initialize().
		float    m_head_current_angle = 0.0f;
		float    m_head_target_angle  = 0.0f;
		float    m_head_phase_start_s = 0.0f;   // seconds from m_start_time
		float    m_head_phase_dur_s   = 0.0f;   // set to a first hold on init
		bool     m_head_phase_is_turn = false;
		std::uint32_t m_head_rng_state = 0u;   // xorshift32, seeded on init
	};
}

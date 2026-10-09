#pragma once

#include "device.hxx"
#include <cstdint>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <string_view>
#include <vector>
#include <wrl/client.h>

namespace render {

	// Vertex format the mesh loader unpacks glTF primitives into. Matches
	// scene3d's textured input layout exactly (position + normal + UV0).
	struct mesh_vertex_t {
		float px, py, pz;
		float nx, ny, nz;
		float u, v;
	};

	// Per-material state — one for every glTF material referenced by the
	// loaded primitives. baseColorTexture + baseColorFactor drive the
	// diffuse; alpha_mode / alpha_cutoff / double_sided / unlit come
	// straight from the glTF material JSON so face planes / eyelashes /
	// print decals render correctly on Fortnite-style characters.
	// normalTexture is loaded when present but currently unused (kept for
	// a future PBR shader upgrade).
	struct loaded_material_t {
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView > diffuse_srv;
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView > normal_srv;
		DirectX::XMFLOAT4 base_color_factor { 1.0f, 1.0f, 1.0f, 1.0f };
		float metallic  = 0.0f;
		float roughness = 1.0f;
		bool  has_diffuse = false;

		// glTF alphaMode: 0 = OPAQUE (default; forced-opaque output),
		// 1 = MASK (discard where alpha < alpha_cutoff),
		// 2 = BLEND (premultiplied-alpha output for translucent overlays).
		int   alpha_mode  = 0;
		float alpha_cutoff = 0.5f;   // glTF default

		// glTF doubleSided — face planes, eyelashes, and print decals
		// authored as single-triangle quads set this so the mesh reads
		// correctly from either side. Honoured via per-primitive
		// rasterizer swap.
		bool  double_sided = false;

		// KHR_materials_unlit — some Fortnite face parts / logos are
		// authored as unlit so shading doesn't wash out their baked
		// details. When set, the shader skips the Lambert step and
		// outputs the sampled colour directly.
		bool  unlit = false;

		// Emissive — a lot of Fortnite skins carry an emissive texture
		// (eye glow, weapon glow, chest reactor). We render those in
		// the caller's tint colour instead of the authored emissive so
		// the glow picks up the menu's accent theme. `emissive_factor`
		// is folded into the multiplier so materials that specify only
		// a factor (no texture) still contribute. Includes the
		// KHR_materials_emissive_strength multiplier when present.
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView > emissive_srv;
		DirectX::XMFLOAT4 emissive_factor { 0.0f, 0.0f, 0.0f, 0.0f };
		bool  has_emissive_tex = false;
	};

	// One glTF primitive → one D3D draw call. A single glTF mesh may have
	// multiple primitives (one per material — body, face-accessory, etc.),
	// so scene3d iterates the vector and issues one DrawIndexed per entry.
	struct loaded_primitive_t {
		Microsoft::WRL::ComPtr< ID3D11Buffer > vb;
		Microsoft::WRL::ComPtr< ID3D11Buffer > ib;
		std::uint32_t index_count   = 0;
		std::uint32_t vertex_stride = static_cast< std::uint32_t >( sizeof( mesh_vertex_t ) );
		DXGI_FORMAT   index_format  = DXGI_FORMAT_R16_UINT;
		int           material      = -1;   // index into loaded_mesh_t::materials
	};

	struct loaded_mesh_t {
		std::vector< loaded_primitive_t > primitives;
		std::vector< loaded_material_t >  materials;

		// Precomputed model transform that centres the mesh on the origin
		// and scales it so its Y-extent is exactly 2.0 units — matches
		// the camera setup in scene3d (which was tuned for the built-in
		// procedural humanoid at that scale).
		DirectX::XMFLOAT4X4 fit_matrix {};

		bool empty( ) const { return primitives.empty( ); }
	};

	// Auto-orientation strategy applied while building fit_matrix.
	//
	//  · none       — leave the mesh in its authored orientation. Bounds
	//                 are still centred + scaled but no rotation is applied.
	//                 Use when the caller wants full manual control.
	//  · tall_to_y  — default for CHARACTERS. Detects the longest axis;
	//                 if it isn't already Y, rotates so it becomes +Y
	//                 (Z-up or X-up mesh → Y-up standing character).
	//  · long_to_x  — for HORIZONTAL PROPS (rifles, swords, poles).
	//                 Detects the longest axis and rotates so it becomes
	//                 +X, i.e. the barrel/blade lies horizontally in the
	//                 world.
	enum class fit_axis_t { none, tall_to_y, long_to_x };

	struct load_options_t {
		fit_axis_t fit_axis        = fit_axis_t::tall_to_y;
		// Post-orient 180° Y rotation so a character's face ends up at
		// world -Z (toward the camera). Only meaningful for characters;
		// props leave this false.
		bool       face_camera_180 = true;
	};

	// Load a glTF binary (.glb) file from disk and upload it to `device`.
	// External .png textures referenced by URI are resolved relative to the
	// .glb's directory (this matches FBX2glTF's default output layout when
	// invoked WITHOUT -e/--embed-textures).
	//
	// Returns an empty mesh (empty()==true) on any parse / upload failure.
	// Any diagnostics go to utility::print so the render worker's log
	// captures a specific reason.
	bool load_glb(
		c_device& device,
		std::wstring_view path,
		loaded_mesh_t& out,
		const load_options_t& opts = {} );
}

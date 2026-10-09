#include <deps/includes.h>
#include "mesh_loader.hxx"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <wincodec.h>

namespace render {

	using namespace DirectX;

	// ── Minimal JSON DOM parser (unchanged from v1) ─────────────────────────

	enum class json_type { null, boolean, number, string, array, object };

	struct json_value {
		json_type type = json_type::null;
		double    num  = 0.0;
		bool      boolean = false;
		std::string_view str;
		std::vector< json_value > arr;
		std::vector< std::pair< std::string_view, json_value > > obj;
	};

	namespace {

		class json_parser {
		public:
			bool parse( std::string_view s, json_value& out ) {
				m_src = s.data( );
				m_len = s.size( );
				m_pos = 0;
				return parse_value( out );
			}

		private:
			const char* m_src = nullptr;
			std::size_t m_len = 0;
			std::size_t m_pos = 0;

			void skip_ws( ) {
				while ( m_pos < m_len ) {
					char c = m_src[ m_pos ];
					if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' ) ++m_pos;
					else return;
				}
			}
			bool parse_value( json_value& v ) {
				skip_ws( );
				if ( m_pos >= m_len ) return false;
				const char c = m_src[ m_pos ];
				if ( c == '"' ) return parse_string( v );
				if ( c == '{' ) return parse_object( v );
				if ( c == '[' ) return parse_array( v );
				if ( c == 't' || c == 'f' ) return parse_bool( v );
				if ( c == 'n' ) return parse_null( v );
				if ( c == '-' || ( c >= '0' && c <= '9' ) ) return parse_number( v );
				return false;
			}
			bool parse_string( json_value& v ) {
				if ( m_pos >= m_len || m_src[ m_pos ] != '"' ) return false;
				++m_pos;
				const std::size_t start = m_pos;
				while ( m_pos < m_len && m_src[ m_pos ] != '"' ) {
					if ( m_src[ m_pos ] == '\\' && m_pos + 1 < m_len ) m_pos += 2;
					else ++m_pos;
				}
				if ( m_pos >= m_len ) return false;
				v.type = json_type::string;
				v.str = std::string_view( m_src + start, m_pos - start );
				++m_pos;
				return true;
			}
			bool parse_number( json_value& v ) {
				char* end = nullptr;
				v.type = json_type::number;
				v.num  = std::strtod( m_src + m_pos, &end );
				m_pos = static_cast< std::size_t >( end - m_src );
				return true;
			}
			bool parse_bool( json_value& v ) {
				if ( m_pos + 4 <= m_len && std::memcmp( m_src + m_pos, "true", 4 ) == 0 ) {
					v.type = json_type::boolean; v.boolean = true; m_pos += 4; return true;
				}
				if ( m_pos + 5 <= m_len && std::memcmp( m_src + m_pos, "false", 5 ) == 0 ) {
					v.type = json_type::boolean; v.boolean = false; m_pos += 5; return true;
				}
				return false;
			}
			bool parse_null( json_value& v ) {
				if ( m_pos + 4 <= m_len && std::memcmp( m_src + m_pos, "null", 4 ) == 0 ) {
					v.type = json_type::null; m_pos += 4; return true;
				}
				return false;
			}
			bool parse_object( json_value& v ) {
				if ( m_pos >= m_len || m_src[ m_pos ] != '{' ) return false;
				++m_pos;
				v.type = json_type::object;
				skip_ws( );
				while ( m_pos < m_len && m_src[ m_pos ] != '}' ) {
					skip_ws( );
					json_value key;
					if ( !parse_string( key ) ) return false;
					skip_ws( );
					if ( m_pos >= m_len || m_src[ m_pos ] != ':' ) return false;
					++m_pos;
					json_value val;
					if ( !parse_value( val ) ) return false;
					v.obj.emplace_back( key.str, std::move( val ) );
					skip_ws( );
					if ( m_pos < m_len && m_src[ m_pos ] == ',' ) ++m_pos;
					skip_ws( );
				}
				if ( m_pos >= m_len ) return false;
				++m_pos;
				return true;
			}
			bool parse_array( json_value& v ) {
				if ( m_pos >= m_len || m_src[ m_pos ] != '[' ) return false;
				++m_pos;
				v.type = json_type::array;
				skip_ws( );
				while ( m_pos < m_len && m_src[ m_pos ] != ']' ) {
					json_value val;
					if ( !parse_value( val ) ) return false;
					v.arr.push_back( std::move( val ) );
					skip_ws( );
					if ( m_pos < m_len && m_src[ m_pos ] == ',' ) ++m_pos;
					skip_ws( );
				}
				if ( m_pos >= m_len ) return false;
				++m_pos;
				return true;
			}
		};

		const json_value* find( const json_value& obj, std::string_view key ) {
			if ( obj.type != json_type::object ) return nullptr;
			for ( auto& [ k, v ] : obj.obj )
				if ( k == key ) return &v;
			return nullptr;
		}
		double as_num( const json_value* v, double dflt = 0.0 ) {
			return ( v && v->type == json_type::number ) ? v->num : dflt;
		}
		int as_int( const json_value* v, int dflt = 0 ) {
			return static_cast< int >( as_num( v, static_cast< double >( dflt ) ) );
		}

		enum : int {
			ct_i8   = 5120, ct_u8   = 5121,
			ct_i16  = 5122, ct_u16  = 5123,
			ct_u32  = 5125, ct_f32  = 5126,
		};

		std::size_t component_size( int component_type ) {
			switch ( component_type ) {
				case ct_i8: case ct_u8: return 1;
				case ct_i16: case ct_u16: return 2;
				case ct_u32: case ct_f32: return 4;
			}
			return 0;
		}
		int type_component_count( std::string_view t ) {
			if ( t == "SCALAR" ) return 1;
			if ( t == "VEC2" )   return 2;
			if ( t == "VEC3" )   return 3;
			if ( t == "VEC4" )   return 4;
			if ( t == "MAT2" )   return 4;
			if ( t == "MAT3" )   return 9;
			if ( t == "MAT4" )   return 16;
			return 0;
		}

		bool create_texture_from_png_bytes(
			ID3D11Device*        d3d,
			ID3D11DeviceContext* dc,
			const void* png_bytes, std::size_t png_size,
			Microsoft::WRL::ComPtr< ID3D11ShaderResourceView >& out_srv )
		{
			Microsoft::WRL::ComPtr< IWICImagingFactory > factory;
			if ( FAILED( CoCreateInstance( CLSID_WICImagingFactory, nullptr,
					CLSCTX_INPROC_SERVER, IID_PPV_ARGS( factory.GetAddressOf( ) ) ) ) )
				return false;
			Microsoft::WRL::ComPtr< IWICStream > stream;
			if ( FAILED( factory->CreateStream( stream.GetAddressOf( ) ) ) )
				return false;
			if ( FAILED( stream->InitializeFromMemory(
					reinterpret_cast< BYTE* >( const_cast< void* >( png_bytes ) ),
					static_cast< DWORD >( png_size ) ) ) )
				return false;
			Microsoft::WRL::ComPtr< IWICBitmapDecoder > decoder;
			if ( FAILED( factory->CreateDecoderFromStream(
					stream.Get( ), nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf( ) ) ) )
				return false;
			Microsoft::WRL::ComPtr< IWICBitmapFrameDecode > frame;
			if ( FAILED( decoder->GetFrame( 0, frame.GetAddressOf( ) ) ) )
				return false;
			Microsoft::WRL::ComPtr< IWICFormatConverter > converter;
			if ( FAILED( factory->CreateFormatConverter( converter.GetAddressOf( ) ) ) )
				return false;
			if ( FAILED( converter->Initialize( frame.Get( ), GUID_WICPixelFormat32bppPBGRA,
					WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom ) ) )
				return false;
			UINT w = 0, h = 0;
			if ( FAILED( converter->GetSize( &w, &h ) ) || !w || !h )
				return false;
			const UINT stride = w * 4;
			std::vector< std::uint8_t > pixels( static_cast< std::size_t >( stride ) * h );
			if ( FAILED( converter->CopyPixels( nullptr, stride,
					static_cast< UINT >( pixels.size( ) ), pixels.data( ) ) ) )
				return false;

			// MipLevels = 0 + BindFlags |= RENDER_TARGET + MiscFlags |=
			// GENERATE_MIPS lets GenerateMips() fill in the mip chain
			// after we upload level-0. Anisotropic sampling in the
			// scene3d shader picks the right mip per pixel — kills
			// texture shimmer and keeps details sharp even when the
			// model shrinks to fill a small preview panel.
			D3D11_TEXTURE2D_DESC td {};
			td.Width  = w; td.Height = h;
			td.MipLevels = 0;               // full mip chain
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			Microsoft::WRL::ComPtr< ID3D11Texture2D > tex;
			if ( FAILED( d3d->CreateTexture2D( &td, nullptr, tex.GetAddressOf( ) ) ) )
				return false;

			// Upload the base level, then have D3D synthesise the
			// lower-res mips by 2×2 filtering. UpdateSubresource is
			// simplest — the texture is D3D11_USAGE_DEFAULT (GPU-side)
			// and this is a one-shot upload at load time.
			if ( dc ) {
				dc->UpdateSubresource(
					tex.Get( ), 0, nullptr,
					pixels.data( ), stride, 0 );
			}
			if ( FAILED( d3d->CreateShaderResourceView( tex.Get( ), nullptr, out_srv.GetAddressOf( ) ) ) )
				return false;
			if ( dc )
				dc->GenerateMips( out_srv.Get( ) );
			return true;
		}

		bool create_texture_from_png_file(
			ID3D11Device*        d3d,
			ID3D11DeviceContext* dc,
			const std::wstring& path,
			Microsoft::WRL::ComPtr< ID3D11ShaderResourceView >& out_srv )
		{
			std::ifstream f( path, std::ios::binary | std::ios::ate );
			if ( !f ) return false;
			const auto size = static_cast< std::size_t >( f.tellg( ) );
			if ( !size ) return false;
			std::vector< std::uint8_t > bytes( size );
			f.seekg( 0 );
			if ( !f.read( reinterpret_cast< char* >( bytes.data( ) ), size ) )
				return false;
			return create_texture_from_png_bytes( d3d, dc, bytes.data( ), size, out_srv );
		}

		std::wstring decode_uri_to_wide(
			std::string_view uri,
			const std::filesystem::path& base_dir )
		{
			std::string decoded;
			decoded.reserve( uri.size( ) );
			for ( std::size_t i = 0; i < uri.size( ); ++i ) {
				if ( uri[ i ] == '%' && i + 2 < uri.size( ) ) {
					auto hex = [ ] ( char c ) -> int {
						if ( c >= '0' && c <= '9' ) return c - '0';
						if ( c >= 'a' && c <= 'f' ) return 10 + ( c - 'a' );
						if ( c >= 'A' && c <= 'F' ) return 10 + ( c - 'A' );
						return -1;
					};
					const int hi = hex( uri[ i + 1 ] ), lo = hex( uri[ i + 2 ] );
					if ( hi >= 0 && lo >= 0 ) {
						decoded.push_back( static_cast< char >( ( hi << 4 ) | lo ) );
						i += 2; continue;
					}
				}
				decoded.push_back( uri[ i ] );
			}
			auto p = base_dir / std::filesystem::path( decoded );
			return p.wstring( );
		}
	}

	// ── Main loader ─────────────────────────────────────────────────────────
	bool load_glb(
		c_device& device,
		std::wstring_view path,
		loaded_mesh_t& out,
		const load_options_t& opts )
	{
		out = loaded_mesh_t {};
		auto* d3d = device.d3d_device( );
		auto* dc  = device.d3d_context( );   // needed for GenerateMips on loaded textures
		if ( !d3d ) {
			utility::print( "mesh_loader: no d3d device" );
			return false;
		}

		std::wstring path_w( path );
		std::ifstream file( path_w, std::ios::binary | std::ios::ate );
		if ( !file ) {
			char narrow[ 260 ] {};
			WideCharToMultiByte( CP_UTF8, 0, path_w.c_str( ), -1, narrow, sizeof( narrow ), nullptr, nullptr );
			utility::print( "mesh_loader: cannot open '%s'", narrow );
			return false;
		}
		const auto file_size = static_cast< std::size_t >( file.tellg( ) );
		std::vector< std::uint8_t > blob( file_size );
		file.seekg( 0 );
		if ( !file.read( reinterpret_cast< char* >( blob.data( ) ), file_size ) ) {
			utility::print( "mesh_loader: read failed" );
			return false;
		}

		if ( file_size < 20 ) { utility::print( "mesh_loader: file too small" ); return false; }
		auto read_u32 = [ & ] ( std::size_t offset ) -> std::uint32_t {
			std::uint32_t v = 0;
			std::memcpy( &v, blob.data( ) + offset, 4 );
			return v;
		};
		const std::uint32_t magic   = read_u32( 0 );
		const std::uint32_t version = read_u32( 4 );
		if ( magic != 0x46546C67u ) { utility::print( "mesh_loader: not a glb (magic=0x%08X)", magic ); return false; }
		if ( version != 2 )         { utility::print( "mesh_loader: unsupported glTF version %u", version ); return false; }

		const std::uint32_t json_len  = read_u32( 12 );
		const std::uint32_t json_type_hdr = read_u32( 16 );
		if ( json_type_hdr != 0x4E4F534Au ) { utility::print( "mesh_loader: first chunk not JSON" ); return false; }
		if ( 20 + json_len > file_size )    { utility::print( "mesh_loader: JSON chunk overruns" ); return false; }
		std::string_view json_view(
			reinterpret_cast< const char* >( blob.data( ) + 20 ), json_len );

		const std::uint8_t* bin_data = nullptr;
		std::size_t         bin_size = 0;
		if ( 20 + json_len + 8 <= file_size ) {
			const std::uint32_t bin_len  = read_u32( 20 + json_len );
			const std::uint32_t bin_type = read_u32( 20 + json_len + 4 );
			if ( bin_type == 0x004E4942u && 20 + json_len + 8 + bin_len <= file_size ) {
				bin_data = blob.data( ) + 20 + json_len + 8;
				bin_size = bin_len;
			}
		}

		json_value root;
		if ( !json_parser {}.parse( json_view, root ) || root.type != json_type::object ) {
			utility::print( "mesh_loader: JSON parse failed" );
			return false;
		}

		auto* buffer_views = find( root, "bufferViews" );
		auto* accessors    = find( root, "accessors" );
		auto* meshes       = find( root, "meshes" );
		auto* materials    = find( root, "materials" );
		auto* textures     = find( root, "textures" );
		auto* images       = find( root, "images" );
		auto* nodes        = find( root, "nodes" );
		auto* scenes       = find( root, "scenes" );
		auto* skins        = find( root, "skins" );   // skeletal skinning — used for the arm-drop pose bake
		if ( !buffer_views || !accessors || !meshes ) {
			utility::print( "mesh_loader: missing bufferViews/accessors/meshes" );
			return false;
		}
		if ( meshes->type != json_type::array || meshes->arr.empty( ) ) {
			utility::print( "mesh_loader: no meshes in glb" );
			return false;
		}

		// extensionsRequired — anything we don't decode (Draco, meshopt)
		// means primitives will drop silently, so flag them as errors.
		if ( auto* er = find( root, "extensionsRequired" );
			 er && er->type == json_type::array ) {
			for ( const auto& e : er->arr ) {
				if ( e.type == json_type::string )
					utility::print( "mesh_loader: extensionsREQUIRED (unsupported): %.*s",
						static_cast< int >( e.str.size( ) ), e.str.data( ) );
			}
		}

		// ── Helpers over the DOM ────────────────────────────────────────
		auto get_buffer_view_range = [ & ] ( int view_index,
			const std::uint8_t*& data_out, std::size_t& size_out, std::size_t& stride_out ) -> bool {
				if ( view_index < 0 || view_index >= static_cast< int >( buffer_views->arr.size( ) ) ) return false;
				const auto& bv = buffer_views->arr[ view_index ];
				const int buffer_i    = as_int( find( bv, "buffer" ), 0 );
				const std::size_t off = static_cast< std::size_t >( as_num( find( bv, "byteOffset" ), 0.0 ) );
				const std::size_t len = static_cast< std::size_t >( as_num( find( bv, "byteLength" ), 0.0 ) );
				stride_out            = static_cast< std::size_t >( as_num( find( bv, "byteStride" ), 0.0 ) );
				if ( buffer_i != 0 || !bin_data || off + len > bin_size ) return false;
				data_out = bin_data + off;
				size_out = len;
				return true;
			};

		struct accessor_view_t {
			const std::uint8_t* data = nullptr;
			std::size_t         count = 0;
			std::size_t         stride = 0;
			int                 component_type = ct_f32;
			int                 components     = 0;
		};
		auto read_accessor = [ & ] ( int accessor_index, accessor_view_t& out_view ) -> bool {
			if ( accessor_index < 0 || accessor_index >= static_cast< int >( accessors->arr.size( ) ) ) return false;
			const auto& a = accessors->arr[ accessor_index ];
			const int view_index = as_int( find( a, "bufferView" ), -1 );
			const std::size_t byte_offset = static_cast< std::size_t >( as_num( find( a, "byteOffset" ), 0.0 ) );
			out_view.component_type       = as_int( find( a, "componentType" ), ct_f32 );
			out_view.count                = static_cast< std::size_t >( as_num( find( a, "count" ), 0.0 ) );
			auto* type = find( a, "type" );
			out_view.components = type_component_count( type ? type->str : std::string_view {} );

			const std::uint8_t* view_data = nullptr;
			std::size_t view_size = 0, view_stride = 0;
			if ( !get_buffer_view_range( view_index, view_data, view_size, view_stride ) ) return false;
			if ( byte_offset > view_size ) return false;
			out_view.data = view_data + byte_offset;
			const std::size_t elem_size = component_size( out_view.component_type ) * out_view.components;
			out_view.stride = view_stride ? view_stride : elem_size;
			return true;
		};

		auto read_float = [ ] ( const std::uint8_t* p, int ct ) -> float {
			switch ( ct ) {
				case ct_f32: { float v; std::memcpy( &v, p, 4 ); return v; }
				case ct_u16: { std::uint16_t v; std::memcpy( &v, p, 2 ); return v / 65535.0f; }
				case ct_u8:  return p[ 0 ] / 255.0f;
				case ct_i16: { std::int16_t v; std::memcpy( &v, p, 2 ); return v / 32767.0f; }
				case ct_i8:  return static_cast< std::int8_t >( p[ 0 ] ) / 127.0f;
			}
			return 0.0f;
		};

		// ── Load textures ────────────────────────────────────────────────
		std::filesystem::path base_dir = std::filesystem::path( path_w ).parent_path( );
		std::vector< Microsoft::WRL::ComPtr< ID3D11ShaderResourceView > > srvs;
		if ( images && images->type == json_type::array ) {
			srvs.reserve( images->arr.size( ) );
			for ( const auto& img : images->arr ) {
				Microsoft::WRL::ComPtr< ID3D11ShaderResourceView > srv;
				auto* uri = find( img, "uri" );
				if ( uri && uri->type == json_type::string ) {
					std::wstring resolved = decode_uri_to_wide( uri->str, base_dir );
					if ( !create_texture_from_png_file( d3d, dc, resolved, srv ) )
						utility::print( "mesh_loader: PNG load failed for image URI" );
				}
				else {
					const int bv_idx = as_int( find( img, "bufferView" ), -1 );
					const std::uint8_t* bv_data = nullptr;
					std::size_t         bv_size = 0, bv_stride = 0;
					if ( get_buffer_view_range( bv_idx, bv_data, bv_size, bv_stride ) ) {
						if ( !create_texture_from_png_bytes( d3d, dc, bv_data, bv_size, srv ) )
							utility::print( "mesh_loader: embedded PNG decode failed" );
					}
				}
				srvs.push_back( std::move( srv ) );
			}
		}

		auto image_index_for_texture = [ & ] ( int tex_index ) -> int {
			if ( !textures || tex_index < 0 || tex_index >= static_cast< int >( textures->arr.size( ) ) ) return -1;
			return as_int( find( textures->arr[ tex_index ], "source" ), -1 );
		};

		// ── Load materials ───────────────────────────────────────────────
		if ( materials && materials->type == json_type::array ) {
			out.materials.reserve( materials->arr.size( ) );
			for ( const auto& mat : materials->arr ) {
				loaded_material_t lm {};
				auto* pbr = find( mat, "pbrMetallicRoughness" );
				if ( pbr ) {
					if ( auto* bcf = find( *pbr, "baseColorFactor" );
						 bcf && bcf->type == json_type::array && bcf->arr.size( ) >= 4 ) {
						lm.base_color_factor = XMFLOAT4(
							static_cast< float >( bcf->arr[ 0 ].num ),
							static_cast< float >( bcf->arr[ 1 ].num ),
							static_cast< float >( bcf->arr[ 2 ].num ),
							static_cast< float >( bcf->arr[ 3 ].num ) );
					}
					lm.metallic  = static_cast< float >( as_num( find( *pbr, "metallicFactor" ),  0.0 ) );
					lm.roughness = static_cast< float >( as_num( find( *pbr, "roughnessFactor" ), 1.0 ) );
					if ( auto* bct = find( *pbr, "baseColorTexture" ) ) {
						const int tex_i = as_int( find( *bct, "index" ), -1 );
						const int img_i = image_index_for_texture( tex_i );
						if ( img_i >= 0 && img_i < static_cast< int >( srvs.size( ) ) && srvs[ img_i ] ) {
							lm.diffuse_srv = srvs[ img_i ];
							lm.has_diffuse = true;
						}
					}
				}
				if ( auto* nt = find( mat, "normalTexture" ) ) {
					const int tex_i = as_int( find( *nt, "index" ), -1 );
					const int img_i = image_index_for_texture( tex_i );
					if ( img_i >= 0 && img_i < static_cast< int >( srvs.size( ) ) && srvs[ img_i ] )
						lm.normal_srv = srvs[ img_i ];
				}

				// emissiveFactor + emissiveTexture — glTF 2.0 core PBR.
				// KHR_materials_emissive_strength (extension) is a scalar
				// that multiplies the factor beyond the default [0..1]
				// range; folded straight into emissive_factor here so
				// the shader doesn't need a separate uniform.
				float emiss_strength = 1.0f;
				if ( auto* exts = find( mat, "extensions" ) ) {
					if ( auto* es = find( *exts, "KHR_materials_emissive_strength" ) ) {
						emiss_strength = static_cast< float >(
							as_num( find( *es, "emissiveStrength" ), 1.0 ) );
					}
				}
				if ( auto* ef = find( mat, "emissiveFactor" );
					 ef && ef->type == json_type::array && ef->arr.size( ) >= 3 ) {
					lm.emissive_factor = XMFLOAT4(
						static_cast< float >( ef->arr[ 0 ].num ) * emiss_strength,
						static_cast< float >( ef->arr[ 1 ].num ) * emiss_strength,
						static_cast< float >( ef->arr[ 2 ].num ) * emiss_strength,
						1.0f );
				}
				if ( auto* et = find( mat, "emissiveTexture" ) ) {
					const int tex_i = as_int( find( *et, "index" ), -1 );
					const int img_i = image_index_for_texture( tex_i );
					if ( img_i >= 0 && img_i < static_cast< int >( srvs.size( ) ) && srvs[ img_i ] ) {
						lm.emissive_srv     = srvs[ img_i ];
						lm.has_emissive_tex = true;
					}
				}

				// alphaMode: OPAQUE (default), MASK, or BLEND. String
				// comparison rather than tokenising because we only need
				// to distinguish three constants.
				if ( auto* am = find( mat, "alphaMode" );
					 am && am->type == json_type::string ) {
					if ( am->str == "MASK" )       lm.alpha_mode = 1;
					else if ( am->str == "BLEND" ) lm.alpha_mode = 2;
					else                            lm.alpha_mode = 0;
				}
				lm.alpha_cutoff  = static_cast< float >( as_num( find( mat, "alphaCutoff" ), 0.5 ) );

				// doubleSided is a plain JSON bool; default false.
				if ( auto* ds = find( mat, "doubleSided" );
					 ds && ds->type == json_type::boolean )
					lm.double_sided = ds->boolean;

				// KHR_materials_unlit lives under material.extensions.
				// The extension body is an empty object; its mere
				// presence flips the material to unlit.
				if ( auto* ext = find( mat, "extensions" ) ) {
					if ( find( *ext, "KHR_materials_unlit" ) )
						lm.unlit = true;
				}

				out.materials.push_back( std::move( lm ) );
			}
		}

		// ── Primitive loader — one call per mesh instance from the graph ─
		XMFLOAT3 bounds_min {  FLT_MAX,  FLT_MAX,  FLT_MAX };
		XMFLOAT3 bounds_max { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		auto expand_bounds = [ & ] ( float x, float y, float z ) {
			bounds_min.x = ( std::min )( bounds_min.x, x );
			bounds_min.y = ( std::min )( bounds_min.y, y );
			bounds_min.z = ( std::min )( bounds_min.z, z );
			bounds_max.x = ( std::max )( bounds_max.x, x );
			bounds_max.y = ( std::max )( bounds_max.y, y );
			bounds_max.z = ( std::max )( bounds_max.z, z );
		};

		std::size_t mesh_instances_loaded = 0;
		std::size_t primitives_loaded     = 0;

		// Forward-declared tables that the mesh-instance loader closes over.
		// Filled BEFORE walk_node runs so the loader can look up per-node
		// info (world transform, skin) without recursion-context threading.
		const int num_nodes = ( nodes && nodes->type == json_type::array ) ? static_cast< int >( nodes->arr.size( ) ) : 0;
		std::vector< XMMATRIX > node_local( num_nodes, XMMatrixIdentity( ) );
		std::vector< XMMATRIX > node_world( num_nodes, XMMatrixIdentity( ) );

		auto load_mesh_instance = [ & ] ( int mesh_idx, int node_idx ) -> void {
			if ( mesh_idx < 0 || mesh_idx >= static_cast< int >( meshes->arr.size( ) ) ) return;
			const auto& mesh_json = meshes->arr[ mesh_idx ];
			auto* primitives = find( mesh_json, "primitives" );
			if ( !primitives || primitives->type != json_type::array ) return;

			++mesh_instances_loaded;

			int prim_index = -1;
			for ( const auto& prim : primitives->arr ) {
				++prim_index;

				// Diagnose common reasons for a primitive to drop out —
				// silent skips have been the culprit for missing body
				// parts before. Draco / meshopt compression stores the
				// real vertex data in a compressed extension buffer
				// (not the classic accessors), so our POSITION lookup
				// finds nothing usable and we'd otherwise just skip.
				if ( auto* pe = find( prim, "extensions" ) ) {
					if ( find( *pe, "KHR_draco_mesh_compression" ) ) {
						utility::print( "mesh_loader: SKIP mesh %d prim %d — Draco compressed (unsupported)",
							mesh_idx, prim_index );
						continue;
					}
					if ( find( *pe, "EXT_meshopt_compression" ) ) {
						utility::print( "mesh_loader: SKIP mesh %d prim %d — meshopt compressed (unsupported)",
							mesh_idx, prim_index );
						continue;
					}
				}

				auto* attribs = find( prim, "attributes" );
				if ( !attribs ) {
					utility::print( "mesh_loader: SKIP mesh %d prim %d — no attributes block",
						mesh_idx, prim_index );
					continue;
				}

				accessor_view_t pos_a {}, nrm_a {}, uv_a {};
				bool has_pos = false, has_nrm = false, has_uv = false;

				if ( auto* p = find( *attribs, "POSITION" ) ) {
					has_pos = read_accessor( as_int( p, -1 ), pos_a )
						&& pos_a.components == 3 && pos_a.component_type == ct_f32;
					if ( !has_pos ) {
						utility::print( "mesh_loader: SKIP mesh %d prim %d — POSITION accessor bad (components=%d ct=%d)",
							mesh_idx, prim_index, pos_a.components, pos_a.component_type );
					}
				}
				else {
					utility::print( "mesh_loader: SKIP mesh %d prim %d — no POSITION attribute",
						mesh_idx, prim_index );
				}
				if ( !has_pos ) continue;

				if ( auto* n = find( *attribs, "NORMAL" ) ) {
					has_nrm = read_accessor( as_int( n, -1 ), nrm_a )
						&& nrm_a.components == 3 && nrm_a.component_type == ct_f32;
				}
				if ( auto* t = find( *attribs, "TEXCOORD_0" ) ) {
					has_uv = read_accessor( as_int( t, -1 ), uv_a ) && uv_a.components == 2;
				}

				std::vector< mesh_vertex_t > verts( pos_a.count );
				for ( std::size_t i = 0; i < pos_a.count; ++i ) {
					mesh_vertex_t& v = verts[ i ];
					std::memcpy( &v.px, pos_a.data + i * pos_a.stride, 12 );
					if ( has_nrm && i < nrm_a.count )
						std::memcpy( &v.nx, nrm_a.data + i * nrm_a.stride, 12 );
					else { v.nx = 0.0f; v.ny = 1.0f; v.nz = 0.0f; }
					if ( has_uv && i < uv_a.count ) {
						v.u = read_float( uv_a.data + i * uv_a.stride + 0 * component_size( uv_a.component_type ), uv_a.component_type );
						v.v = read_float( uv_a.data + i * uv_a.stride + 1 * component_size( uv_a.component_type ), uv_a.component_type );
					} else { v.u = 0.0f; v.v = 0.0f; }
				}

				// ── Skinning vs static bake ────────────────────────────
				// If this mesh instance's node has a `skin`, apply linear-
				// blend skinning at load time so any pose modifications
				// (e.g. shoulder rotation for arm-drop) baked into
				// node_world[] land in the vertex buffer directly — no
				// runtime skinning shader, no CB inflation, and the same
				// downstream code path as unskinned meshes.
				//
				// glTF spec formula (mesh-parent-inverse cancels the
				// outer mesh_node_world multiply, so effectively):
				//   world_pos = Σ w_j · (node_world[skin.joints[j]] · IBM[j]) · bind_pos
				//
				// If the node has no skin (or the skin data is missing),
				// fall back to the classic bake of node_world into
				// vertices — matches the pre-skinning behaviour byte for
				// byte for characters like the procedural humanoid and
				// props with no bones.
				int skin_idx = -1;
				if ( node_idx >= 0 && node_idx < num_nodes && nodes ) {
					if ( auto* si = find( nodes->arr[ node_idx ], "skin" ) )
						skin_idx = as_int( si, -1 );
				}
				const XMMATRIX world = ( node_idx >= 0 && node_idx < num_nodes )
					? node_world[ node_idx ] : XMMatrixIdentity( );

				bool did_skinning = false;
				if ( skin_idx >= 0 && skins && skin_idx < static_cast< int >( skins->arr.size( ) ) ) {
					const auto& skin_json = skins->arr[ skin_idx ];
					auto* joints_list = find( skin_json, "joints" );

					accessor_view_t joints_a {}, weights_a {}, ibm_a {};
					bool has_j = false, has_w = false, has_ibm = false;
					if ( auto* j = find( *attribs, "JOINTS_0" ) )
						has_j = read_accessor( as_int( j, -1 ), joints_a ) && joints_a.components == 4;
					if ( auto* w = find( *attribs, "WEIGHTS_0" ) )
						has_w = read_accessor( as_int( w, -1 ), weights_a ) && weights_a.components == 4;
					if ( auto* ib = find( skin_json, "inverseBindMatrices" ) )
						has_ibm = read_accessor( as_int( ib, -1 ), ibm_a ) && ibm_a.components == 16;

					if ( has_j && has_w && joints_list && joints_list->type == json_type::array ) {
						const std::size_t num_joints = joints_list->arr.size( );
						// Precompute joint matrices — one per joint in the skin.
						std::vector< XMMATRIX > joint_matrices( num_joints, XMMatrixIdentity( ) );
						for ( std::size_t j = 0; j < num_joints; ++j ) {
							const int joint_node = static_cast< int >( joints_list->arr[ j ].num );
							XMMATRIX ibm = XMMatrixIdentity( );
							if ( has_ibm && j < ibm_a.count ) {
								XMFLOAT4X4 mf {};
								std::memcpy( &mf, ibm_a.data + j * ibm_a.stride, 64 );
								// glTF stores column-major bytes of a column-
								// vector matrix — those are byte-identical to
								// the row-major bytes of the equivalent row-
								// vector matrix that DirectXMath wants. So a
								// direct memcpy + XMLoadFloat4x4 is correct;
								// the transpose pattern that used to live here
								// produced inv(IBM) instead of IBM at bind
								// pose and folded every skinned vertex.
								ibm = XMLoadFloat4x4( &mf );
							}
							const XMMATRIX joint_world = ( joint_node >= 0 && joint_node < num_nodes )
								? node_world[ joint_node ] : XMMatrixIdentity( );
							joint_matrices[ j ] = ibm * joint_world;
						}

						// Blend per vertex — accumulate positions/normals
						// weighted by w_j, no per-vertex matrix summation.
						const std::size_t vcount = verts.size( );
						for ( std::size_t i = 0; i < vcount; ++i ) {
							const XMVECTOR bind_pos = XMVectorSet( verts[ i ].px, verts[ i ].py, verts[ i ].pz, 1.0f );
							const XMVECTOR bind_nrm = XMVectorSet( verts[ i ].nx, verts[ i ].ny, verts[ i ].nz, 0.0f );

							// Read up to 4 joints for this vertex — JOINTS_0
							// is UBYTE (small skins) or USHORT (large).
							std::uint16_t ji[ 4 ] = { 0, 0, 0, 0 };
							const std::uint8_t* jp = joints_a.data + i * joints_a.stride;
							if ( joints_a.component_type == ct_u8 ) {
								ji[ 0 ] = jp[ 0 ]; ji[ 1 ] = jp[ 1 ];
								ji[ 2 ] = jp[ 2 ]; ji[ 3 ] = jp[ 3 ];
							} else if ( joints_a.component_type == ct_u16 ) {
								std::memcpy( ji, jp, 8 );
							}
							// WEIGHTS_0 is FLOAT or normalised UBYTE/USHORT.
							float jw[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
							const std::uint8_t* wp = weights_a.data + i * weights_a.stride;
							const std::size_t wsz = component_size( weights_a.component_type );
							for ( int k = 0; k < 4; ++k )
								jw[ k ] = read_float( wp + k * wsz, weights_a.component_type );

							XMVECTOR pos_out = XMVectorZero( );
							XMVECTOR nrm_out = XMVectorZero( );
							for ( int k = 0; k < 4; ++k ) {
								if ( jw[ k ] <= 0.0f || ji[ k ] >= num_joints ) continue;
								const XMMATRIX& jm = joint_matrices[ ji[ k ] ];
								const XMVECTOR p_k = XMVector3Transform      ( bind_pos, jm );
								const XMVECTOR n_k = XMVector3TransformNormal( bind_nrm, jm );
								const XMVECTOR w_v = XMVectorReplicate( jw[ k ] );
								pos_out = XMVectorMultiplyAdd( p_k, w_v, pos_out );
								nrm_out = XMVectorMultiplyAdd( n_k, w_v, nrm_out );
							}
							nrm_out = XMVector3Normalize( nrm_out );

							verts[ i ].px = XMVectorGetX( pos_out );
							verts[ i ].py = XMVectorGetY( pos_out );
							verts[ i ].pz = XMVectorGetZ( pos_out );
							verts[ i ].nx = XMVectorGetX( nrm_out );
							verts[ i ].ny = XMVectorGetY( nrm_out );
							verts[ i ].nz = XMVectorGetZ( nrm_out );
							expand_bounds( verts[ i ].px, verts[ i ].py, verts[ i ].pz );
						}
						did_skinning = true;
					}
				}

				if ( !did_skinning ) {
					// Classic bake — node's world transform applied to
					// every vertex. Assembles multi-mesh characters (body +
					// hair + face each in its own glTF mesh) correctly
					// instead of collapsing to the origin.
					for ( auto& v : verts ) {
						XMVECTOR pos = XMVectorSet( v.px, v.py, v.pz, 1.0f );
						pos = XMVector3Transform( pos, world );
						v.px = XMVectorGetX( pos );
						v.py = XMVectorGetY( pos );
						v.pz = XMVectorGetZ( pos );

						XMVECTOR nrm = XMVectorSet( v.nx, v.ny, v.nz, 0.0f );
						nrm = XMVector3TransformNormal( nrm, world );
						nrm = XMVector3Normalize( nrm );
						v.nx = XMVectorGetX( nrm );
						v.ny = XMVectorGetY( nrm );
						v.nz = XMVectorGetZ( nrm );

						expand_bounds( v.px, v.py, v.pz );
					}
				}

				const int indices_accessor = as_int( find( prim, "indices" ), -1 );
				std::vector< std::uint32_t > indices_u32;
				accessor_view_t idx_a {};
				if ( indices_accessor >= 0 && read_accessor( indices_accessor, idx_a ) && idx_a.components == 1 ) {
					indices_u32.reserve( idx_a.count );
					for ( std::size_t i = 0; i < idx_a.count; ++i ) {
						const std::uint8_t* p = idx_a.data + i * idx_a.stride;
						std::uint32_t v = 0;
						switch ( idx_a.component_type ) {
							case ct_u8:  v = p[ 0 ]; break;
							case ct_u16: { std::uint16_t s; std::memcpy( &s, p, 2 ); v = s; break; }
							case ct_u32: { std::memcpy( &v, p, 4 ); break; }
							default: v = 0; break;
						}
						indices_u32.push_back( v );
					}
				}
				else {
					indices_u32.resize( verts.size( ) );
					for ( std::size_t i = 0; i < verts.size( ); ++i )
						indices_u32[ i ] = static_cast< std::uint32_t >( i );
				}

				loaded_primitive_t lp {};
				lp.index_count = static_cast< std::uint32_t >( indices_u32.size( ) );
				lp.material    = as_int( find( prim, "material" ), -1 );

				D3D11_BUFFER_DESC vbd {};
				vbd.ByteWidth = static_cast< UINT >( verts.size( ) * sizeof( mesh_vertex_t ) );
				vbd.Usage     = D3D11_USAGE_IMMUTABLE;
				vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				D3D11_SUBRESOURCE_DATA vinit { verts.data( ), 0, 0 };
				if ( FAILED( d3d->CreateBuffer( &vbd, &vinit, lp.vb.GetAddressOf( ) ) ) ) {
					utility::print( "mesh_loader: VB create failed (mesh %d)", mesh_idx );
					continue;
				}

				const bool fits_u16 = verts.size( ) <= 0xFFFFu;
				if ( fits_u16 ) {
					std::vector< std::uint16_t > i16( indices_u32.size( ) );
					for ( std::size_t i = 0; i < indices_u32.size( ); ++i )
						i16[ i ] = static_cast< std::uint16_t >( indices_u32[ i ] );
					D3D11_BUFFER_DESC ibd {};
					ibd.ByteWidth = static_cast< UINT >( i16.size( ) * sizeof( std::uint16_t ) );
					ibd.Usage     = D3D11_USAGE_IMMUTABLE;
					ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
					D3D11_SUBRESOURCE_DATA iinit { i16.data( ), 0, 0 };
					if ( FAILED( d3d->CreateBuffer( &ibd, &iinit, lp.ib.GetAddressOf( ) ) ) ) {
						utility::print( "mesh_loader: IB create failed (mesh %d)", mesh_idx );
						continue;
					}
					lp.index_format = DXGI_FORMAT_R16_UINT;
				}
				else {
					D3D11_BUFFER_DESC ibd {};
					ibd.ByteWidth = static_cast< UINT >( indices_u32.size( ) * sizeof( std::uint32_t ) );
					ibd.Usage     = D3D11_USAGE_IMMUTABLE;
					ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
					D3D11_SUBRESOURCE_DATA iinit { indices_u32.data( ), 0, 0 };
					if ( FAILED( d3d->CreateBuffer( &ibd, &iinit, lp.ib.GetAddressOf( ) ) ) ) {
						utility::print( "mesh_loader: IB create failed (mesh %d)", mesh_idx );
						continue;
					}
					lp.index_format = DXGI_FORMAT_R32_UINT;
				}

				out.primitives.push_back( std::move( lp ) );
				++primitives_loaded;
			}
		};

		// ── Walk the scene node graph ────────────────────────────────────
		// glTF's authoritative rendering order is the scene graph, not
		// meshes[]. Each node has a local matrix (or T/R/S) and may
		// reference a mesh; the mesh's primitives are rendered under the
		// node's world transform. We recurse from the root scene's nodes,
		// bake the world transform into every vertex, and collect
		// everything into one flat primitive list.
		auto node_local_matrix = [ & ] ( const json_value& node ) -> XMMATRIX {
			if ( auto* m = find( node, "matrix" );
				 m && m->type == json_type::array && m->arr.size( ) == 16 ) {
				XMFLOAT4X4 mf {};
				for ( int i = 0; i < 16; ++i )
					( &mf._11 )[ i ] = static_cast< float >( m->arr[ i ].num );
				// glTF's column-major bytes of a column-vector matrix are
				// byte-identical to the row-major bytes of the equivalent
				// row-vector matrix that DirectXMath uses. Direct load,
				// no transpose. (This branch is rarely hit for character
				// glTFs since most nodes use TRS, but keeping consistent
				// with the IBM load — the same bug used to live here.)
				return XMLoadFloat4x4( &mf );
			}
			XMVECTOR t = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
			XMVECTOR r = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
			XMVECTOR s = XMVectorSet( 1.0f, 1.0f, 1.0f, 0.0f );
			if ( auto* tv = find( node, "translation" );
				 tv && tv->type == json_type::array && tv->arr.size( ) >= 3 )
				t = XMVectorSet( ( float )tv->arr[ 0 ].num, ( float )tv->arr[ 1 ].num, ( float )tv->arr[ 2 ].num, 0.0f );
			if ( auto* rv = find( node, "rotation" );
				 rv && rv->type == json_type::array && rv->arr.size( ) >= 4 )
				r = XMVectorSet( ( float )rv->arr[ 0 ].num, ( float )rv->arr[ 1 ].num, ( float )rv->arr[ 2 ].num, ( float )rv->arr[ 3 ].num );
			if ( auto* sv = find( node, "scale" );
				 sv && sv->type == json_type::array && sv->arr.size( ) >= 3 )
				s = XMVectorSet( ( float )sv->arr[ 0 ].num, ( float )sv->arr[ 1 ].num, ( float )sv->arr[ 2 ].num, 0.0f );
			return XMMatrixScalingFromVector( s )
				 * XMMatrixRotationQuaternion( r )
				 * XMMatrixTranslationFromVector( t );
		};

		// Fill node_local[] from JSON. No pose modifications applied —
		// the arm-drop experiment was reverted; character renders in
		// its authored bind pose. Skinning infrastructure stays in
		// place because at bind pose it collapses to identity (same
		// result as the classic node-world bake), and having it wired
		// up means future pose changes can drop in without another
		// mesh-loader refactor.
		if ( nodes ) {
			for ( int i = 0; i < num_nodes; ++i )
				node_local[ i ] = node_local_matrix( nodes->arr[ i ] );
		}

		// Compute world transforms via DFS from every root (root = node
		// that no other node lists as a child). Roots are typically the
		// scene's `nodes[]` entries but we detect them from the tree so
		// we don't miss orphan chains that are still valid subtrees.
		std::vector< bool > is_child( num_nodes, false );
		if ( nodes ) {
			for ( int i = 0; i < num_nodes; ++i ) {
				auto* ch = find( nodes->arr[ i ], "children" );
				if ( !ch || ch->type != json_type::array ) continue;
				for ( const auto& c : ch->arr ) {
					const int ci = static_cast< int >( c.num );
					if ( ci >= 0 && ci < num_nodes ) is_child[ ci ] = true;
				}
			}
		}
		std::function< void( int, XMMATRIX ) > compute_world_dfs =
			[ & ] ( int idx, XMMATRIX parent ) {
				if ( idx < 0 || idx >= num_nodes ) return;
				node_world[ idx ] = node_local[ idx ] * parent;
				auto* ch = find( nodes->arr[ idx ], "children" );
				if ( !ch || ch->type != json_type::array ) return;
				for ( const auto& c : ch->arr )
					compute_world_dfs( static_cast< int >( c.num ), node_world[ idx ] );
			};
		if ( nodes ) {
			for ( int i = 0; i < num_nodes; ++i )
				if ( !is_child[ i ] ) compute_world_dfs( i, XMMatrixIdentity( ) );
		}

		// ── Walk the scene node graph ────────────────────────────────────
		// glTF's authoritative rendering order is the scene graph, not
		// meshes[]. We recurse from the root scene's nodes and call the
		// loader with the containing node index — the loader looks up
		// world / skin from the precomputed tables.
		std::function< void( int ) > walk_node =
			[ & ] ( int node_idx ) {
				if ( !nodes || node_idx < 0 || node_idx >= num_nodes ) return;
				const auto& node = nodes->arr[ node_idx ];

				if ( auto* mi = find( node, "mesh" ) )
					load_mesh_instance( as_int( mi, -1 ), node_idx );

				if ( auto* children = find( node, "children" );
					 children && children->type == json_type::array ) {
					for ( const auto& c : children->arr )
						walk_node( static_cast< int >( c.num ) );
				}
			};

		bool walked_scene = false;
		if ( scenes && scenes->type == json_type::array && !scenes->arr.empty( ) ) {
			const int scene_idx = as_int( find( root, "scene" ), 0 );
			const int use_scene = ( scene_idx >= 0 && scene_idx < static_cast< int >( scenes->arr.size( ) ) ) ? scene_idx : 0;
			auto* root_node_list = find( scenes->arr[ use_scene ], "nodes" );
			if ( root_node_list && root_node_list->type == json_type::array ) {
				for ( const auto& n : root_node_list->arr )
					walk_node( static_cast< int >( n.num ) );
				walked_scene = true;
			}
		}

		// Fallback for glTF files without a scene graph — load every mesh
		// under identity (node_idx = -1 disables skinning too). If nodes[]
		// exists but no scene references them, this still catches everything.
		if ( !walked_scene ) {
			for ( int i = 0; i < static_cast< int >( meshes->arr.size( ) ); ++i )
				load_mesh_instance( i, -1 );
		}

		if ( out.primitives.empty( ) ) {
			utility::print( "mesh_loader: no usable primitives" );
			return false;
		}

		// ── Fit matrix (built from world-space bounds) ───────────────────
		// Stage 1: detect the "tall axis" from the bounds. glTF assumes
		// Y-up, but many FBX pipelines export characters with head at +Z
		// (Z-up) that don't get properly converted through Blender's
		// axis-conversion; the character ends up lying on its belly in
		// glTF space. If Z (or X) is longer than Y, the character is
		// lying down along that axis and we build a rotation to stand it
		// back up so Y becomes the tallest dimension.
		const XMFLOAT3 size {
			bounds_max.x - bounds_min.x,
			bounds_max.y - bounds_min.y,
			bounds_max.z - bounds_min.z };
		const XMFLOAT3 center {
			( bounds_min.x + bounds_max.x ) * 0.5f,
			( bounds_min.y + bounds_max.y ) * 0.5f,
			( bounds_min.z + bounds_max.z ) * 0.5f };

		XMMATRIX    standup      = XMMatrixIdentity( );
		XMFLOAT3    rotated_size = size;
		const char* orient       = "unchanged";

		if ( opts.fit_axis == fit_axis_t::tall_to_y ) {
			// Character path: bring the longest axis to +Y so the mesh
			// stands upright.
			orient = "already-Y-up";
			if ( size.z > size.y && size.z > size.x ) {
				// Lying along Z axis (head at +Z / -Z). Rotate -90° X so
				// the character's head moves from +Z to +Y (standing).
				standup      = XMMatrixRotationX( -XM_PIDIV2 );
				rotated_size = XMFLOAT3 { size.x, size.z, size.y };
				orient       = "Z-up→Y-up";
			}
			else if ( size.x > size.y && size.x > size.z ) {
				// Lying along X axis. Rotate around Z to bring X → Y.
				standup      = XMMatrixRotationZ( XM_PIDIV2 );
				rotated_size = XMFLOAT3 { size.y, size.x, size.z };
				orient       = "X-up→Y-up";
			}
		}
		else if ( opts.fit_axis == fit_axis_t::long_to_x ) {
			// Prop path (rifle, blade, pole): bring the longest axis to +X
			// so the barrel lies horizontally in world space and the
			// caller can freely translate/rotate it as a placement.
			orient = "already-X-long";
			if ( size.z > size.y && size.z > size.x ) {
				// Long along Z → rotate +90° around Y (row-vector convention),
				// which maps (0,0,+1) → (+1,0,0). Y stays the vertical axis.
				standup      = XMMatrixRotationY( XM_PIDIV2 );
				rotated_size = XMFLOAT3 { size.z, size.y, size.x };
				orient       = "Z-long→X-long";
			}
			else if ( size.y > size.x && size.y > size.z ) {
				// Long along Y → rotate -90° around Z, which maps
				// (0,+1,0) → (+1,0,0).
				standup      = XMMatrixRotationZ( -XM_PIDIV2 );
				rotated_size = XMFLOAT3 { size.y, size.x, size.z };
				orient       = "Y-long→X-long";
			}
		}
		// fit_axis_t::none — keep standup = identity and orient = "unchanged".

		// Stage 2: after standing the character up we still may be facing
		// AWAY from the camera (camera sits at −Z looking toward +Z, so a
		// character whose face points at +Z is looking away). A 180° Y
		// rotation puts the face at −Z, toward the viewer. Opt-in via
		// load_options_t::face_camera_180 (true by default for characters,
		// false for props whose orientation is caller-managed).
		const XMMATRIX face_camera = opts.face_camera_180
			? XMMatrixRotationY( XM_PI )
			: XMMatrixIdentity( );

		// Stage 3: scale to fit a 2-unit tall preview, centred on origin.
		//
		// An earlier revision lifted the model by +rotated_size.y*scale*0.5
		// so its feet sat at y=0 (with the intent of "rotating about the
		// feet"). That was a mistake — every camera then had to aim at
		// the model's mid-height (~y=1) instead of the origin, and the
		// framing broke as soon as the scene was re-framed with an
		// origin-aim (the whole model ended up in the top half of the
		// frame with the head clipping out). Leaving the mesh centred
		// is also what you want for the rotation path: rotating around
		// origin now spins the model about its own centre, not about a
		// point below its feet.
		const float max_dim = ( std::max )(
			rotated_size.y, ( std::max )( rotated_size.x, rotated_size.z ) );
		const float scale = max_dim > 1e-4f ? ( 2.0f / max_dim ) : 1.0f;

		// Order: center at origin → standup → face-camera → scale.
		// The centering translation is applied BEFORE the rotations so
		// the standup rotation acts on a mesh whose centroid is already
		// at the origin (otherwise the rotation would swing the model
		// away from the frustum on top of standing it up).
		const XMMATRIX m =
			XMMatrixTranslation( -center.x, -center.y, -center.z ) *
			standup *
			face_camera *
			XMMatrixScaling( scale, scale, scale );
		XMStoreFloat4x4( &out.fit_matrix, m );
		(void) mesh_instances_loaded; (void) primitives_loaded; (void) orient;
		return true;
	}
}

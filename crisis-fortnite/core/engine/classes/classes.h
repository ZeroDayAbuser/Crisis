#pragma once
#define current_class reinterpret_cast<uint64_t>( this )

#define declare_member(type, name, offset) type name() { return bypass::read<type>( current_class + offset ); } 
#define declare_member_bit(Bit, Name, Offset) bool Name( ) { return bool( bypass::read<char>( current_class + Offset) & (1 << Bit)); }

#define apply_member(type, name, offset) void name( type val ) { bypass::write<type>( current_class + offset, val); }
#define apply_member_bit(Bit, Name, Offset) void Name( bool Value ) { bypass::write<char>( bypass::read<char>( current_class + Offset) | (1 << Bit), Value); }
#define apply_member_array(type, name, offset) void name(int index, type val) { bypass::write<type>(current_class + offset + (index * sizeof(type)), val); }

namespace engine {
	class f_text {
	public:
		declare_member( std::uint64_t, data, 0x20 );
		declare_member( std::int32_t, length, 0x28 );

		std::string get( ) {
			const auto& __ftext = current_class;
			if ( !__ftext ) return "";

			const auto& src = this->data( );
			if ( !src ) return "";

			const auto& size = this->length( );
			if ( size <= 0 || size > 100 ) return "";
			if ( size <= 64 ) {
				wchar_t stack_buffer[ 65 ] = { 0 };
				if ( !bypass::target::read_memory( src, stack_buffer, size * sizeof( wchar_t ) ) ) {
					return "";
				}

				try {
					std::wstring wstr( stack_buffer );
					std::string str( wstr.begin( ), wstr.end( ) );
					return str;
				}
				catch ( ... ) {
					return "";
				}
			}

			std::unique_ptr<wchar_t[ ]> dst( new( std::nothrow ) wchar_t[ size + 1 ] );
			if ( !dst ) return "";

			std::memset( dst.get( ), 0, ( size + 1 ) * sizeof( wchar_t ) );

			if ( !bypass::target::read_memory( src, dst.get( ), size * sizeof( wchar_t ) ) ) {
				return "";
			}

			try {
				std::wstring wstr( dst.get( ) );
				std::string str( wstr.begin( ), wstr.end( ) );

				if ( !str.empty( ) ) {
					str.erase( str.begin( ), std::find_if( str.begin( ), str.end( ), [ ]( unsigned char ch ) {
						return !std::isspace( ch );
						} ) );

					str.erase( std::find_if( str.rbegin( ), str.rend( ), [ ]( unsigned char ch ) {
						return !std::isspace( ch ) && ch != '\0';
						} ).base( ), str.end( ) );
				}

				return str;
			}
			catch ( ... ) {
				return "";
			}
		}
	};

	// Forward declaration — u_class is defined below u_struct.
	class u_class;

	class u_object {
	public:
		// UObject::ClassPrivate — the UClass* every UObject carries at
		// +0x08 in this Fortnite build. Used by exploits that need to
		// reach a class's CDO. Routed through the SDK per project
		// convention rather than an inline raw read at the call site.
		declare_member( u_class*, class_private, offsets::class_private );

		// https://github.com/EpicGames/UnrealEngine/blob/5.8/Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectBaseUtility.h#L231
		// Add an object to the root set. This prevents the object and all its descendants from being deleted during garbage collection.
		void add_to_root( ) {
			constexpr std::uint32_t rf_root_set = 0x04;
			const auto addr = reinterpret_cast< std::uint64_t >( this ) + 0x08;
			bypass::write< std::int32_t >( addr,
				bypass::read< std::int32_t >( addr ) | rf_root_set );
		}

		// ── Unreal function-calling surface ────────────────────────────
		// [nexus removed 2026-09-01] This block used to route every
		// StaticFindObject / StaticLoadObject / ProcessEvent through
		// ::engine::nexus_bridge::call<>, which manual-mapped Fortnite
		// into our address space and ran the game's own function bodies
		// locally against fault-in'd pages. Nexus is gone; see
		// core/engine/nexus_bridge.hxx for the rationale and the
		// kernel-APC follow-up.
		//
		// The templates below are kept as SIGNATURE-COMPATIBLE no-op
		// stubs so every downstream call site in this header (dozens of
		// static_auto = u_object::find_object< ... >( L"..." ) helpers
		// under KismetStringLibrary / MaterialInstanceDynamic /
		// PrimitiveComponent / SkeletalMeshComponent / etc.) keeps
		// compiling. At runtime they always return null / no-op —
		// downstream features that depended on live UE dispatch (dynamic
		// materials, bone-space queries, string conversions, etc.)
		// silently do nothing until the kernel-APC path is wired up.
		//
		// f_string_view is preserved because it's a stable UE6 data
		// layout, cheap, and useful for any future re-introduction of
		// a working call path.
		struct f_string_view {
			const wchar_t* ptr    { nullptr };
			std::int32_t   length { 0 };

			f_string_view( ) = default;
			f_string_view( const wchar_t* str, std::int32_t len )
				: ptr( str ), length( len ) { }
			explicit f_string_view( const wchar_t* str ) : ptr( str ) {
				if ( !str ) return;
				while ( str[ length ] != L'\0' ) ++length;
			}
		};

		// Resolve the target-side VA of UObject::StaticFindObject once the
		// bypass has discovered the game's base address. Returns 0 while
		// setup hasn't run yet - callers gate on the return being non-zero.
		static std::uint64_t static_find_object_addr( ) {
			if ( !bypass::target::m_base_address )
				return 0;
			return bypass::target::m_base_address + offsets::static_find_object;
		}

		// Call the target's own UObject::StaticFindObject through the
		// bypass caller. Signature on this Fortnite build (UE 5.x with
		// FStringView-based lookups):
		//   __fastcall StaticFindObject( UClass* Class, UObject* Outer,
		//                                FStringView* Name, bool ExactClass )
		//
		// We build the FStringView + backing wide-char buffer inside a
		// single blob and hand it to the bypass as stack_params. The blob
		// layout matches Plu::UE::FStringView followed by the chars:
		//   +0x00  ptr        (placeholder → chars offset; bypass rewrites)
		//   +0x08  length     (int32_t)
		//   +0x0C  pad
		//   +0x10  wide chars (name + null)
		//
		// call_function receives:
		//   a1 = Class      (UClass*, nullable → ANY_CLASS)
		//   a2 = Outer      (UObject*)
		//   a3 = FStringView* — passed as stack_param_base + 0 placeholder,
		//                       bypass rewrites to (params_va + 0) so the
		//                       game sees a real pointer into its own space.
		//   a4 = ExactClass (bool)
		//
		// Returns whatever StaticFindObject returned in rax (a UObject*),
		// reinterpreted to the caller's requested T.
		template< typename T = u_object* >
		static T find_object( const wchar_t* name, u_object* outer = nullptr,
			bool exact = false, u_object* klass = nullptr )
		{
			if ( !name )
				return T {};

			const auto sfo_va = static_find_object_addr( );
			if ( !sfo_va )
				return T {};

			constexpr std::size_t k_chars_off = 0x10;
			constexpr std::size_t k_chars_cap = 200;

			std::uint8_t blob[ k_chars_off + k_chars_cap * sizeof( wchar_t ) ] { };

			// Wide-string length (bounded, non-fatal if name is longer).
			std::size_t len = 0;
			while ( name[ len ] != L'\0' && len < k_chars_cap - 1 )
				++len;
			std::memcpy( blob + k_chars_off, name,
				( len + 1 ) * sizeof( wchar_t ) );

			// FStringView.ptr — placeholder pointing at our own chars
			// section; bypass rewrites this in the copied blob so the
			// game reads a valid target-side pointer.
			const std::uint64_t chars_placeholder =
				bypass::target::stack_param_base + k_chars_off;
			std::memcpy( blob + 0x00, &chars_placeholder,
				sizeof( chars_placeholder ) );

			// FStringView.length (int32).
			const std::int32_t length_i32 = static_cast< std::int32_t >( len );
			std::memcpy( blob + 0x08, &length_i32, sizeof( length_i32 ) );

			// FStringView* arg itself: placeholder for blob offset 0.
			const std::uint64_t fsv_arg = bypass::target::stack_param_base;

			const auto ret = bypass::target::call_function(
				sfo_va,
				reinterpret_cast< std::uint64_t >( klass ),   // Class (ANY_CLASS = 0)
				reinterpret_cast< std::uint64_t >( outer ),   // Outer
				fsv_arg,                                       // FStringView*
				exact ? 1ull : 0ull,                           // ExactClass
				0, 0, 0, 0, 0, 0,
				blob, sizeof( blob ) );

			return reinterpret_cast< T >( ret );
		}

		template< typename T = u_object, std::size_t N >
		static T* static_find_object( const wchar_t( &name )[ N ],
			u_object* outer = nullptr, u_object* klass = nullptr,
			bool exact = false )
		{
			return find_object< T* >( name, outer, exact, klass );
		}

		template< typename T = u_object, std::size_t N >
		static T* static_load_object( const wchar_t( & /*name*/ )[ N ], u_object* /*outer*/ = nullptr, u_object* /*klass*/ = nullptr, const wchar_t* /*filename*/ = nullptr, uint32_t /*load_flags*/ = 0 ) {
			// [nexus removed] used to nexus_bridge::call StaticLoadObject.
			return nullptr;
		}

		template< typename T = u_object* >
		static T load_object( const wchar_t* /*name*/, u_object* /*outer*/ = nullptr, u_object* /*klass*/ = nullptr, const wchar_t* /*filename*/ = nullptr, uint32_t /*load_flags*/ = 0 ) {
			// [nexus removed] used to nexus_bridge::call StaticLoadObject.
			return T {};
		}

		template< typename params_t >
		static void process_event( u_object* object, u_object* function, params_t& params ) {
			if ( !object || !function || !bypass::target::m_base_address )
				return;
			const auto obj_va  = reinterpret_cast< std::uint64_t >( object );
			const auto func_va = reinterpret_cast< std::uint64_t >( function );
			if ( !utility::is_valid_pointer( obj_va )  || !bypass::target::translate_linear( obj_va  ) ) return;
			if ( !utility::is_valid_pointer( func_va ) || !bypass::target::translate_linear( func_va ) ) return;
			bypass::target::call_function(
				bypass::target::m_base_address + offsets::process_event,
				obj_va,
				func_va,
				bypass::target::stack_param_base,
				0, 0, 0, 0, 0, 0, 0,
				&params, sizeof( params ) );
		}

		template< typename params_t >
		void process_event( u_object* function, params_t& params ) {
			u_object::process_event( reinterpret_cast< u_object* >( this ), function, params );
		}

		void process_event( u_object* function ) {
			u_object::process_event( reinterpret_cast< u_object* >( this ), function );
		}
	};

	class u_field : public u_object {
	public:
		declare_member( u_field*, next, 0x28 );
	};

	class u_struct : public u_field {
	public:
		declare_member( u_struct*, super_struct, 0x40 );
	};

	class u_script_struct : public u_struct {
	public:
	};

	// UClass — carries the Class Default Object (CDO) at +0x108 in this
	// Fortnite build. Reached from any UObject via u_object::class_private().
	class u_class : public u_struct {
	public:
		declare_member( u_object*, default_object, offsets::class_default_object );
	};

	// USkeletalMesh — the ASSET pointed at by SkeletalMeshComponent's
	// SkinnedAsset / SkeletalMesh property. Its FReferenceSkeleton's
	// FinalRefBonePose is a TArray<FTransform> of the ref pose per bone.
	// Used by exploits::magic_bullet to scale specific bones.
	class u_skeletal_mesh : public u_object {
	public:
		declare_member( t_array<f_transform>, final_ref_bone_pose,
			offsets::skeletal_mesh_final_ref_bone_pose );
	};

	class u_data_table : public u_object {
	public:
		typedef t_map<f_name, uint8_t*> row_map_type;

		declare_member( u_script_struct*, row_struct, 0x28 );
		declare_member( row_map_type, row_map, 0x30 );

		template <typename type>
		type* find_row( const std::string& row_name ) {
			auto row_map = this->row_map( );
			auto row_array = row_map.pairs.sparse_array;
			auto data_pairs = row_array.get_itter( );

			for ( const auto& data_pair : data_pairs ) {
				auto curr_row_name = emulator::fname_to_string( data_pair.first );
				if ( curr_row_name.find( row_name ) != std::string::npos ) {
					return reinterpret_cast< type* >( data_pair.second );
				}
			}

			return nullptr;
		}
	};

	struct f_data_table_row_handle {
		u_data_table* data_table;
		f_name row_name;
		uint8_t pad_c[ 0x4 ];
	};

	class u_material_interface : public u_object {

	};

	class u_material_instance : public u_material_interface {
	public:
		declare_member( t_array<f_scalar_parameter_value>, scalar_parameter_values, 0x1e8 );
		declare_member( t_array<f_vector_parameter_value>, vector_parameter_values, 0x1f8 );

		apply_member( t_array<f_scalar_parameter_value>, set_scalar_parameter_values, 0x1e8 );
		apply_member( t_array<f_vector_parameter_value>, set_vector_parameter_values, 0x1f8 );

		float get_scalar_parameter_value( f_name parameter_name, float default_value = 0.0f ) {
			auto values = scalar_parameter_values( );

			for ( auto i = 0; i < values.count; i++ ) {
				auto entry = values.get( i );

				if ( entry.parameter_info.name == parameter_name ) {
					return entry.parameter_value;
				}
			}

			return default_value;
		}

		flinear_color get_vector_parameter_value( f_name parameter_name, flinear_color default_value = { } ) {
			auto values = vector_parameter_values( );

			for ( auto i = 0; i < values.count; i++ ) {
				auto entry = values.get( i );

				if ( entry.parameter_info.name == parameter_name ) {
					return entry.parameter_value;
				}
			}

			return default_value;
		}

		void set_scalar_parameter_value( f_name parameter_name, float new_value ) {
			auto values = scalar_parameter_values( );

			for ( auto i = 0; i < values.count; i++ ) {
				auto entry = values.get( i );

				if ( entry.parameter_info.name == parameter_name ) {
					entry.parameter_value = new_value;
					values.set( i, entry );
					break;
				}
			}
		}

		void set_vector_parameter_value( f_name parameter_name, flinear_color new_value ) {
			auto values = vector_parameter_values( );

			for ( auto i = 0; i < values.count; i++ ) {
				auto entry = values.get( i );

				if ( entry.parameter_info.name == parameter_name ) {
					entry.parameter_value = new_value;
					values.set( i, entry );
					break;
				}
			}
		}

		float get_scalar_parameter_value( const fguid& guid, float default_value = 0.0f ) {
			auto values = scalar_parameter_values( );

			for ( auto i = 0; i < values.count; i++ ) {
				auto entry = values.get( i );

				if ( entry.expression_guid == guid ) {
					return entry.parameter_value;
				}
			}

			return default_value;
		}

		flinear_color get_vector_parameter_value( const fguid& guid, flinear_color default_value = { } ) {
			auto values = vector_parameter_values( );

			for ( auto i = 0; i < values.count; i++ ) {
				auto entry = values.get( i );

				if ( entry.expression_guid == guid ) {
					return entry.parameter_value;
				}
			}

			return default_value;
		}

		void set_scalar_parameter_value( const fguid& guid, float new_value ) {
			auto values = scalar_parameter_values( );

			for ( auto i = 0; i < values.count; i++ ) {
				auto entry = values.get( i );

				if ( entry.expression_guid == guid && entry.parameter_value != new_value ) {
					entry.parameter_value = new_value;
					values.set( i, entry );
					break;
				}
			}
		}

		void set_vector_parameter_value( const fguid& guid, flinear_color new_value ) {
			auto values = vector_parameter_values( );

			for ( auto i = 0; i < values.count; i++ ) {
				auto entry = values.get( i );

				if ( entry.expression_guid == guid && entry.parameter_value != new_value ) {
					entry.parameter_value = new_value;
					values.set( i, entry );
					break;
				}
			}
		}
	};

	class u_material_instance_dynamic : public u_material_instance {
	public:
		void k2_set_vector_parameter_value( f_name parameter_name, flinear_color value ) {
			static auto function = u_object::find_object<u_object*>( L"MaterialInstanceDynamic.SetVectorParameterValue" );
			if ( !function )
				return;

			struct { f_name parameter_name; flinear_color value; } params { parameter_name, value };
			this->process_event( function, params );
		}

		flinear_color k2_get_vector_parameter_value( f_name parameter_name ) {
			static auto function = u_object::find_object<u_object*>( L"MaterialInstanceDynamic.K2_GetVectorParameterValue" );
			if ( !function )
				return {};

			struct { f_name parameter_name; flinear_color return_value; } params { parameter_name, {} };
			this->process_event( function, params );
			return params.return_value;
		}

		void k2_set_scalar_parameter_value( f_name parameter_name, float value ) {
			static auto function = u_object::find_object<u_object*>( L"MaterialInstanceDynamic.SetScalarParameterValue" );
			if ( !function )
				return;

			struct { f_name parameter_name; float value; } params { parameter_name, value };
			this->process_event( function, params );
		}

		float k2_get_scalar_parameter_value( f_name parameter_name ) {
			static auto function = u_object::find_object<u_object*>( L"MaterialInstanceDynamic.K2_GetScalarParameterValue" );
			if ( !function )
				return 0.0f;

			struct { f_name parameter_name; float return_value; } params { parameter_name, 0.0f };
			this->process_event( function, params );
			return params.return_value;
		}
	};

	class u_scene_component : public u_object {
	public:
		declare_member( f_vector, relative_location, offsets::relative_location );
		declare_member( f_vector, component_velocity, offsets::component_velocity );
		declare_member( f_transform, component_to_world, offsets::component_to_world );
		apply_member_bit( 3, b_hidden_in_game, 0x1A1 );

		void set_custom_depth_stencil_value( int value ) {
			static auto function = u_object::find_object<u_object*>( L"PrimitiveComponent.SetCustomDepthStencilValue" );
			if ( !function ) return;
			struct { int value; } params { value };
			this->process_event( function, params );
		}

		void set_render_custom_depth( bool value ) {
			static auto function = u_object::find_object<u_object*>( L"PrimitiveComponent.SetRenderCustomDepth" );
			if ( !function ) return;
			struct { bool value; } params { value };
			this->process_event( function, params );
		}

		u_material_instance_dynamic* create_dynamic_material_instance( std::int32_t element_index, u_material_interface* parent, f_name optional_name ) {
			static auto function = u_object::find_object<u_object*>( L"PrimitiveComponent.CreateDynamicMaterialInstance" );
			if ( !function ) return nullptr;
			struct {
				std::int32_t element_index;
				u_material_interface* parent;
				f_name optional_name;
				u_material_instance_dynamic* return_value;
			} params { element_index, parent, optional_name, nullptr };
			this->process_event( function, params );
			return params.return_value;
		}

		void set_material( std::int32_t element_index, u_material_interface* material ) {
			static auto function = u_object::find_object<u_object*>( L"PrimitiveComponent.SetMaterial" );
			if ( !function ) return;
			struct { std::int32_t element_index; u_material_interface* material; } params { element_index, material };
			this->process_event( function, params );
		}

		u_material_interface* get_material( std::int32_t element_index ) {
			static auto function = u_object::find_object<u_object*>( L"PrimitiveComponent.GetMaterial" );
			if ( !function ) return nullptr;
			struct { std::int32_t element_index; u_material_interface* return_value; } params { element_index, nullptr };
			this->process_event( function, params );
			return params.return_value;
		}

		t_array<u_material_interface*> get_materials( ) {
			static auto function = u_object::find_object<u_object*>( L"MeshComponent.GetMaterials" );
			if ( !function ) return {};
			struct { t_array<u_material_interface*> return_value; } params {};
			this->process_event( function, params );
			return params.return_value;
		}
	};

	class a_actor : public u_object {
	public:
		declare_member( u_scene_component*, root_component, offsets::root_component );
		declare_member( float, revive_from_dbno_time, offsets::revive_from_dbno_time );
		declare_member( float, simulating_too_long_length, offsets::simulating_too_long_length );
		declare_member( int, max_level, offsets::max_level );
		declare_member( f_text*, search_text, offsets::search_text );
		// LastDamagedTime — replaces a raw bypass::read at the health-esp
		// per-frame update site.
		declare_member( float, last_damaged_time, offsets::last_damaged_time );

		bool is_player( ) {
			auto revive_from_dbno_time = this->revive_from_dbno_time( );
			if ( revive_from_dbno_time != 10.f )
				return false;
			return true;
		}

		bool is_pickup( ) {
			auto simulating_too_long_length = this->simulating_too_long_length( );
			if ( simulating_too_long_length != 30.f )
				return false;
			return true;
		}

		bool is_weakspot( ) {
			auto max_level = this->max_level( );
			if ( max_level != 7 )
				return false;
			return true;
		}

		bool is_container( ) {
			auto search_text = this->search_text( )->get( );
			if ( search_text.contains( static_cast<const char*>( obf( "Search" ) ) ) )
				return true;
			return false;
		}
	};

	class u_material : public u_material_instance {
	public:
		apply_member( std::uint8_t, set_blend_mode, 0x189 );

		void set_b_disable_depth_test( bool value ) {
			auto flags = bypass::read<std::uint8_t>( current_class + 0x210 );
			flags = value ? static_cast< std::uint8_t >( flags | 0x01 )
				: static_cast< std::uint8_t >( flags & ~0x01 );
			bypass::write<std::uint8_t>( current_class + 0x210, flags );
		}

		void set_wireframe( bool value ) {
			auto flags = bypass::read<std::uint8_t>( current_class + 0x228 );
			flags = value ? static_cast< std::uint8_t >( flags | 0x80 )
				: static_cast< std::uint8_t >( flags & ~0x80 );
			bypass::write<std::uint8_t>( current_class + 0x228, flags );
		}
	};

	class u_primitive_component :public u_scene_component {
	public:
		u_material_interface* get_material( int32_t element_index ) {
			static auto function = u_object::find_object<u_object*>( L"PrimitiveComponent.GetMaterial" );
			if ( !function ) return nullptr;
			struct {
				int32_t element_index;
				u_material_interface* return_value;
			} params { element_index };
			this->process_event( function, params );
			return params.return_value;
		}
	};

	class u_mesh_component : public u_primitive_component {
	public:

	};

	class u_skinned_mesh_component : public u_mesh_component {
	public:
		declare_member( std::int32_t, is_cached, offsets::component_space_transforms_array + 0x48 );
		declare_member( t_array<f_transform>, component_space_transforms_array, offsets::component_space_transforms_array + ( this->is_cached( ) * 0x10 ) );
		declare_member( u_object*, skeletal_mesh, offsets::skinned_mesh_component_skeletal_mesh );
		// SkinnedAsset — newer UE property that supersedes the legacy
		// SkeletalMesh pointer on some builds. Callers prefer this and
		// fall back to skeletal_mesh() when null.
		declare_member( u_skeletal_mesh*, skinned_asset, offsets::skinned_asset );
		declare_member( t_array<u_object*>, materials, offsets::skinned_mesh_component_override_materials );

		static f_vector bone_world_location( f_transform component_to_world, f_transform bone ) {
			component_to_world.fix_scaler( );
			auto matrix = bone.to_matrix( ).to_multiplication(
				component_to_world.to_matrix( )
			);

			return matrix.w_plane;
		}

		f_vector get_bone_location( t_array < f_transform > bone_array, std::uint32_t bone_index ) {
			return bone_world_location( this->component_to_world( ), bone_array.get( bone_index ) );
		}

		f_rotator get_bone_rotation( t_array < f_transform > bone_array, std::uint32_t bone_index ) {
			auto component_to_world = this->component_to_world( );
			component_to_world.fix_scaler( );

			auto bone = bone_array.get( bone_index );
			auto matrix = bone.to_matrix( ).to_multiplication(
				component_to_world.to_matrix( )
			);

			float pitch, yaw, roll;
			pitch = atan2( -matrix.m[ 2 ][ 1 ], sqrt( matrix.m[ 0 ][ 1 ] * matrix.m[ 0 ][ 1 ] + matrix.m[ 1 ][ 1 ] * matrix.m[ 1 ][ 1 ] ) );
			yaw = atan2( matrix.m[ 0 ][ 1 ], matrix.m[ 1 ][ 1 ] );
			roll = atan2( matrix.m[ 2 ][ 0 ], matrix.m[ 2 ][ 2 ] );

			pitch = pitch * ( 180.0f / std::numbers::pi );
			yaw = yaw * ( 180.0f / std::numbers::pi );
			roll = roll * ( 180.0f / std::numbers::pi );

			return f_rotator( pitch, yaw, roll );
		}
	};

	class u_skeletal_mesh_component : public u_skinned_mesh_component {
	public:
		f_vector get_bone_location( f_name bone_name ) {
			if ( !bypass::read< std::uint64_t >( reinterpret_cast< std::uint64_t >( this ) ) )
				return {};
			static u_object* function = nullptr;
			if ( function ) {
				const auto fva = reinterpret_cast< std::uint64_t >( function );
				const auto idx = bypass::read< std::int32_t >( fva + 0x0C );
				if ( !bypass::read< std::uint64_t >( fva ) || idx < 0 || idx >= 4194304 )
					function = nullptr;
			}
			if ( !function )
				function = u_object::find_object< u_object* >( obf( L"SkeletalMeshComponent.GetBoneLocation" ) );
			if ( !function ) return {};
			struct { f_name bone; f_vector return_value; } params { bone_name, {} };
			this->process_event( function, params );
			return params.return_value;
		}

	};

	class u_fort_player_state_component_habanero : public a_actor {
	public:
		declare_member( int32_t, rank_progress, offsets::rank_progress );
		declare_member( int32_t, highest_rank, offsets::highest_rank );
		declare_member( int32_t, current_mmr, offsets::current_mmr );
		declare_member( int32_t, current_position_rank, offsets::current_position_rank );
	};

	class a_player_state : public a_actor {
	public:
		declare_member( u_fort_player_state_component_habanero*, habanero_component, offsets::habanero_component );
		declare_member( std::uint8_t, team_index, offsets::team_index );
		declare_member( std::int32_t, kill_score, offsets::kill_score );

		std::string get_platform_name( ) {
			const auto& state = current_class;
			if ( !state ) return "";

			int size = bypass::read<int>( current_class + offsets::platform + 0x8 );
			if ( size <= 0 || size > 50 ) return "";

			std::uint64_t src = bypass::read( current_class + offsets::platform );
			if ( !src ) return "";

			if ( size <= 32 ) {
				wchar_t stack_buffer[ 33 ] = { 0 };
				if ( !bypass::target::read_memory( src, stack_buffer, size * sizeof( wchar_t ) ) ) {
					return "";
				}

				try {
					std::wstring conversion( stack_buffer );
					return std::string( conversion.begin( ), conversion.end( ) );
				}
				catch ( ... ) {
					return "";
				}
			}

			std::unique_ptr<wchar_t[ ]> dst( new ( std::nothrow ) wchar_t[ size + 1 ] );
			if ( !dst ) return "";

			if ( !bypass::target::read_memory( src, dst.get( ), size * sizeof( wchar_t ) ) ) {
				return "";
			}

			try {
				dst[ size ] = L'\0';

				std::wstring conversion( dst.get( ) );
				return std::string( conversion.begin( ), conversion.end( ) );
			}
			catch ( ... ) {
				return "";
			}
		}

		std::string get_player_name( ) {
			auto name_addr = bypass::read( current_class + offsets::player_name );
			if ( !name_addr )
				return static_cast<const char*>( obf( "Robot" ) );

			auto length = bypass::read<std::uint32_t>( name_addr + 0x10 );
			auto data = bypass::read<std::uint64_t>( name_addr + 0x8 );
			if ( !data )
				return static_cast<const char*>( obf( "Robot" ) );

			auto decrypt = [ ]( wchar_t* buf, int64_t v6 ) -> std::string {
				try {
					char v21 = ( char ) ( v6 - 1 ); if ( !( WORD ) v6 ) v21 = 0;
					int v22 = 0; WORD* v23 = ( WORD* ) buf;
					for ( int i = ( int ) ( v21 & 3 ); ; *v23++ += i & 7 ) {
						int v25 = ( int ) ( v6 - 1 ); if ( !( WORD ) v6 ) v25 = 0;
						if ( v22 >= v25 ) break;
						i += 3; ++v22;
					}
					std::wstring w( buf ); return std::string( w.begin( ), w.end( ) );
				}
				catch ( ... ) {
					return static_cast<const char*>( obf( "Robot" ) );
				} };

			if ( length <= 32 ) {
				wchar_t stack_buf[ 33 ] = {};
				bypass::target::read_memory( data, stack_buf, length * sizeof( wchar_t ) );
				return decrypt( stack_buf, ( int64_t ) length );
			}

			std::unique_ptr<wchar_t[ ]> dst( new ( std::nothrow ) wchar_t[ length + 1 ] );
			if ( !dst )
				return static_cast<const char*>( obf( "Robot" ) );

			bypass::target::read_memory( data, dst.get( ), length * sizeof( wchar_t ) );
			dst[ length ] = L'\0';
			return decrypt( dst.get( ), ( int64_t ) length );
		}
	};

	class f_fort_ranged_weapon_stats {
	public:
		declare_member( float, spread, 0x1f8 );
		declare_member( int32_t, bullets_per_cartridge, 0x27c );
		declare_member( int32_t, min_hits_per_cartridge, 0x280 );
		declare_member( float, firing_rate, 0x284 );
		declare_member( float, recoil_vert, 0x2bc );
		declare_member( float, recoil_horz, 0x2cc );
		declare_member( float, offset_recoil_factor, 0x320 );
		declare_member( float, recoil_bounce_factor, 0x324 );
		declare_member( float, recoil_bounce_offset_factor, 0x328 );
		declare_member( float, projectile_velocity, 0x3c8 );
	};

	class f_fort_base_weapon_stats : public f_fort_ranged_weapon_stats {
	public:
		// ── Direct damage by range ───────────────────────────────────────
		declare_member( float, dmg_pb,            0x0010 );  // point-blank damage
		declare_member( float, dmg_mid,           0x0014 );  // mid-range damage
		declare_member( float, dmg_long,          0x0018 );  // long-range damage
		declare_member( float, dmg_max_range,     0x001C );  // at max range

		// ── Environmental damage by range ────────────────────────────────
		declare_member( float, env_dmg_pb,        0x0020 );
		declare_member( float, env_dmg_mid,       0x0024 );
		declare_member( float, env_dmg_long,      0x0028 );
		declare_member( float, env_dmg_max_range, 0x002C );

		// ── Damage zone multipliers ───────────────────────────────────────
		// Published values from Fortnite data tables (FortBaseWeaponStats):
		//   Normal (body)  ≈ 1.0   Critical (head) ≈ 2.0   Resistant (limbs) ≈ 0.75–1.0
		declare_member( float, dmg_zone_normal,    0x0030 );
		declare_member( float, dmg_zone_critical,  0x0034 );
		declare_member( float, dmg_zone_resistant, 0x0038 );

		// ── Range thresholds (cm) ────────────────────────────────────────
		declare_member( float, rng_pb,            0x0044 );  // point-blank cutoff
		declare_member( float, rng_mid,           0x0048 );  // mid-range cutoff
		declare_member( float, rng_long,          0x004C );  // long-range cutoff
		declare_member( float, rng_max,           0x0050 );  // max effective range

		// ── Timing / stun ────────────────────────────────────────────────
		declare_member( float, stun_time,  0x0108 );
		declare_member( float, stun_scale, 0x010C );

		// ── Magazine / ammo ─────────────────────────────────────────────
		declare_member( float,   reload_time,                   0x0174 );
		declare_member( int32_t, num_individual_bullets_to_reload, 0x0180 );
		declare_member( int32_t, clip_size,                     0x0184 );
		declare_member( int32_t, initial_clips,                 0x018C );
		declare_member( int32_t, cartridge_per_fire,            0x0194 );
		declare_member( int32_t, ammo_cost_per_fire,            0x0198 );
		declare_member( int32_t, max_ammo_cost_per_fire,        0x019C );
	};


	class u_fort_item_definition : public a_actor {
	public:
		//declare_member( e_fort_rarity, rarity, offsets::rarity );
		declare_member( f_text*, item_name, offsets::item_name );
	};

	class u_fort_world_item_definition : public u_fort_item_definition {
	public:
	};

	class u_fort_weapon_item_definition : public u_fort_world_item_definition {
	public:
		declare_member( f_data_table_row_handle, weapon_stat_handle, 0x128 );

		f_fort_base_weapon_stats* get_weapon_stats( ) {
			auto row_handle = this->weapon_stat_handle( );
			auto data_table = row_handle.data_table;
			if ( !data_table ) return nullptr;

			auto row_name = emulator::fname_to_string( row_handle.row_name );
			return data_table->find_row<f_fort_base_weapon_stats>( row_name );
		}
	};

	class a_fort_ranged_weapon : public a_actor {
	public:
	};

	class a_fort_weapon : public a_fort_ranged_weapon {
	public:
		declare_member( u_fort_weapon_item_definition*, weapon_data, offsets::weapon_data );
		declare_member( int, ammo_count, offsets::ammo_count );
		declare_member( std::uint8_t, weapon_core_animation, offsets::weapon_core_animation );
		declare_member( int, current_gun_fire_index, offsets::current_gun_fire_index );
		declare_member( float, current_projected_impact_distance, offsets::current_projected_impact_distance );
		// LastFireTime — replaces the raw bypass::read on weapon_actor at
		// the health-ESP heal-tracking sites.
		declare_member( float, last_fire_time, offsets::last_fire_time );

		f_vector get_muzzle_location( int pattern_index ) {
			if ( !bypass::read< std::uint64_t >( reinterpret_cast< std::uint64_t >( this ) ) )
				return {};

			static u_object* function = nullptr;
			if ( function ) {
				const auto fva = reinterpret_cast< std::uint64_t >( function );
				const auto idx = bypass::read< std::int32_t >( fva + 0x0C );
				if ( !bypass::read< std::uint64_t >( fva ) || idx < 0 || idx >= 4194304 )
					function = nullptr;
			}
			if ( !function )
				function = u_object::find_object< u_object* >( obf( L"FortWeapon.GetMuzzleLocation" ) );
			if ( !function )
				return {};

			struct { int pattern_index; f_vector return_value; } params { pattern_index };
			this->process_event( function, params );
			return params.return_value;
		}
	};

	class a_pawn : public a_actor {
	public:
		declare_member( a_player_state*, player_state, offsets::player_state );
	};

	class a_character : public a_pawn {
	public:
		declare_member( u_skeletal_mesh_component*, mesh, offsets::mesh );
	};

	class afgf_character : public a_character {
	public:
	};

	class a_fort_pawn : public afgf_character {
	public:
		declare_member( a_fort_weapon*, current_weapon, offsets::current_weapon );
		declare_member_bit( 5, b_is_dying, offsets::b_is_dying );
		declare_member_bit( 7, b_is_dbno, offsets::b_is_dbno );

		std::pair<bool, bool> dying_and_dbno( ) {
			const auto flags = bypass::read<char>( current_class + offsets::b_is_dying );
			return { bool( flags & ( 1 << 5 ) ), bool( flags & ( 1 << 7 ) ) };
		}
	};

	class a_building_sm_actor : public a_actor {
	public:
	};

	class a_building_container : public a_building_sm_actor {
	public:
	};

	class a_building_weakspot : public a_actor {
	public:
	};

	class f_fort_item_entry {
	public:
		declare_member( u_fort_item_definition*, item_definition, offsets::item_definition );
	};

	class a_fort_pickup : public a_actor {
	public:
		// PrimaryPickupItemEntry is an inline FortItemEntry (size 0x98), not a pointer —
		// return the remote address cast as a pointer so declare_member reads work.
		f_fort_item_entry* primary_pickup_item_entry( ) {
			return reinterpret_cast< f_fort_item_entry* >( current_class + offsets::primary_pickup_item_entry );
		}
	};

	class f_replicated_athena_vehicle_attributes {
	public:
	};

	class a_fort_player_pawn : public a_fort_pawn {
	public:
		declare_member( std::uint8_t, building_state, offsets::building_state );
		declare_member_bit( 1, b_is_in_safe_zone, offsets::b_is_in_safe_zone );
		declare_member( bool, b_is_skydiving, offsets::b_is_skydiving );
	};

	class a_fort_player_pawn_athena : public a_fort_player_pawn {
	public:
		declare_member( t_array<u_skeletal_mesh_component*>, skeletal_meshes, offsets::skeletal_meshes );
		declare_member( f_vector, last_fired_location, offsets::last_fired_location );
		declare_member( f_vector, last_fired_direction, offsets::last_fired_direction );
		declare_member( float,    last_fired_time,     offsets::last_fired_time );

	};

	class a_controller : public a_actor { };
	class a_player_controller : public a_controller {
	public:
		declare_member( a_fort_player_pawn_athena*, acknowledged_pawn, offsets::acknowledged_pawn );
		apply_member( f_rotator, rotation_input, 0x04B0 );
	};

	class u_player : public u_object {
	public:
		declare_member( a_player_controller*, player_controller, offsets::player_controller );
	};

	class u_fort_client_settings_record : public u_object {
	public:
		declare_member( float, mouse_sensitivity, offsets::mouse_sensitivity );
	};

	class u_localplayer : public u_player {
	public:
		declare_member( a_player_controller*, player_controller, offsets::player_controller );
		declare_member( u_fort_client_settings_record*, client_settings_record, offsets::client_settings_record );
		declare_member( t_array<f_scene_view_state_reference>, view_states, offsets::view_states );
	};

	class a_game_state_base {
	public:
		declare_member( int,             players_left,         offsets::players_left         );
		declare_member( a_player_state*, winning_player_state, offsets::winning_player_state );
		declare_member( int,             winning_team,         offsets::winning_team         );
	};

	class u_level {
	public:
		declare_member( t_array<a_actor*>, actor_clustor, offsets::actor_cluster );
	};

	class u_game_instance {
	public:
		declare_member( t_array<u_localplayer*>, localplayers, offsets::local_players );

		auto get_localplayer( ) -> u_localplayer* {
			return this->localplayers( ).get( 0 );
		}
	};

	class u_world;
	class u_game_viewport_client : public u_object {
	public:
		declare_member( u_world*, world, offsets::world );
	};

	class u_engine : public u_object {
	public:
		declare_member( u_game_viewport_client*, viewport_client, offsets::viewport_client );

		static u_engine* get( ) {
			return bypass::read< u_engine* >( bypass::target::m_base_address + offsets::gengine );
		}
	};

	class u_world : public u_object {
	public:
		declare_member( u_game_instance*, owning_game_instance, offsets::owning_game_instance );
		declare_member( a_game_state_base*, game_state, offsets::game_state );
		declare_member( t_array<u_level*>, levels, offsets::levels );

		void* get_data_layer_manager( ) {
			static auto fn = u_object::find_object< u_object* >( L"World.K2_GetWorldSettings" );
			if ( !fn ) return nullptr;
			struct { void* return_value; } params { };
			this->process_event( fn, params );
			return params.return_value;
		}

		static u_world* get_frontend( ) {
			static auto frontend_world = u_object::find_object< u_world* >( L"Frontend" );
			return reinterpret_cast< u_world* >( frontend_world );
		}

		static u_world* get( ) {
			auto gengine = u_engine::get( );
			if ( !gengine )
				return nullptr;

			auto viewport_client = gengine->viewport_client( );
			if ( !viewport_client )
				return nullptr;

			return viewport_client->world( );
		}
	};
}

#pragma once

namespace engine {
	class u_fort_kismet_library : public u_object {
	public:
		bool check_line_of_sight_to_actor_with_channel( f_vector source_pos, a_actor* target, e_collision_channel target_filter_channel, a_actor* source ) {
			const auto valid_actor = []( const a_actor* a ) -> bool {
				if ( !a ) return false;
				const auto va = reinterpret_cast< std::uint64_t >( a );
				if ( va < 0x10000 || va > 0x7FFFFFFFFFFull ) return false;
				if ( !bypass::target::translate_linear( va ) ) return false;
				const auto sub_ptr = bypass::read< std::uint64_t >( va + 0x08 );
				if ( sub_ptr < 0x10000 || sub_ptr > 0x7FFFFFFFFFFull ) return false;
				return bypass::target::translate_linear( sub_ptr ) != 0;
			};
			if ( !valid_actor( reinterpret_cast< a_actor* >( this ) ) )
				return false;
			if ( !valid_actor( target ) )
				return false;
			if ( source && !valid_actor( source ) )
				return false;

			static u_object* function = nullptr;
			if ( function ) {
				const auto fva = reinterpret_cast< std::uint64_t >( function );
				const auto idx = bypass::read< std::int32_t >( fva + 0x0C );
				if ( !bypass::read< std::uint64_t >( fva ) || idx < 0 || idx >= 4194304 )
					function = nullptr;
			}
			if ( !function )
				function = u_object::find_object< u_object* >( obf( L"FortKismetLibrary.CheckLineOfSightToActorWithChannel" ) );
			if ( !function )
				return false;

			struct {
				f_vector source_pos;
				a_actor* target;
				e_collision_channel target_filter_channel;
				a_actor* source;
				bool return_value;
			} params { source_pos, target, target_filter_channel, source };

			this->process_event( function, params );
			return params.return_value;
		}
	};


	class u_gameplay_statics : public u_object {
	public:
		t_array<u_object*> get_all_actors_of_class( u_object* world_object, u_object* actor_class ) {
			static auto function = u_object::find_object<u_object*>( L"GameplayStatics.GetAllActorsOfClass" );
			if ( !function )
				return {};

			struct {
				u_object* world_context_object;
				u_object* actor_class;
				t_array<u_object*> out_actors;
			} params { world_object, actor_class, {} };

			this->process_event( function, params );
			return params.out_actors;
		}
	};

	class u_kismet_string_library : public u_object {
	public:
		f_string conv_int_to_string( std::int32_t value ) {
			static auto function = u_object::find_object<u_object*>( L"KismetStringLibrary.Conv_IntToString" );
			if ( !function )
				return {};

			struct {
				std::int32_t in_int;
				f_string return_value;
			} params { value, {} };

			this->process_event( function, params );
			return params.return_value;
		}

		bool contains( f_string search_in, f_string substring, bool use_case, bool search_from_end ) {
			static auto function = u_object::find_object<u_object*>( L"KismetStringLibrary.Contains" );
			if ( !function )
				return false;

			struct {
				f_string search_in;
				f_string substring;
				bool use_case;
				bool search_from_end;
				bool return_value;
			} params { search_in, substring, use_case, search_from_end, false };

			this->process_event( function, params );
			return params.return_value;
		}

		// Keep the string storage in the ProcessEvent parameter block so the
		// pointer remains valid after that block is copied to the target process.
		template< std::size_t N >
		f_name conv_literal_to_name( const wchar_t( &value )[ N ] ) {
			static auto function = u_object::find_object<u_object*>( L"KismetStringLibrary.Conv_StringToName" );
			if ( !function )
				return {};

			struct params_t {
				f_string in_string {};
				f_name return_value {};
				wchar_t chars[ N ] {};
			} params {};

			const auto chars_offset = reinterpret_cast< std::uintptr_t >( params.chars ) -
				reinterpret_cast< std::uintptr_t >( &params );
			params.in_string.data = reinterpret_cast< wchar_t* >(
				bypass::target::stack_param_base + chars_offset );
			params.in_string.count = static_cast< std::int32_t >( N );
			params.in_string.max_count = static_cast< std::int32_t >( N );
			std::memcpy( params.chars, value, sizeof( params.chars ) );

			this->process_event( function, params );
			return params.return_value;
		}

		f_name conv_wstring_to_name( const wchar_t* value ) {
			if ( !value )
				return {};

			static auto function = u_object::find_object<u_object*>( L"KismetStringLibrary.Conv_StringToName" );
			if ( !function )
				return {};

			constexpr std::size_t max_chars = 128;
			const auto length = std::wcslen( value );
			if ( !length || length >= max_chars )
				return {};

			struct params_t {
				f_string in_string {};
				f_name return_value {};
				wchar_t chars[ max_chars ] {};
			} params {};

			const auto chars_offset = reinterpret_cast< std::uintptr_t >( params.chars ) -
				reinterpret_cast< std::uintptr_t >( &params );
			params.in_string.data = reinterpret_cast< wchar_t* >(
				bypass::target::stack_param_base + chars_offset );
			params.in_string.count = static_cast< std::int32_t >( length + 1 );
			params.in_string.max_count = static_cast< std::int32_t >( max_chars );
			std::memcpy( params.chars, value, ( length + 1 ) * sizeof( wchar_t ) );

			this->process_event( function, params );
			return params.return_value;
		}
		f_string conv_name_to_string( f_name value ) {
			static auto function = u_object::find_object<u_object*>( L"KismetStringLibrary.Conv_NameToString" );
			if ( !function )
				return {};

			struct {
				f_name in_name;
				f_string return_value;
			} params { value, {} };

			this->process_event( function, params );
			return params.return_value;
		}

		std::wstring conv_name_to_wstring( f_name value ) {
			static auto function = u_object::find_object<u_object*>( L"KismetStringLibrary.Conv_NameToString" );
			if ( !function )
				return {};

			struct {
				f_name in_name;
				f_string return_value;
			} params { value, {} };

			this->process_event( function, params );

			const auto count = params.return_value.count;
			if ( !params.return_value.data || count <= 1 )
				return {};

			constexpr std::int32_t max_chars = 256;
			if ( count > max_chars )
				return {};

			wchar_t buffer[ max_chars ] { };
			if ( !bypass::target::read_memory(
				reinterpret_cast< std::uint64_t >( params.return_value.data ),
				buffer,
				static_cast< std::size_t >( count ) * sizeof( wchar_t ) ) )
				return {};

			std::size_t length = 0;
			while ( length + 1 < static_cast< std::size_t >( count ) && buffer[ length ] )
				++length;

			return std::wstring( buffer, length );
		}

		f_string build_string_int( f_string append_to, f_string prefix, std::int32_t value, f_string suffix ) {
			static auto function = u_object::find_object<u_object*>( L"KismetStringLibrary.BuildString_Int" );
			if ( !function )
				return {};

			struct {
				f_string append_to;
				f_string prefix;
				std::int32_t in_int;
				f_string suffix;
				f_string return_value;
			} params { append_to, prefix, value, suffix, {} };

			this->process_event( function, params );
			return params.return_value;
		}

		f_string build_string_double( f_string append_to, f_string prefix, double value, f_string suffix ) {
			static auto function = u_object::find_object<u_object*>( L"KismetStringLibrary.BuildString_Double" );
			if ( !function )
				return {};

			struct {
				f_string append_to;
				f_string prefix;
				double in_double;
				f_string suffix;
				f_string return_value;
			} params { append_to, prefix, value, suffix, {} };

			this->process_event( function, params );
			return params.return_value;
		}
	};
}
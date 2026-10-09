#pragma once
#include <core/injector/thread/thread.hxx>

namespace injector {

	struct reloc_entry_t {
		std::uint32_t m_to_rva;
		std::uint32_t m_size;
		struct {
			std::int16_t m_offset : 0xc;
			std::int16_t m_type : 0x4;
		} m_item[ 0x1 ];
		std::int8_t m_pad0[ 0x2 ];
	};

	struct rop_gadget_t {
		const char*   m_name;
		const char*   m_pattern;
		std::uint8_t  m_bytes[ 8 ];
		std::uint8_t  m_size;
		std::uint64_t m_address {};
	};

	static std::unordered_map<std::wstring, std::uint64_t> m_mapped_modules;
	static std::vector< std::uint64_t > m_allocated_modules;

	class c_injector {
	public:
		c_injector( std::shared_ptr< image::c_image > image, bool is_system = false, bool use_context = false )
			: m_image( image ), m_is_system_dependency( is_system ), m_use_context( use_context ) { }

		bool prepare( ) {
			if ( !m_is_system_dependency )
				logging::print( obf( "manually mapping %ls into process" ), m_image->get_file_name( ).c_str( ) );

			if ( !m_is_system_dependency ) {
				m_image->add_section( ".exec", sizeof( thread::exec_context_t ) );
				m_image->add_section( ".ctx",  sizeof( thread::thread_context_t ) );
			}

			this->m_image_size = m_image->size_of_image( );
			this->m_image_address = virtual_allocate( m_image_size );
			if ( !m_image_address )
				return false;

			if ( m_is_system_dependency ) {
				logging::print( obf( "preparing system allocation -> 0x%llx ( 0x%x )" ),
					m_image_address, m_image_size
				);
			}
			else {
				logging::print( obf( "prepared image allocation -> 0x%llx ( 0x%x )" ),
					m_image_address, m_image_size
				);
			}

			if ( !resolve_relocations( ) )
				return false;

			if ( !resolve_imports( ) )
				return false;

			if ( !resolve_sections( ) )
				return false;

			return true;
		}
		bool execute( ) {
			if ( !m_image_address )
				return false;

			const auto dll_base    = m_image_address;
			const auto entry_point = dll_base + m_image->get_entry_point( );

			auto exec_section = m_image->find_section( ".exec" );
			if ( !exec_section ) {
				logging::print( obf( "could not find .exec section" ) );
				return false;
			}

			const auto exec_fort_va = dll_base + exec_section->m_virtual_address;

			std::uint64_t first_arg = dll_base;
			if ( m_use_context ) {
				auto ctx_section = m_image->find_section( ".ctx" );
				if ( !ctx_section ) {
					logging::print( obf( "could not find .ctx section" ) );
					return false;
				}

				const auto ctx_va = dll_base + ctx_section->m_virtual_address;

				struct { std::uint64_t m_game_base; std::uint64_t m_image_base; std::uint32_t m_image_size; } ctx_data {};
				ctx_data.m_game_base  = bypass::target::m_base_address;
				ctx_data.m_image_base = dll_base;
				ctx_data.m_image_size = m_image_size;
				bypass::target::write_memory( ctx_va, &ctx_data, sizeof( ctx_data ) );

				first_arg = ctx_va;
			}

			logging::print( obf( "calling entry ( 0x%llx ) base ( 0x%llx )" ), entry_point, dll_base );

			thread::exec_context_t zero_ctx {};
			bypass::target::write_memory( exec_fort_va, &zero_ctx, sizeof( zero_ctx ) );

			std::uint32_t tid    = 0;
			void*         ethread = nullptr;
			CONTEXT       ctx    {};

			for ( const auto& [ t_tid, t_ethread ] : bypass::target::enumerate_threads( ) ) {
				if ( !bypass::target::suspend_thread( t_ethread ) )
					continue;

				CONTEXT t_ctx {};
				if ( !bypass::target::get_context_thread( t_ethread, &t_ctx ) ) {
					bypass::target::resume_thread( t_ethread );
					continue;
				}

				if ( t_ctx.Rip >= 0x7ffe00000000ULL ) {
					bypass::target::resume_thread( t_ethread );
					continue;
				}

				tid     = t_tid;
				ethread = t_ethread;
				ctx     = t_ctx;
				break;
			}

			if ( !ethread ) {
				logging::print( obf( "no suitable injection thread found" ) );
				return false;
			}

			logging::print( obf( "injection thread: tid %u" ), tid );

			CONTEXT original_ctx = ctx;

			auto shellcode = thread::build_shellcode( entry_point, first_arg, exec_fort_va );
			const auto sc_addr = virtual_allocate( shellcode.size( ) );
			if ( !sc_addr ) {
				bypass::target::resume_thread( ethread );
				return false;
			}

			bypass::target::write_memory( sc_addr, shellcode.data( ), shellcode.size( ) );
			ctx.Rip = sc_addr;

			if ( !bypass::target::set_context_thread( ethread, &ctx ) ) {
				virtual_free( sc_addr );
				bypass::target::resume_thread( ethread );
				return false;
			}

			bypass::target::resume_thread( ethread );

			thread::exec_context_t exec_ctx {};
			const auto deadline = GetTickCount64( ) + 5000;
			while ( GetTickCount64( ) < deadline ) {
				bypass::target::read_memory( exec_fort_va, &exec_ctx, sizeof( exec_ctx ) );
				if ( exec_ctx.m_status == thread::status_complete )
					break;
				Sleep( 1 );
			}

			if ( bypass::target::suspend_thread( ethread ) ) {
				bypass::target::set_context_thread( ethread, &original_ctx );
				bypass::target::resume_thread( ethread );
			} else {
				logging::print( obf( "restore: could not suspend tid %u" ), tid );
			}

			virtual_free( sc_addr );

			if ( exec_ctx.m_status != thread::status_complete ) {
				logging::print( obf( "tid %u: execution timed out" ), tid );
				return false;
			}

			const bool result = static_cast<bool>( exec_ctx.m_result );
			logging::print( obf( "executed ( %s )" ), result ? obf( "success" ) : obf( "failure" ) );
			cleanup( );
			return result;
		}

		std::uint64_t get_module_base( ) const {
			return m_image_address;
		}

	private:
		bool m_is_system_dependency = false;
		bool m_use_context = false;
		std::shared_ptr < image::c_image > m_image { };
		std::uint32_t m_image_size { };
		std::uint64_t m_image_address { };

		std::uint64_t virtual_allocate( std::size_t size ) {
			auto ntdll = bypass::target::get_module_base( obf( L"ntdll.dll" ) );
			auto kernelbase = bypass::target::get_module_base( obf( L"kernelbase.dll" ) );
			if ( !ntdll || !kernelbase )
				return 0;

			auto virtual_alloc = bypass::target::get_module_export( kernelbase, obf( "VirtualAlloc" ) );
			auto zero_memory = bypass::target::get_module_export( ntdll, obf( "RtlZeroMemory" ) );
			if ( !virtual_alloc || !zero_memory )
				return 0;

			auto base_address = bypass::target::call_function(
				virtual_alloc,
				std::uint64_t { 0 },
				size,
				MEM_COMMIT | MEM_RESERVE,
				PAGE_EXECUTE_READWRITE );
			if ( !base_address )
				return 0;

			bypass::target::call_function( zero_memory, base_address, size );
			return base_address;
		}

		void virtual_free( std::uint64_t base_address ) {
			if ( !base_address )
				return;

			auto kernelbase = bypass::target::get_module_base( obf( L"kernelbase.dll" ) );
			if ( !kernelbase )
				return;

			auto virtual_free = bypass::target::get_module_export( kernelbase, obf( "VirtualFree" ) );
			if ( !virtual_free ) return;

			bypass::target::call_function(
				virtual_free,
				base_address,
				std::uint64_t { 0 },
				MEM_RELEASE
			);
		}

		void cleanup( ) {
			if ( !m_image_address )
				return;

			const auto import_dir = m_image->get_import_table( );
			if ( import_dir->m_virtual_address && import_dir->m_size ) {
				auto desc = m_image->rva_to_va< import_descriptor_t* >( import_dir->m_virtual_address );

				while ( desc && desc->m_name ) {
					auto dll_name = m_image->rva_to_va< char* >( desc->m_name );
					if ( dll_name ) {
						auto len = std::strlen( dll_name );
						std::vector< std::uint8_t > zeros( len, 0 );
						bypass::target::write_memory( m_image_address + desc->m_name, zeros.data( ), len );
					}

					if ( desc->m_original_first_thunk ) {
						auto orig = m_image->rva_to_va< image_thunk_data_t* >( desc->m_original_first_thunk );
						while ( orig && orig->m_u1.m_address_of_data ) {
							if ( !( orig->m_u1.m_ordinal & IMAGE_ORDINAL_FLAG64 ) ) {
								auto entry = m_image->rva_to_va< image_import_name_t* >( orig->m_u1.m_address_of_data );
								if ( entry ) {
									auto len = std::strlen( entry->m_name );
									auto name_rva = orig->m_u1.m_address_of_data + sizeof( std::uint16_t );
									std::vector< std::uint8_t > zeros( len, 0 );
									bypass::target::write_memory( m_image_address + name_rva, zeros.data( ), len );
								}
							}
							++orig;
						}
					}
					++desc;
				}

				std::vector< std::uint8_t > zeros( import_dir->m_size, 0 );
				bypass::target::write_memory(
					m_image_address + import_dir->m_virtual_address,
					zeros.data( ),
					import_dir->m_size );
			}

			{
				auto hdr_size = m_image->m_nt_headers->m_size_of_headers;
				std::vector< std::uint8_t > zeros( hdr_size, 0 );
				bypass::target::write_memory( m_image_address, zeros.data( ), hdr_size );
			}

			const auto reloc = m_image->get_reloc_directory( );
			if ( reloc->m_virtual_address && reloc->m_size ) {
				std::vector< std::uint8_t > zeros( reloc->m_size, 0 );
				bypass::target::write_memory(
					m_image_address + reloc->m_virtual_address,
					zeros.data( ),
					reloc->m_size );
			}

			logging::print( obf( "cleaned image\n" ) );
		}

		bool resolve_relocations( ) {
			const auto reloc_directory = m_image->get_reloc_directory( );
			if ( !reloc_directory->m_virtual_address || !reloc_directory->m_size ) {
				logging::print( obf( "no relocation table ( skipping )" ) );
				return true;
			}

			const auto reloc_delta = m_image_address - m_image->get_image_base( );
			auto reloc_entry = m_image->rva_to_va< reloc_entry_t* >( reloc_directory->m_virtual_address );
			if ( !reloc_entry ) {
				logging::print( obf( "could not resolve relocations" ) );
				return false;
			}

			const auto reloc_end = reinterpret_cast< reloc_entry_t* >(
				reinterpret_cast< std::uint8_t* >( reloc_entry ) + reloc_directory->m_size );
			while ( reloc_entry < reloc_end && reloc_entry->m_size ) {
				auto records_count { ( reloc_entry->m_size - sizeof( std::int8_t* ) ) >> 0x1 };
				for ( std::size_t i {}; i < records_count; i++ ) {
					auto fix_type { reloc_entry->m_item[ i ].m_type };
					auto shift_delta { reloc_entry->m_item[ i ].m_offset % 0x1000 };

					if ( fix_type == 0x3 || fix_type == 0xa ) {
						auto fix_va = m_image->rva_to_va< std::int8_t* >( reloc_entry->m_to_rva );
						if ( !fix_va )
							fix_va = reinterpret_cast< std::int8_t* >( m_image->m_dos_header );
						*reinterpret_cast< std::uint64_t* >( fix_va + shift_delta ) += reloc_delta;
					}
				}

				reloc_entry = reinterpret_cast< reloc_entry_t* >(
					reinterpret_cast< std::uint8_t* >( reloc_entry ) + reloc_entry->m_size );
			}

			logging::print( obf( "resolved relocations ( delta -> 0x%x )" ), reloc_delta );
			return true;
		}

		bool resolve_imports( ) {
			const auto import_table = m_image->get_import_table( );
			if ( !import_table->m_virtual_address || !import_table->m_size ) {
				logging::print( obf( "no import table ( skipping )" ) );
				return true;
			}

			std::uint32_t module_count = 0;
			std::uint32_t import_count = 0;

			auto import_desc = m_image->rva_to_va< import_descriptor_t* >( import_table->m_virtual_address );
			for ( auto descriptor = import_desc; descriptor->m_name; descriptor++ ) {
				auto module_name = m_image->rva_to_va < char* >( descriptor->m_name );
				if ( !module_name )
					return false;

				module_count++;

				auto module_lib = LoadLibraryA( module_name );
				if ( !module_lib )
					return false;

				char actual_path_buf[ MAX_PATH ] = {};
				GetModuleFileNameA( module_lib, actual_path_buf, MAX_PATH );
				auto actual_wpath  = utility::ansi_to_wstring( std::string( actual_path_buf ) );
				auto actual_fname  = utility::strip_path( actual_wpath );
				std::wstring actual_lower = actual_fname;
				std::transform( actual_lower.begin( ), actual_lower.end( ), actual_lower.begin( ), ::towlower );

				auto module_path = modules::get_module_path( module_name, m_image->get_file_path( ) );
				if ( module_path.empty( ) ) {
					FreeLibrary( module_lib );
					return false;
				}

				auto file_name = utility::strip_path( module_path );
				if ( file_name.empty( ) ) {
					FreeLibrary( module_lib );
					return false;
				}

				std::wstring file_name_lower = file_name;
				std::transform( file_name_lower.begin( ), file_name_lower.end( ),
					file_name_lower.begin( ), ::towlower );

				if ( !actual_lower.empty( ) && actual_lower != file_name_lower ) {
					file_name_lower = actual_lower;
					file_name       = actual_fname;
					module_path     = actual_wpath;
				}

				auto module_base = bypass::target::get_module_base( file_name.c_str( ) );
				if ( !module_base ) {
					printf( obf( "\n" ) );

					auto module = m_mapped_modules.find( file_name_lower );
					if ( module != m_mapped_modules.end( ) ) {
						if ( !module->second ) {
							logging::print( obf( "circular system dependency detected ( %ws )" ), file_name.c_str( ) );
							FreeLibrary( module_lib );
							return false;
						}

						module_base = module->second;
						logging::print( obf( "already-mapped %ws ( base -> 0x%llx )" ),
							file_name.c_str( ), module_base );
					}

					if ( !module_base ) {
						m_mapped_modules[ file_name_lower ] = 0;
						logging::print( obf( "parsing system dependency ( %ws )" ),
							file_name.c_str( ) );
						Sleep( 1500 );

						auto image = std::make_shared< image::c_image >( module_path );
						if ( !image ) {
							logging::print( obf( "could not parse system dependency" ) );
							m_mapped_modules.erase( file_name_lower );
							FreeLibrary( module_lib );
							return false;
						}

						auto injector = std::make_unique< injector::c_injector >( image, true );
						if ( !injector->prepare( ) ) {
							logging::print( obf( "could not prepare dependency ( %ws )" ), file_name.c_str( ) );
							m_mapped_modules.erase( file_name_lower );
							FreeLibrary( module_lib );
							return false;
						}

						module_base = injector->get_module_base( );
						if ( !module_base ) {
							logging::print( obf( "system dependency has no base address ( %ws )" ), file_name.c_str( ) );
							m_mapped_modules.erase( file_name_lower );
							FreeLibrary( module_lib );
							return false;
						}

						logging::print( obf( "created system dependency!\n" ) );
						m_mapped_modules[ file_name_lower ] = module_base;
						m_allocated_modules.push_back( module_base );
					}
				}

				auto thunk = m_image->rva_to_va< image_thunk_data_t* >( descriptor->m_first_thunk );
				if ( !thunk ) {
					logging::print( obf( "could not resolve first thunk ( %ws )" ), file_name.c_str( ) );
					FreeLibrary( module_lib );
					return false;
				}

				auto original_thunk_rva = descriptor->m_original_first_thunk ?
					descriptor->m_original_first_thunk : descriptor->m_first_thunk;
				auto original_thunk = m_image->rva_to_va< image_thunk_data_t* >( original_thunk_rva );
				if ( !original_thunk ) {
					logging::print( obf( "could not resolve original thunk ( %ws )" ), file_name.c_str( ) );
					FreeLibrary( module_lib );
					return false;
				}

				std::unordered_map<HMODULE, std::uint64_t> forward_cache;

				std::uint32_t func_count = 0;
				for ( auto current_thunk = thunk; original_thunk->m_u1.m_address_of_data; original_thunk++, current_thunk++ ) {
					std::uint64_t function = 0;

					if ( original_thunk->m_u1.m_ordinal & IMAGE_ORDINAL_FLAG64 ) {
						auto ordinal = ( std::uint16_t ) ( original_thunk->m_u1.m_ordinal & 0xFFFF );
						function = reinterpret_cast< std::uint64_t >( GetProcAddress( module_lib, ( LPCSTR ) ordinal ) );
					}
					else {
						auto import_name = m_image->rva_to_va< image_import_name_t* >( original_thunk->m_u1.m_address_of_data );
						if ( !import_name ) {
							logging::print( obf( "could not resolve import data ( %ws )" ), file_name.c_str( ) );
							FreeLibrary( module_lib );
							return false;
						}

						function = reinterpret_cast< std::uint64_t >( GetProcAddress( module_lib, import_name->m_name ) );
					}

					if ( !function ) {
						logging::print( obf( "could not resolve import ( %ws )" ), file_name.c_str( ) );
						FreeLibrary( module_lib );
						return false;
					}

					auto target_base = module_base;
					auto local_base  = reinterpret_cast< std::uint64_t >( module_lib );

					HMODULE func_module = nullptr;
					GetModuleHandleExA(
						GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
						reinterpret_cast< LPCSTR >( function ), &func_module );

					if ( func_module && func_module != module_lib ) {
						auto it = forward_cache.find( func_module );
						if ( it == forward_cache.end( ) ) {
							char buf[ MAX_PATH ] = {};
							GetModuleFileNameA( func_module, buf, MAX_PATH );
							const char* sep = strrchr( buf, '\\' );
							auto resolved_w = utility::ansi_to_wstring( sep ? sep + 1 : buf );
							auto resolved_base = bypass::target::get_module_base( resolved_w.c_str( ) );
							forward_cache.emplace( func_module, resolved_base );
							it = forward_cache.find( func_module );
						}
						if ( !it->second ) {
							logging::print( obf( "forwarded module not in target process ( %ws )" ), file_name.c_str( ) );
							FreeLibrary( module_lib );
							return false;
						}
						target_base = it->second;
						local_base  = reinterpret_cast< std::uint64_t >( func_module );
					}

					auto offset = function - local_base;
					current_thunk->m_u1.m_function = target_base + offset;

					func_count++;
					import_count++;
				}

				FreeLibrary( module_lib );
			}

			logging::print( obf( "resolved imports ( %u )" ), import_count );
			return true;
		}

		bool resolve_sections( ) {
			std::uint32_t mapped_size = 0;

			bypass::target::write_memory(
				m_image_address,
				m_image->m_image_bytes.get( ),
				m_image->m_nt_headers->m_size_of_headers );

			auto section = m_image->m_section_header;
			for ( auto idx = 0; idx < m_image->m_nt_headers->m_number_of_sections; idx++, section++ ) {
				auto raw_size = section->m_size_of_raw_data;
				auto virt_size = section->m_virtual_size;

				if ( raw_size > 0 ) {
					auto src = m_image->m_image_bytes.get( ) + section->m_pointer_to_raw_data;
					auto write_size = min( raw_size, virt_size );

					bypass::target::write_memory(
						m_image_address + section->m_virtual_address,
						src,
						write_size );
				}

				mapped_size += virt_size;
			}

			logging::print( obf( "resolved sections ( 0x%x )" ), mapped_size );
			return true;
		}
	};
}

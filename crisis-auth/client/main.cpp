#include <windows.h>
#include <iostream>
#include <source/logging/logger.hpp>
#include <source/auth/auth.hpp>
#include "client/auth_module.hpp"
#include "client/pe_loader.hpp"

int main( ) {
    std::string key;
    if ( !auth::prompt_key( key ) ) {
        rise::log_error( "Failed to read license key." );
        return 1;
    }

    std::string hwid = auth::generate_hwid( );
    rise::log_info( "HWID -> %s", hwid.c_str( ) );

    auto mod_result = loader::authenticate_and_download( key, hwid, "127.0.0.1:223", "test.dll" );
    if ( !mod_result.success ) {
        rise::log_error( "%s", mod_result.message.c_str( ) );
        return 1;
    }

    rise::log_info( "%s", mod_result.message.c_str( ) );

    pe::mapped_module_t mapped = pe::manual_map( mod_result.decrypted_bytes );
    if ( !mapped.ok ) {
        rise::log_error( "Failed to map module into memory." );
        return 1;
    }

    using run_fn_t = void( * )( );
    auto* run_fn = reinterpret_cast<run_fn_t>( pe::get_export( mapped, "Run" ) );
    if ( run_fn ) {
        rise::log_info( "Calling exported Run()..." );
        run_fn( );
    }
    else {
        rise::log_info( "No Run() export found; module loaded but not executed." );
    }

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get( );
    return 0;
}

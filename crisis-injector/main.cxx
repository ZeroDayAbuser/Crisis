#include <deps\includes.h>

int main( ) {
	SetConsoleTitleA( obf( "crisis-injector" ) );
	CoInitializeEx( nullptr, COINIT_MULTITHREADED );
	SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );

	bypass::c_config config {
		obf( L"FortniteClient-Win64-Shipping.exe" ),
		obf( L"crisis" )
	};

	config.enable_mouse( );
	config.enable_overlay( );
	config.enable_spoofer( );

	bypass::initialize( config );
	if ( !bypass::create( ) ) {
		logging::print( obf( "failed to create bypass." ) );
		return std::getchar( );
	}

	if ( !bypass::target::setup( ) ) {
		logging::print( obf( "failed to create process." ) );
		return std::getchar( );
	}

	auto path = utility::get_exe_directory( ) + L"\\" + L"crisis-dumper.dll";
	if ( utility::file_exists( path ) ) {
		auto image = std::make_shared < image::c_image >( path );
		if ( !image->is_valid( ) ) {
			logging::print( obf( "could not parse crisis-dumper.dll." ) );
			return std::getchar( );
		}

		const auto time_stamp = static_cast< std::time_t >( image->get_time_date_stamp( ) );
		char buf[ 64 ];
		std::tm tm { };
		gmtime_s( &tm, &time_stamp );
		std::strftime( buf, sizeof( buf ), "%Y-%m-%d %H:%M:%S UTC", &tm );
		logging::print( obf( "successfully parsed %ls ( %s )" ), image->get_file_name( ).c_str( ), buf );

		auto injector = std::make_unique < injector::c_injector >( image, false, true );
		if ( !injector->prepare( ) ) {
			logging::print( obf( "could not prepare %ls." ), image->get_file_name( ).c_str( ) );
			return std::getchar( );
		}

		if ( !injector->execute( ) ) {
			logging::print( obf( "could not execute %ls." ), image->get_file_name( ).c_str( ) );
			return std::getchar( );
		}

		return std::getchar( );
	}
	else {
		logging::print( obf( "could not find crisis-dumper.dll in folder." ) );
		logging::print( obf( "please drag the .dll into the same folder." ) );
		return std::getchar( );
	}
}
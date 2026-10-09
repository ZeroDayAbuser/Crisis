#pragma once

namespace logging {
	template<typename... Args>
	inline void print( const char* format, Args... args ) {
		auto now = std::chrono::system_clock::now( );
		std::time_t time = std::chrono::system_clock::to_time_t( now );
		tm local_tm;
		localtime_s( &local_tm, &time );
		printf( obf( "\x1b[38;2;128;128;128m[\x1b[38;2;128;128;128m%02d\x1b[38;2;128;128;128m/\x1b[38;2;128;128;128m%02d\x1b[38;2;128;128;128m/\x1b[38;2;128;128;128m%04d\x1b[38;2;128;128;128m \x1b[38;2;128;128;128m%02d\x1b[38;2;128;128;128m:\x1b[38;2;128;128;128m%02d\x1b[38;2;128;128;128m:\x1b[38;2;128;128;128m%02d]\x1b[0m " ),
			local_tm.tm_mon + 1,
			local_tm.tm_mday,
			local_tm.tm_year + 1900,
			local_tm.tm_hour,
			local_tm.tm_min,
			local_tm.tm_sec );
		printf( obf( "\x1b[38;2;1;86;131m>\x1b[0m " ) );
		printf( obf( "\x1b[37m" ) );
		printf( format, args... );
		printf( obf( "\x1b[0m\n" ) );
	}
}

namespace utility {
	std::uint32_t find_pid( std::wstring target_process );
	std::wstring to_lower( std::wstring str );
	std::wstring ansi_to_wstring( const std::string& input, DWORD locale = 0 );
	std::string  wstring_to_ansi( const std::wstring& input, DWORD locale = 0 );
	std::wstring get_exe_directory( );
	std::wstring strip_path( const std::wstring& path );
	bool         file_exists( const std::wstring& path );
	std::wstring get_process_directory( DWORD pid );
	std::string  add_commas( std::uint64_t n );
}
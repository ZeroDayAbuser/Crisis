#pragma once

namespace usn {
    class c_journal {
    public:
        bool clear( ) {
            char sys_dir[ MAX_PATH ]{};
            GetSystemDirectoryA( sys_dir, MAX_PATH );
            const std::string primary = std::string( 1, sys_dir[ 0 ] ) + ":";
            return clear_drive( primary );
        }

    private:
        bool clear_drive( const std::string& drive ) {
            const auto path = std::format( "\\\\.\\{}", drive );
            const auto vol  = CreateFileA(
                path.c_str( ),
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, 0, nullptr
            );

            if ( vol == INVALID_HANDLE_VALUE )
                return false;

            const auto result = delete_journal( vol, drive );
            CloseHandle( vol );
            return result;
        }

        bool delete_journal( HANDLE vol, const std::string& drive ) {
            DWORD br{};
            USN_JOURNAL_DATA_V0 jd{};
            if ( !DeviceIoControl( vol, FSCTL_QUERY_USN_JOURNAL,
                nullptr, 0, &jd, sizeof( jd ), &br, nullptr ) )
                return false;

            std::vector< uint8_t > buf( 65536 );
            READ_USN_JOURNAL_DATA_V0 ruj{};
            ruj.StartUsn     = jd.FirstUsn;
            ruj.ReasonMask   = 0xFFFFFFFF;
            ruj.UsnJournalID = jd.UsnJournalID;

            while ( true ) {
                DWORD bytes = 0;
                if ( !DeviceIoControl( vol, FSCTL_READ_USN_JOURNAL,
                    &ruj, sizeof( ruj ), buf.data( ), static_cast< DWORD >( buf.size( ) ), &bytes, nullptr ) )
                    break;
                if ( bytes <= sizeof( USN ) )
                    break;
                ruj.StartUsn = *reinterpret_cast< USN* >( buf.data( ) );
            }
			USN_JOURNAL_DATA_V0 jd_after {};
			if ( DeviceIoControl( vol, FSCTL_QUERY_USN_JOURNAL,
				nullptr, 0, &jd_after, sizeof( jd_after ), &br, nullptr ) ) {
				if ( jd_after.MaximumSize > 0 ) {
					const auto remaining = static_cast< ULONGLONG >( jd_after.NextUsn - jd_after.FirstUsn );
					const ULONGLONG max_threshold = 100ULL * 1024 * 1024;
					const auto threshold = ( jd_after.MaximumSize / 20ULL ) < max_threshold
						? ( jd_after.MaximumSize / 20ULL )
						: max_threshold;

					// clamp: journal can legitimately exceed MaximumSize before trimming kicks in
					const auto usage_ratio = ( remaining * 100ULL ) / jd_after.MaximumSize;
					const auto percent_left = usage_ratio >= 100ULL ? 100ULL : usage_ratio;

					if ( remaining > threshold ) {
						logging::print( obf( "warning: %llu%% of usn journal uncleaned!" ),
							percent_left );
					}
				}
			}

            DELETE_USN_JOURNAL_DATA del{};
            del.UsnJournalID = jd.UsnJournalID;
            del.DeleteFlags  = 0x00000002UL;

            if ( !DeviceIoControl( vol, FSCTL_DELETE_USN_JOURNAL,
                &del, sizeof( del ), nullptr, 0, &br, nullptr ) )
                return false;

            return true;
        }
    };
}

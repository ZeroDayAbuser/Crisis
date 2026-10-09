#pragma once

namespace components::firmware {
    namespace efi {
        constexpr GUID m_guid = {
            0xeaec226f,
            0xc9a3,
            0x477a,
            { 0xa8, 0x26, 0xdd, 0xc7, 0x16, 0xcd, 0xc0, 0xe3 }
        };

        void randomize_buffer( std::uint8_t* buffer, unsigned long length ) {
            if ( !buffer || !length )
                return;

            for ( auto i = 0ul; i < length; ++i )
                buffer[ i ] = static_cast< std::uint8_t >( kernel::rand( ) & 0xFF );
        }

        bool spoof_entry( const wchar_t* entry_name ) {
            if ( !entry_name )
                return false;

            UNICODE_STRING name{ };
            RtlInitUnicodeString( &name, entry_name );

            std::uint8_t buffer[ 0x100 ]{ };
            std::uint8_t original[ 0x100 ]{ };
            auto length = static_cast< unsigned long >( sizeof( buffer ) );
            auto attributes = 0ul;

            auto status = kernel::ex_get_firmware_environment_variable(
                &name,
                const_cast< GUID* >( &m_guid ),
                buffer,
                &length,
                &attributes );

            if ( !NT_SUCCESS( status ) ) {
                disk::print( obf( "efi %ws get failed -> 0x%x" ), entry_name, status );
                return false;
            }

            RtlCopyMemory( original, buffer, length );
            randomize_buffer( buffer, length );

            if ( RtlCompareMemory( buffer, original, length ) == length ) {
                disk::print( obf( "efi %ws randomize failed -> bytes unchanged" ), entry_name );
                return false;
            }

            attributes |= VARIABLE_ATTRIBUTE_NON_VOLATILE;

            status = kernel::ex_set_firmware_environment_variable(
                &name,
                const_cast< GUID* >( &m_guid ),
                buffer,
                length,
                attributes );

            if ( !NT_SUCCESS( status ) ) {
                disk::print( obf( "efi %ws set failed -> 0x%x" ), entry_name, status );
                return false;
            }

            std::uint8_t readback[ 0x100 ]{ };
            auto readback_length = static_cast< unsigned long >( sizeof( readback ) );

            status = kernel::ex_get_firmware_environment_variable(
                &name,
                const_cast< GUID* >( &m_guid ),
                readback,
                &readback_length,
                nullptr );

            if ( !NT_SUCCESS( status ) ) {
                disk::print( obf( "efi %ws readback failed -> 0x%x" ), entry_name, status );
                return false;
            }

            if ( RtlCompareMemory( readback, original, readback_length ) == readback_length ) {
                disk::print( obf( "efi %ws readback matches original -> write did not persist" ), entry_name );
                return false;
            }

            disk::print( obf( "efi %ws spoofed" ), entry_name );
            return true;
        }
    }

    namespace registry {
        bool set_smbios_data( const void* data, unsigned long size ) {
            HANDLE key = nullptr;
            UNICODE_STRING keyPath;
            RtlInitUnicodeString( &keyPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\mssmbios\\Data" );
            OBJECT_ATTRIBUTES attributes;
            InitializeObjectAttributes( &attributes, &keyPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr );
            if ( !NT_SUCCESS( ZwOpenKey( &key, KEY_WRITE, &attributes ) ) )
                return false;

            UNICODE_STRING valName;
            RtlInitUnicodeString( &valName, L"SMBiosData" );
            bool result = NT_SUCCESS( ZwSetValueKey( key, &valName, 0, REG_BINARY, const_cast< void* >( data ), size ) );
            ZwClose( key );
            return result;
        }
    }

    bool spoof( ) {
        
        
        
        
        
        
        
        

        
        
        
        

        return true;
    }
}

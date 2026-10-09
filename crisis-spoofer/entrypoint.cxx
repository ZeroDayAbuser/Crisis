#include <includes.h>

bool entry_point( entry_t* entry ) {
	pdb::m_data = entry->m_pdb_blob;
	pdb::m_size = entry->m_pdb_blob_size;

	kernel::m_image_base = entry->m_image_base;
	kernel::m_image_size = entry->m_image_size;

	kernel::m_ntoskrnl_base = entry->m_ntoskrnl_base;
	if ( !kernel::m_ntoskrnl_base )
		return false;

	pdb::rebase( kernel::m_ntoskrnl_base );

	auto* dos_header = reinterpret_cast< dos_header_t* >( kernel::m_ntoskrnl_base );
	auto* nt_headers = reinterpret_cast< nt_headers_t* >(
		kernel::m_ntoskrnl_base + dos_header->m_lfanew );
	kernel::m_ntoskrnl_size = nt_headers->m_size_of_image;

	if ( !disk::setup( entry->m_product_name ) )
		return false;

	disk::start_worker( );

	paging::m_directory_table_base = entry->m_directory_table_base;
	if ( !paging::m_directory_table_base )
		return false;

	LARGE_INTEGER system_time = { };
	LARGE_INTEGER local_time = { };
	TIME_FIELDS   time_fields = { };
	KeQuerySystemTime( &system_time );
	ExSystemTimeToLocalTime( &system_time, &local_time );
	RtlTimeToTimeFields( &local_time, &time_fields );

	disk::print( obf( "=== crisis initialized | %02hd/%02hd/%04hd %02hd:%02hd:%02hd ===" ),
		time_fields.Month, time_fields.Day, time_fields.Year,
		time_fields.Hour, time_fields.Minute, time_fields.Second );
	disk::print( obf( "directory table base -> 0x%llx" ), paging::m_directory_table_base );
	disk::print( obf( "ntoskrnl base -> 0x%llx, size -> 0x%x" ),
		kernel::m_ntoskrnl_base, kernel::m_ntoskrnl_size );
	disk::print( obf( "image base -> 0x%llx, size -> 0x%x" ),
		entry->m_image_base, entry->m_image_size );

	phys::setup( );
	if ( !paging::dpm::initialize( ) )
		return false;

	if ( !exception::create( ) )
		return false;

#if !COMMUNICATION_ONLY
	hwid::build_identity( );

	components::monitor::spoof( );
	components::motherboard::spoof( );
	components::boot::spoof( );

	if ( !image::callback::create( ) )
		return false;

	if ( !etw::initialize( ) )
		return false;

	if ( !hooks::um::create( ) )
		return false;

	if ( !hooks::km::create( ) )
		return false;
#endif

	disk::print( obf( "created exception framework!" ) );
	return true;
}
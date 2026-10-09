#pragma once

namespace module {
	inline constexpr std::uint32_t k_capacity = 4096;
	inline constexpr std::uint32_t k_capacity_mask = k_capacity - 1;
	inline constexpr std::size_t k_payload_size = 0x1000;
	inline constexpr std::uint64_t k_max_ranges = 64;

	enum packet_status_t : LONG {
		packet_empty,
		packet_pending,
		packet_complete
	};

	enum class memory_request_type : std::uint32_t {
		none,
		map_memory,
		read_memory,
		write_memory,
		query_mapping
	};

	enum class overlay_request_type : std::uint32_t {
		none,
		render_clear,
		render_clear_color,
		render_line,
		render_rectangle,
		render_filled_rectangle,
		render_circle,
		render_filled_circle,
		render_text,
		render_interactive_area,
		render_minimize,
		render_register_font,
		render_bring_to_top,
		render_set_monitor,
		render_get_size,
		render_register_image,
		render_image,
		render_rounded_rectangle,
		render_filled_rounded_rectangle,
		render_aurora,
		render_frosted_panel,
		render_push_clip,
		render_pop_clip,
		render_skeleton,
		render_gradient_rectangle,
		render_gradient_rectangle_h
	};

	struct packet_t {
		volatile LONG status;
		std::uint32_t command;
		std::uint64_t addr;
		std::uint64_t buffer;
		std::uint64_t size;
		std::uint64_t sequence;
		std::uint64_t result;
		std::uint8_t  payload[ k_payload_size ];
	};

	struct mapping_range_t {
		std::uint64_t base_pa { };
		std::uint64_t size { };
		std::uint64_t va_base { };
	};

	struct mapping_info_t {
		std::uint64_t   physical_mask { };
		std::uint64_t   max_ranges { };
		std::uint64_t   range_count { };
		mapping_range_t ranges[ k_max_ranges ] { };
	};

	struct memory_state_t {
		volatile LONG   stop;
		volatile LONG64 write_index;
		volatile LONG64 read_index;
		packet_t        packets[ k_capacity ];
	};

	struct overlay_state_t {
		volatile LONG   stop;
		volatile LONG   ready;
		volatile LONG64 write_index;
		volatile LONG64 read_index;
		packet_t        packets[ k_capacity ];
	};

	struct size_response_t {
		std::int32_t width { };
		std::int32_t height { };
	};

	class c_module {
	public:
		c_module( ) = default;
		c_module( const std::string& image_path, std::uint32_t process_id );
		c_module( std::vector<std::uint8_t> image, std::uint32_t process_id );
		~c_module( );

		bool is_dll( ) const;
		bool inject( LPVOID reserved = nullptr );
		bool inject( const void* parameter, std::size_t parameter_size );
		bool cleanup( );
		bool unload( );
		std::optional<std::uint64_t> get_base( ) const;

	private:
		std::uint32_t             m_process_id { };
		std::uint64_t             m_mapped_base { };
		std::vector<std::uint8_t> m_image { };
		IMAGE_NT_HEADERS* m_nt_headers { nullptr };

		bool load_stub(
			HANDLE  process_handle,
			BYTE* image_data,
			SIZE_T  file_size,
			bool    clear_header,
			bool    clear_non_needed_sections,
			bool    adjust_protections,
			bool    seh_exception_support,
			DWORD   reason,
			LPVOID  reserved );
		[[nodiscard]] bool discard_image( );

		bool load_image( const std::string& image_path );
		bool initialize_image( );
	};

	class c_memory {
	public:
		~c_memory( ) { close( ); }

		bool open( );
		bool ping( );
		void benchmark( );
		void close( );
		bool read( std::uint64_t addr, void* buffer, std::size_t size );
		bool write( std::uint64_t addr, const void* buffer, std::size_t size );
		bool map( std::uint64_t pa, void* va, std::size_t size );
		bool query_mapping( mapping_info_t& info );

	private:
		bool request( memory_request_type command, std::uint64_t addr,
			void* buffer, std::size_t size );

		transport::channel_t  m_channel { };
		transport::section_t* m_state = nullptr;
		std::mutex            m_mutex { };
	};

	class c_overlay {
	public:
		~c_overlay( ) { close( ); }
		struct set_monitor_request_t {
			std::uint32_t index {};
		};

		bool open( );
		bool minimize( );
		bool bring_to_top( );
		bool set_monitor( std::uint32_t index );

		bool ping( );
		void close( );

	private:
		bool request( overlay_request_type command, void* buffer, std::size_t size );

		transport::channel_t  m_channel { };
		transport::section_t* m_state = nullptr;
		std::mutex            m_mutex { };
	};
}

#pragma once

#ifdef small
#undef small
#endif

namespace overlay {
	static constexpr std::uint32_t capacity            = transport::k_capacity;
	static constexpr std::uint32_t capacity_mask       = transport::k_capacity_mask;
	static constexpr std::size_t   packet_payload_size = transport::k_payload;
	static constexpr std::size_t   text_capacity       = 256;
	static constexpr std::size_t   font_name_capacity  = 64;
	static constexpr std::size_t   font_path_capacity  = 260;
	static constexpr std::size_t   image_path_capacity = 260;
	static constexpr std::size_t   max_aurora_blobs    = 8;

	enum class command_id : std::uint32_t {
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
		render_gradient_rectangle_h,
		render_present,
		render_batch,
		render_ping,
		render_scene3d,
	};

	struct batch_record_t {
		std::uint32_t command;
		std::uint32_t size;
	};

	enum class text_align_t : std::uint32_t { left, center, right };
	enum class text_weight_t : std::uint32_t {
		thin = 100, light = 300, regular = 400,
		medium = 500, semi_bold = 600, bold = 700, black = 900
	};

	struct color_t { std::uint8_t r {}; std::uint8_t g {}; std::uint8_t b {}; std::uint8_t a { 255 }; };
	struct point_t { float x {}; float y {}; };
	struct rect_t  { float x {}; float y {}; float width {}; float height {}; };

	struct aurora_blob_t { float x {}; float y {}; float radius {}; color_t color {}; };

	struct line_request_t           { point_t from {}; point_t to {}; color_t color {}; float thickness { 1.0f }; };
	struct rectangle_request_t      { rect_t area {}; color_t color {}; float thickness { 1.0f }; };
	struct circle_request_t         { point_t center {}; float radius {}; color_t color {}; float thickness { 1.0f }; };
	struct rounded_rectangle_request_t { rect_t area {}; color_t color {}; float radius {}; float thickness { 1.0f }; };
	struct frosted_panel_request_t  { rect_t area {}; float corner_radius {}; float sigma {}; color_t tint {}; };
	struct gradient_rectangle_request_t { rect_t area {}; color_t color_a {}; color_t color_b {}; float corner_radius { 0.0f }; };

	struct text_request_t {
		wchar_t       value[ text_capacity ] {};
		wchar_t       font[ font_name_capacity ] {};
		rect_t        area {};
		color_t       color {};
		float         size { 14.0f };
		text_weight_t weight { text_weight_t::bold };
		text_align_t  alignment { text_align_t::left };
		bool          wrap {};
	};

	struct font_register_request_t {
		wchar_t name[ font_name_capacity ] {};
		wchar_t path[ font_path_capacity ] {};
	};

	struct image_register_request_t {
		std::uint32_t id {};
		wchar_t       path[ image_path_capacity ] {};
	};

	struct image_request_t {
		std::uint32_t id {};
		rect_t        area {};
		color_t       tint { 255, 255, 255, 255 };
		float         opacity { 1.0f };
		float         corner_radius { 0.0f };
	};

	// Byte-identical to crisis-overlay's scene3d_request_t. Shared memory
	// carries this struct verbatim; the consumer's sizeof() gates dispatch.
	struct scene3d_request_t {
		rect_t  area {};
		color_t tint { 255, 255, 255, 255 };
		float   rotation_x { 0.0f };
		float   rotation_y { 0.0f };
		float   rotation_z { 0.0f };
		float   opacity    { 1.0f };
	};

	struct set_monitor_request_t { std::uint32_t index {}; };

	struct aurora_request_t {
		rect_t        area {};
		float         corner_radius {};
		float         sigma {};
		float         grain_strength { 0.06f };
		std::uint32_t blob_count {};
		aurora_blob_t blobs[ max_aurora_blobs ] {};
	};

	struct size_response_t { std::int32_t width {}; std::int32_t height {}; };

	using transport::packet_empty;
	using transport::packet_pending;
	using transport::packet_complete;
	using transport::packet_abandoned;

	using packet_t = transport::packet_t;
	using shared_state_t = transport::section_t;
}

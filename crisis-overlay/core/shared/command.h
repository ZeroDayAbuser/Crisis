#pragma once

#ifdef small
#undef small
#endif

namespace communication {
    constexpr std::uint32_t capacity = transport::k_capacity;
    constexpr std::uint32_t capacity_mask = transport::k_capacity_mask;
    constexpr std::size_t packet_payload_size = transport::k_payload;
    constexpr std::size_t text_capacity = 256;
    constexpr std::size_t font_name_capacity = 64;
    constexpr std::size_t font_path_capacity = 260;

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
		render_gradient_rectangle,    // vertical   — color_a (top)  → color_b (bottom)
		render_gradient_rectangle_h,  // horizontal — color_a (left) → color_b (right)
		render_present,
		// Frame batch — see crisis-bypass counterpart. Payload is a
		// concatenated stream of [batch_record_t{command,size}, payload].
		render_batch,
		// Ping — used by producer's startup handshake to verify batching
		// support. Reply is a trivial "true"; the producer runs a probe
		// render_batch right after and only enables batching if both succeed.
		render_ping,
		// 3D scene draw — see crisis-bypass counterpart. Renders the
		// built-in humanoid mesh into an off-screen D3D texture with the
		// requested rotation + tint, then blits it into the D2D pass as
		// an image inside `area`.
		render_scene3d,
	};

	// Layout byte-identical to crisis-bypass's copy.
	struct batch_record_t {
		std::uint32_t command;
		std::uint32_t size;
	};

    constexpr std::size_t image_path_capacity = 260;

    enum class text_align_t : std::uint32_t {
        left,
        center,
        right
    };

    enum class text_weight_t : std::uint32_t {
        thin      = 100,
        light     = 300,
        regular   = 400,
        medium    = 500,
        semi_bold = 600,
        bold      = 700,
        black     = 900
    };

    struct color_t {
        std::uint8_t r{};
        std::uint8_t g{};
        std::uint8_t b{};
        std::uint8_t a{ 255 };
    };

    struct point_t {
        float x{};
        float y{};
    };

    struct rect_t {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct line_request_t {
        point_t from{};
        point_t to{};
        color_t color{};
        float thickness{ 1.0f };
    };

    constexpr std::size_t skeleton_segment_capacity = 14;

    struct skeleton_segment_t {
        point_t from{};
        point_t to{};
    };

    struct skeleton_request_t {
        std::uint32_t count{};
        skeleton_segment_t segments[ skeleton_segment_capacity ]{};
        color_t color{};
        color_t outline_color{ 0, 0, 0, 180 };
        float thickness{ 1.5f };
        bool outline{ true };
    };
    static_assert( sizeof( skeleton_request_t ) == 244 );

    struct rectangle_request_t {
        rect_t area{};
        color_t color{};
        float thickness{ 1.0f };
    };

    struct circle_request_t {
        point_t center{};
        float radius{};
        color_t color{};
        float thickness{ 1.0f };
    };

    struct text_request_t {
        wchar_t value[ text_capacity ]{};
        wchar_t font[ font_name_capacity ]{};
        rect_t area{};
        color_t color{};
        float size{ 14.0f };
        text_weight_t weight{ text_weight_t::bold };
        text_align_t alignment{ text_align_t::left };
        bool wrap{};
    };

    struct font_register_request_t {
        wchar_t name[ font_name_capacity ]{};
        wchar_t path[ font_path_capacity ]{};
    };

    // Must be byte-identical to crisis-bypass's copy — both sides read/write
    // the same packet payload from shared memory. No mask preprocessing byte —
    // the tinted draw path (FillOpacityMask) reads only the bitmap's alpha
    // channel regardless of RGB, so no bitmap preprocessing is required for
    // solid-tint silhouette rendering.
    struct image_register_request_t {
        std::uint32_t id{};
        wchar_t       path[ image_path_capacity ]{};
    };

    struct image_request_t {
        std::uint32_t id{};
        rect_t        area{};
        color_t       tint{ 255, 255, 255, 255 };
        float         opacity{ 1.0f };
        float         corner_radius{ 0.0f };
    };

    // 3D-scene draw payload — see command_id::render_scene3d.
    // Layout byte-identical to crisis-bypass's copy.
    struct scene3d_request_t {
        rect_t  area{};
        color_t tint{ 255, 255, 255, 255 };
        float   rotation_x{ 0.0f };
        float   rotation_y{ 0.0f };
        float   rotation_z{ 0.0f };
        float   opacity   { 1.0f };
    };

    // Index into the deterministic monitor ordering both sides derive from
    // EnumDisplayMonitors (primary first, then by top-left position). Same
    // index -> same physical panel on both sides, without shipping HMONITOR
    // handles that aren't valid across processes.
    struct set_monitor_request_t {
        std::uint32_t index{};
    };

    // Response payload for render_get_size. The overlay writes this directly
    // into command.buffer (== packet.payload in shared memory); the client
    // reads it back from the same packet once request() observes
    // packet_complete.
    struct size_response_t {
        std::int32_t width{};
        std::int32_t height{};
    };

    struct rounded_rectangle_request_t {
        rect_t  area{};
        color_t color{};
        float   radius{};
        float   thickness{ 1.0f };
    };

    static constexpr std::size_t max_aurora_blobs = 8;

    struct aurora_blob_t {
        float   x{};
        float   y{};
        float   radius{};
        color_t color{};
    };

    struct aurora_request_t {
        rect_t        area{};
        float         corner_radius{};
        float         sigma{};
        float         grain_strength{ 0.06f };
        std::uint32_t blob_count{};
        aurora_blob_t blobs[ max_aurora_blobs ]{};
    };

    struct frosted_panel_request_t {
        rect_t  area{};
        float   corner_radius{};
        float   sigma{};
        color_t tint{};
    };

    // Filled rectangle with a two-stop linear gradient.
    // Vertical variant  (render_gradient_rectangle):   color_a at top,  color_b at bottom.
    // Horizontal variant (render_gradient_rectangle_h): color_a at left, color_b at right.
    // corner_radius > 0 clips to a rounded rect (same as filled_rounded_rectangle).
    struct gradient_rectangle_request_t {
        rect_t  area{};
        color_t color_a{};
        color_t color_b{};
        float   corner_radius{ 0.0f };
    };

    struct command_t {
        command_id id{ command_id::none };
        void* buffer{};
        std::size_t size{};
    };

    using transport::packet_empty;
    using transport::packet_pending;
    using transport::packet_complete;
    using transport::packet_abandoned;

    using packet_t = transport::packet_t;
    using shared_state_t = transport::section_t;
}

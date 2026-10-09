#pragma once

namespace tostring_emu
{
	struct mapped_region
	{
		std::uint64_t remote_base {};
		std::uint64_t emu_base {};
		std::uint64_t size {};
		std::vector< std::uint8_t > bytes {};
		bool writable {};
	};

	struct run_config
	{
		std::uint64_t function {};
		std::uint64_t image_base {};
		std::uint64_t image_size {};
		std::uint64_t max_instructions { 200000 };
		std::uint64_t timeout_us { 250000 };

		std::uint64_t rcx {};
		std::uint64_t rdx {};
		std::uint64_t r8 {};
		std::uint64_t r9 {};

		std::uint64_t stack_base { 0x700000000000 };
		std::uint64_t stack_size { 0x100000 };

		std::vector< mapped_region > mapped_data {};
	};

	struct run_result
	{
		bool ok {};
		std::string error {};
		std::uint64_t rip {};
		std::uint64_t rax {};
		std::unordered_map< std::uint64_t, std::vector< std::uint8_t > > mapped_output {};
	};

	constexpr std::uint64_t page_size = 0x1000;
	constexpr std::uint64_t emu_image_base = 0x140000000;
	constexpr std::uint64_t stop_address = 0x00000000DEAD0000ull;




	std::uint64_t align_down(
		std::uint64_t value,
		std::uint64_t alignment = page_size );
	std::uint64_t align_up(
		std::uint64_t value,
		std::uint64_t alignment = page_size );
	std::string uc_error( const char* prefix, uc_err error );
	struct tostring_layout
	{
		std::uint64_t state {};
		std::uint64_t guard {};
		std::uint64_t guard_delta {};
	};

	// Resolves the lazy-init block used by FName::ToString directly from the
	// live function bytes.  The caller passes a *runtime* function address, so
	// this is independent of the PE/IDA image base used by the dump.
	//
	// Expected shape (register allocation may vary):
	//     cmp  byte ptr [rip + guard], 0
	//     jne  ...
	//     lea  rcx, [rip + state]
	//     call initializer
	//     mov  byte ptr [rip + guard], 1
	//
	// Zydis is used for every RIP-relative calculation; no state RVA is
	// hardcoded.

	bool resolve_tostring_layout(
		std::uint64_t function,
		tostring_layout& output,
		std::string& error );
	class emulator
	{
	public:
		using call_handler_t = std::function< bool( std::uint64_t target, std::uint64_t return_address ) >;

		emulator( const emulator& ) = delete;
		emulator& operator=( const emulator& ) = delete;






		emulator( );
		~emulator( );
		bool init( std::string& error );
		void set_call_handler( call_handler_t handler );
		run_result run( const run_config& config );
	private:

		struct impl;
		impl* m_impl {};
	};

	std::vector< std::uint8_t > make_input_buffer(
		std::span< const std::uint8_t > bytes );
}
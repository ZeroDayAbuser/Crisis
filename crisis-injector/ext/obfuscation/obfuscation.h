#pragma once
#ifndef OBFUSHEADER_H
#define OBFUSHEADER_H

#define obf_enabled 1

#define obf_const_encryption        1
#define obf_const_encrypt_threadlocal 0
#define obf_cflow_const_decryption  0
#define obf_cflow_branching         1
#define obf_indirect_branching      1
#define obf_fake_signatures         1
#define obf_inline_std              0

#if defined(_MSC_VER) && !defined(__clang__)
#  define _obf_msvc
#elif defined(__GNUC__) || defined(__clang__)
#  define _obf_gnuc
#endif

#if defined(__x86_64__) || defined(_M_X64)
#  define _obf_x64
#elif defined(i386) || defined(_M_IX86)
#  define _obf_x86
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define _obf_arm64
#endif

#if defined(_WIN64) || defined(_WIN32)
#  define _obf_windows
#elif defined(__linux__)
#  define _obf_linux
#elif defined(__APPLE__)
#  define _obf_apple
#endif

#ifdef _obf_msvc
#  pragma warning(disable: 4996 4005)
#  define obf_inline   __forceinline
#  define obf_noinline __declspec(noinline)
#  define obf_section(x) __declspec(allocate(x))
#else
#  define obf_inline   __attribute__((always_inline))
#  define obf_noinline __attribute__((noinline))
#  define obf_section(x) __attribute__((section(x)))
#endif

#define obf_cast(T, v)  static_cast<T>(v)
#define obf_rcast(T, v) reinterpret_cast<T>(v)
#define obf_ccast(T, v) const_cast<T>(v)

static constexpr int _obf_ctime =
__TIME__[ 0 ] + __TIME__[ 1 ] + __TIME__[ 3 ] +
__TIME__[ 4 ] + __TIME__[ 6 ] + __TIME__[ 7 ];
#define _obf_seed ((__COUNTER__ + _obf_ctime) * 2654435761u)
#define obf_rnd(mn, mx) ((mn) + (_obf_seed % ((mx) - (mn) + 1)))
#define _obf_rnd obf_rnd(1, 10)
#define obf_xor(x, y) ((x) + (y) - (2 * ((x) & (y))))

#ifdef _obf_windows
#  include <windows.h>
#endif

#if obf_enabled && obf_fake_signatures && defined(_obf_windows)
#  ifdef _obf_msvc
#    pragma section(".arch")
#    pragma section(".srdata")
#    pragma section(".xpdata")
#    pragma section(".xdata")
#    pragma section(".xtls")
#    pragma section(".themida")
#    pragma section(".vmp0")
#    pragma section(".vmp1")
#    pragma section(".vmp2")
#    pragma section(".enigma1")
#    pragma section(".enigma2")
#    pragma section(".dsstext")
#  endif
#  define obf_fake_sig(name, section, sig) \
       obf_section(section) volatile static char* name = (char*)(sig);
obf_fake_sig( _obf_sig1, ".arch", 0 )
obf_fake_sig( _obf_sig2, ".srdata", 0 )
obf_fake_sig( _obf_sig3, ".xdata", 0 )
obf_fake_sig( _obf_sig4, ".xpdata", 0 )
obf_fake_sig( _obf_sig5, ".xtls", "\x64\x65\x6E\x75\x76\x6F\x5F\x61\x74\x64\x00\x00\x00\x00\x00\x00" )
#else
#  define obf_fake_sig(name, section, sig)
#endif

#if obf_enabled && (obf_cflow_const_decryption || obf_cflow_branching)
static volatile char _oa = 'a', _ob = 'b', _oc = 'c', _od = 'd', _oe = 'e', _of = 'f',
_og = 'g', _oh = 'h', _oi = 'i', _oj = 'j', _ok = 'k', _ol = 'l', _om = 'm', _on = 'n',
_oo = 'o', _op = 'p', _oq = 'q', _or = 'r', _os = 's', _ot = 't', _ou = 'u', _ov = 'v',
_ow = 'w', _ox = 'x', _oy = 'y', _oz = 'z';
static volatile char _o0 = 0, _o1 = 1, _o2 = 2, _o3 = 3, _o4 = 4,
_o5 = 5, _o6 = 6, _o7 = 7, _o8 = 8, _o9 = 9;
static obf_noinline char _op0( ) { return 0; } static obf_noinline char _op1( ) { return 1; }
static obf_noinline char _op2( ) { return 2; } static obf_noinline char _op3( ) { return 3; }
static obf_noinline char _op4( ) { return 4; } static obf_noinline char _op5( ) { return 5; }
static obf_noinline char _op6( ) { return 6; } static obf_noinline char _op7( ) { return 7; }
static obf_noinline char _op8( ) { return 8; } static obf_noinline char _op9( ) { return 9; }

#  define _obf_true  ((((_o9 + _op7() + ((_obf_rnd * _op2()) * _op0()))) / _o8) - _o1)
#  define _obf_false ((_o3 + _op6() + ((_obf_rnd * _op3()) * _o0)) - _op9())
#else
#  define _obf_true  (1)
#  define _obf_false (0)
#endif

#if obf_enabled && obf_cflow_branching
#  define obf_block_cond(cond, block) if (cond) { block; }
#  define obf_block_true(block)  obf_block_cond(_obf_true,  block)
#  define obf_block_false(block) obf_block_cond(_obf_false, block)
#else
#  define obf_block_cond(cond, block)
#  define obf_block_true(block)
#  define obf_block_false(block)
#endif

#if obf_enabled && (obf_cflow_const_decryption || obf_cflow_branching)
volatile static obf_inline int _obf_int_proxy( double val ) {
#  if obf_indirect_branching && defined(_obf_x64) && !defined(_obf_msvc)
	asm volatile( "xor %%rax, %%rax\njz 1f\n.byte 0x00\n1:" : : : "rax" );
#  endif
	volatile double a = val * ( ( double ) _o7 - ( ( double ) _o3 * 2 ) );
	obf_block_true( obf_block_false( return _obf_rnd;) )
		obf_block_true(
			if ( _obf_rnd ) return ( int ) ( a * _obf_true );
	return _obf_rnd;
		)
		return ( int ) a;
}
#endif

#if obf_enabled && obf_indirect_branching
#  if defined(_obf_x64) && !defined(_obf_msvc)
#    define obf_indirect_branch \
         asm volatile("xor %%rax, %%rax\njz 1f\n.byte 0x00\n1:" : : : "rax");
#  else
#    define obf_indirect_branch
#  endif
#else
#  define obf_indirect_branch
#endif

#if obf_enabled && (obf_cflow_const_decryption || obf_cflow_branching)
#  define obf_segfault _obf_int_proxy(*(int*)obf_rnd(0, 0x7FFFFF))
#else
#  define obf_segfault
#endif

static void _obf_watermark_hook( const char* ) { }
typedef volatile void( *_obf_draw_ptr )( const char* );
static volatile _obf_draw_ptr _obf_watermark_orig = ( _obf_draw_ptr ) _obf_watermark_hook;
#define obf_watermark(...) \
    do { const char* _obf_wm_data[] = {__VA_ARGS__}; \
    for (volatile int _i = 0; _i < (int)(sizeof(_obf_wm_data)/sizeof(_obf_wm_data[0])); _i++) \
        _obf_watermark_orig(_obf_wm_data[_i]); } while(0)

static void _obf_decoy_main( ) {
	obf_watermark( "Stop reversing the binary",
		"Reconsider your life choices",
		"And go touch some grass", 0 );
}
static void _obf_d1( ) { _obf_decoy_main( ); } static void _obf_d2( ) { _obf_decoy_main( ); }
static void _obf_d3( ) { _obf_decoy_main( ); } static void _obf_d4( ) { _obf_decoy_main( ); }
static void _obf_d5( ) { _obf_decoy_main( ); } static void _obf_d6( ) { _obf_decoy_main( ); }
static void _obf_d7( ) { _obf_decoy_main( ); } static void _obf_d8( ) { _obf_decoy_main( ); }
static void _obf_d9( ) { _obf_decoy_main( ); } static void _obf_d10( ) { _obf_decoy_main( ); }

#ifdef __cplusplus
#undef true
#undef false
#define true  1
#define false 0

namespace obf {
	template<class T> using clean_type =
		std::remove_const_t<std::remove_reference_t<T>>;

	template<typename T, T v>
	obf_inline constexpr T ensure_constexpr( ) { return v; }
#  define obf_constexpr(x) obf::ensure_constexpr<decltype(x), x>()

	template<typename T, int N> constexpr size_t getsize( const T( & )[ N ] ) { return N; }
	template<typename T>        constexpr size_t getsize( T ) { return 1; }
	template<typename T, size_t N> constexpr static T gettype( const T( & )[ N ] );
	template<typename T>           constexpr static T gettype( T );

	template<class T, char key, size_t sz>
	obf_inline void _xord( T* data ) {
#if obf_enabled && obf_cflow_const_decryption
		for ( volatile int i = 0; i < ( int ) sz; i++ ) {
			obf_block_false( data[ i ] = obf_xor( data[ i ], _obf_int_proxy( key + 1 ) );)
				obf_block_true(
					obf_block_false( data[ i ] = obf_xor( data[ i ], _obf_int_proxy( key + 2 ) ););
			obf_block_false( data[ i ] = obf_xor( data[ i ], _obf_int_proxy( key + 3 ) ););
			obf_block_true( data[ i ] = obf_xor( data[ i ], obf_cast( T, _obf_int_proxy( key + i ) ) );)
				)
				obf_block_false( data[ i ] = obf_xor( data[ i ], _obf_int_proxy( key + 4 ) );)
		}
#else
		for ( volatile int i = 0; i < ( int ) sz; i++ )
			data[ i ] = data[ i ] ^ obf_cast( T, key + i );
#endif
	}

	template<class T, size_t sz, char key>
	class encryptor {
	public:
		constexpr encryptor( const T* data ) {
			for ( int i = 0; i < ( int ) sz; i++ )
				m_data[ i ] = data[ i ] ^ obf_cast( T, key + i );
		}
		constexpr encryptor( const T data ) { m_data[ 0 ] = data ^ key; }

		obf_inline T* decrypt( ) {
			if ( !m_decrypted ) { _xord<T, key, sz>( m_data ); m_decrypted = true; }
			return m_data;
		}
		obf_inline operator T* ( ) { return decrypt( ); }
		obf_inline operator T( ) { return decrypt( )[ 0 ]; }

		bool m_decrypted = false;
		T    m_data[ sz ] {};
	};

	template<class T, size_t sz, char key>
	class tl_decryptor {
	public:
		obf_inline tl_decryptor( const encryptor<T, sz, key>& src ) {
			for ( int i = 0; i < ( int ) sz; i++ ) m_data[ i ] = src.m_data[ i ];
		}
		obf_inline T* decrypt( ) {
			if ( !m_decrypted ) { _xord<T, key, sz>( m_data ); m_decrypted = true; }
			return m_data;
		}
		obf_inline operator T* ( ) { return decrypt( ); }
		obf_inline operator T( ) { return decrypt( )[ 0 ]; }

		bool m_decrypted = false;
		T    m_data[ sz ] {};
	};

	template<typename T, int N, int real_idx, T real_val, int idx>
	constexpr T _select_func( ) {
		T arr[ N + 1 ] = {
			obf_rcast( T,( char* ) _obf_rnd ), obf_rcast( T,_obf_d1 ), obf_rcast( T,_obf_d2 ),
			obf_rcast( T,_obf_d3 ),         obf_rcast( T,( char* ) _obf_rnd ), obf_rcast( T,0 ),
			obf_rcast( T,( char* ) _obf_rnd ), obf_rcast( T,_obf_d5 ), obf_rcast( T,( char* ) _obf_rnd ),
			obf_rcast( T,( char* ) _obf_rnd ), obf_rcast( T,real_val )
		};
		return ( idx == real_idx ) ? arr[ N ] : arr[ idx ];
	}

	template<typename T, int N, int real_idx, T real_val, int... Is>
	struct ptr_hider { static T arr[ N ]; };
	template<typename T, int N, int real_idx, T real_val, int... Is>
	T ptr_hider<T, N, real_idx, real_val, Is...>::arr[ N ] = {
		_select_func<T,N,real_idx,real_val,Is>( )...
	};
}

#if obf_enabled

#  define _obf_enc_normal(x) \
    []() { constexpr static auto r = obf::encryptor< \
        obf::clean_type<decltype(obf::gettype(x))>, \
        obf::getsize(x), (char)obf_rnd(1,255)>(x); return r; }()

#  define _obf_enc_tl(x) \
    []() { constexpr static auto _d = obf::encryptor< \
        obf::clean_type<decltype(obf::gettype(x))>, \
        obf::getsize(x), (char)obf_rnd(1,255)>(x); \
        thread_local auto _r = obf::tl_decryptor< \
        obf::clean_type<decltype(obf::gettype(x))>, \
        obf::getsize(x), (char)obf_rnd(1,255)>(_d); return _r; }()

#  define obf_hide_ptr_i(ptr, idx) \
    obf::ptr_hider<decltype(ptr),10,idx,ptr,0,1,2,3,4,5,6,7,8,9>::arr[idx]

#endif

#if obf_enabled
#  define obf(x)            _obf_enc_normal(x)
#else
#  define obf(x)            (x)
#endif
#define obf_hide_ptr(ptr)   (ptr)
#define obf_call(ptr, ...)  ((ptr)(__VA_ARGS__))
#define obf_block_true(b)
#define obf_block_false(b)
#define obf_indirect_branch
#define obf_segfault
#define obf_watermark(...)

#ifdef _obf_windows
#  define obf_call_export(lib, fn, def, ...) \
     obf_rcast(def, GetProcAddress(GetModuleHandleA(lib), fn))(__VA_ARGS__)
#elif defined(_obf_linux)
#  define obf_call_export(fn, def, ...) \
     obf_rcast(def, dlsym(RTLD_DEFAULT, fn))(__VA_ARGS__)
#endif

#if obf_enabled

#  undef  enable_obfuscation
#  undef  disable_obfuscation

#  define enable_obfuscation                                                    \
    _Pragma("push_macro(\"obf\")")                                              \
    _Pragma("push_macro(\"obf_hide_ptr\")")                                     \
    _Pragma("push_macro(\"obf_call\")")                                         \
    _Pragma("push_macro(\"obf_block_true\")")                                   \
    _Pragma("push_macro(\"obf_block_false\")")                                  \
    _Pragma("push_macro(\"obf_indirect_branch\")")                              \
    _Pragma("push_macro(\"obf_segfault\")")                                     \
    _Pragma("push_macro(\"obf_watermark\")")                                    \
    _Pragma("push_macro(\"obf_call_export\")")                                  \
                                                                                \
    _Pragma("push_macro(\"_obf_scope_open\")")                                  \
    {

#  define disable_obfuscation                                                   \
    }                                                                           \
                                                                                \
    _Pragma("pop_macro(\"obf_call_export\")")                                   \
    _Pragma("pop_macro(\"obf_watermark\")")                                     \
    _Pragma("pop_macro(\"obf_segfault\")")                                      \
    _Pragma("pop_macro(\"obf_indirect_branch\")")                               \
    _Pragma("pop_macro(\"obf_block_false\")")                                   \
    _Pragma("pop_macro(\"obf_block_true\")")                                    \
    _Pragma("pop_macro(\"obf_call\")")                                          \
    _Pragma("pop_macro(\"obf_hide_ptr\")")                                      \
    _Pragma("pop_macro(\"obf\")")                                               \
    _Pragma("pop_macro(\"_obf_scope_open\")")

#else

#  define enable_obfuscation  {
#  define disable_obfuscation }

#endif


#ifndef OBF_SALT
#  define OBF_SALT 0x1337u
#endif

#define obf_volatile_calc(x) do {          \
    volatile unsigned _t = (unsigned)(x);  \
    _t ^= obf(0xDEADBEEFu);               \
    _t += obf(0xCAFEBABEu);               \
    _t *= obf(0x13371337u);               \
    (void)_t;                              \
} while(0)

#define obf_bogus_block(seed) do {                         \
    obf_volatile_calc((unsigned)(seed) ^ (unsigned)OBF_SALT); \
    obf_indirect_branch;                                   \
} while(0)

#define obf_bogus_16()  \
    obf_bogus_block(__COUNTER__); obf_bogus_block(__COUNTER__); \
    obf_bogus_block(__COUNTER__); obf_bogus_block(__COUNTER__); \
    obf_bogus_block(__COUNTER__); obf_bogus_block(__COUNTER__); \
    obf_bogus_block(__COUNTER__); obf_bogus_block(__COUNTER__); \
    obf_bogus_block(__COUNTER__); obf_bogus_block(__COUNTER__); \
    obf_bogus_block(__COUNTER__); obf_bogus_block(__COUNTER__); \
    obf_bogus_block(__COUNTER__); obf_bogus_block(__COUNTER__); \
    obf_bogus_block(__COUNTER__); obf_bogus_block(__COUNTER__);

#define obf_bogus_64()  \
    obf_bogus_16(); obf_bogus_16(); obf_bogus_16(); obf_bogus_16();

#define obf_bogus_256() \
    obf_bogus_64(); obf_bogus_64(); obf_bogus_64(); obf_bogus_64();

#define obf_scope_begin     \
    do {                    \
    obf_bogus_256();        \
    obf_indirect_branch;    \
    obf_block_true(         \
        obf_bogus_64();     \
    )                       \
    {

#define obf_scope_end       \
    }                       \
    obf_block_false(        \
        obf_bogus_64();     \
    )                       \
    obf_bogus_256();        \
    obf_indirect_branch;    \
    } while(0)

#endif

#ifdef _obf_msvc
#  pragma section(".themida")
#  pragma section(".vmp0")
#  pragma section(".vmp1")
#  pragma section(".vmp2")
#  pragma section(".enigma1")
#  pragma section(".enigma2")
#  pragma section(".dsstext")
#  pragma section(".packhdr")
#  pragma section(".nsp0")
#  pragma section(".nsp1")
#  pragma section(".nsp2")
#endif

obf_fake_sig( _obf_sig_themida, ".themida", "\x74\x68\x65\x6D\x69\x64\x61\x00" )
obf_fake_sig( _obf_sig_vmp1, ".vmp1", 0 )
obf_fake_sig( _obf_sig_vmp2, ".vmp2", 0 )
obf_fake_sig( _obf_sig_enigma1, ".enigma1", "\x45\x6E\x69\x67\x6D\x61\x00" )
obf_fake_sig( _obf_sig_enigma2, ".enigma2", 0 )
obf_fake_sig( _obf_sig_securom, ".dsstext", "\x53\x65\x63\x75\x52\x4F\x4D\x00" )
obf_fake_sig( _obf_sig_nsp0, ".nsp0", "\x4E\x53\x50\x61\x63\x6B\x00" )
obf_fake_sig( _obf_sig_nsp1, ".nsp1", 0 )
obf_fake_sig( _obf_sig_nsp2, ".nsp2", 0 )

#if obf_inline_std
static obf_inline void obf_strcpy( char* d, const char* s ) {
	while ( ( *d++ = *s++ ) );
}
static obf_inline size_t obf_strlen( const char* s ) {
	const char* p = s; while ( *p ) p++; return p - s;
}
static obf_inline int obf_strcmp( const char* a, const char* b ) {
	while ( *a == *b++ ) if ( !*a++ ) return 0;
	return *( unsigned char* ) a - *( unsigned char* )--b;
}
static obf_inline int obf_strncmp( const char* a, const char* b, size_t n ) {
	while ( n-- ) {
		if ( *a != *b ) return *( unsigned char* ) a - *( unsigned char* ) b;
		if ( !*a ) return 0; a++; b++;
	}
	return 0;
}
static obf_inline char* obf_strstr( const char* h, const char* n ) {
	if ( !*n ) return ( char* ) h;
	for ( ; *h; h++ ) {
		const char* a = h, * b = n;
		while ( *a && *b && *a == *b ) { a++; b++; }
		if ( !*b ) return ( char* ) h;
	}
	return nullptr;
}
#endif

#endif

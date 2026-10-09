#pragma once

namespace std {
    char to_lower( char c ) {
        if ( c >= 'A' && c <= 'Z' )
            return c + ( 'a' - 'A' );
        return c;
    }

    std::size_t strlen(
        const char* str
    ) {
        const char* s;
        for ( s = str; *s; ++s );
        return ( s - str );
    }

    void* memcpy(
        void* dest,
        const void* src,
        size_t len
    ) {
        char* d = static_cast< char* >( dest );
        const char* s = static_cast< const char* >( src );
        while ( len-- )
            *d++ = *s++;
        return dest;
    }

    void* memset(
        void* dest,
        int value,
        size_t count
    ) {
        unsigned char* p = static_cast< unsigned char* >( dest );
        while ( count-- )
            *p++ = static_cast< unsigned char >( value );
        return dest;
    }

    int memcmp( const void* lhs, const void* rhs, size_t count ) {
        const unsigned char* a = static_cast< const unsigned char* >( lhs );
        const unsigned char* b = static_cast< const unsigned char* >( rhs );
        while ( count-- ) {
            if ( *a != *b )
                return *a < *b ? -1 : 1;
            a++;
            b++;
        }
        return 0;
    }

    __forceinline bool strcmp( const char* a, const char* b ) {
        while ( *a && *b ) {
            if ( *a != *b )
                return false;
            a++;
            b++;
        }
        return *a == *b;
    }

    int wcscmp(
        const wchar_t* s1,
        const wchar_t* s2
    ) {
        while ( *s1 == *s2++ )
            if ( *s1++ == '\0' )
                return ( 0 );

        return ( *( const unsigned int* )s1 - *( const unsigned int* )--s2 );
    }

    const wchar_t* wcsrchr( const wchar_t* str, wchar_t ch ) {
        const wchar_t* last = nullptr;
        while ( *str ) {
            if ( *str == ch )
                last = str;
            str++;
        }
        
        if ( ch == L'\0' )
            return str;
        return last;
    }

    bool wchar_equal_ignore_case( wchar_t c1, wchar_t c2 ) {
        if ( c1 >= L'A' && c1 <= L'Z' ) c1 += 32;
        if ( c2 >= L'A' && c2 <= L'Z' ) c2 += 32;
        return c1 == c2;
    }

    const wchar_t* wcsstr( const wchar_t* str, const wchar_t* substr ) {
        if ( !str || !substr || *substr == L'\0' )
            return str;

        for ( ; *str; ++str ) {
            const wchar_t* h = str;
            const wchar_t* n = substr;

            while ( *h && *n && wchar_equal_ignore_case( *h, *n ) ) {
                h++;
                n++;
            }

            if ( *n == L'\0' )
                return str;
        }

        return nullptr;
    }
}

unsigned long strtoul(
    const char* str,
    char** endptr,
    int base
) {
    while ( *str == ' ' || *str == '\t' || *str == '\n' || *str == '\r' )
        str++;

    bool negative = false;
    if ( *str == '-' ) {
        negative = true;
        str++;
    }
    else if ( *str == '+' ) {
        str++;
    }

    if ( base == 0 ) {
        if ( *str == '0' ) {
            str++;
            if ( *str == 'x' || *str == 'X' ) {
                base = 16;
                str++;
            }
            else {
                base = 8;
            }
        }
        else {
            base = 10;
        }
    }
    else if ( base == 16 ) {
        if ( *str == '0' && ( *( str + 1 ) == 'x' || *( str + 1 ) == 'X' ) ) {
            str += 2;
        }
    }

    unsigned long result = 0;
    bool valid_digit_found = false;

    while ( *str ) {
        int digit;

        if ( *str >= '0' && *str <= '9' ) {
            digit = *str - '0';
        }
        else if ( *str >= 'a' && *str <= 'z' ) {
            digit = *str - 'a' + 10;
        }
        else if ( *str >= 'A' && *str <= 'Z' ) {
            digit = *str - 'A' + 10;
        }
        else {
            break;
        }

        if ( digit >= base ) {
            break;
        }

        valid_digit_found = true;

        if ( result > ( ULONG_MAX - digit ) / base ) {
            result = ULONG_MAX;
            break;
        }

        result = result * base + digit;
        str++;
    }

    if ( endptr ) {
        *endptr = const_cast< char* >( valid_digit_found ? str : str - valid_digit_found );
    }

    return negative ? static_cast< unsigned long >( -static_cast< long >( result ) ) : result;
}

bool strstr_lowercase( const char* haystack, const char* needle ) {
    if ( !haystack || !needle )
        return false;

    auto haystack_len = std::strlen( haystack );
    auto needle_len = std::strlen( needle );

    if ( needle_len > haystack_len )
        return false;

    for ( std::size_t i = 0; i <= haystack_len - needle_len; i++ ) {
        bool match = true;
        for ( std::size_t j = 0; j < needle_len; j++ ) {
            if ( std::to_lower( haystack[ i + j ] ) != std::to_lower( needle[ j ] ) ) {
                match = false;
                break;
            }
        }
        if ( match )
            return true;
    }
    return false;
}
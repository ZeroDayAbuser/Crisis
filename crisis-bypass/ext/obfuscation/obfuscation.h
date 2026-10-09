#pragma once

#define obf(x)           (x)
#define obf_inline       __forceinline
#define obf_noinline     __declspec(noinline)
#define obf_constexpr    constexpr

#define obf_bogus_16()   do {} while(0)
#define obf_bogus_64()   do {} while(0)
#define obf_bogus_256()  do {} while(0)

#define obf_indirect_branch  ((void)0)
#define obf_scope_begin      {
#define obf_scope_end        }
#define obf_segfault         __debugbreak()
#define obf_watermark(...)   ((void)0)

#define _obf_sig_themida  nullptr
#define _obf_sig_vmp1     nullptr
#define _obf_sig_enigma1  nullptr
#define _obf_sig_enigma2  nullptr
#define _obf_sig_securom  nullptr
#define _obf_sig_nsp0     nullptr

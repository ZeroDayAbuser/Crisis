

#ifndef UNICORN_ENGINE_H
#define UNICORN_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"
#include <stdarg.h>

#if defined(UNICORN_HAS_OSXKERNEL)
#include <libkern/libkern.h>
#else
#include <stdlib.h>
#include <stdio.h>
#endif

struct uc_struct;
typedef struct uc_struct uc_engine;

typedef size_t uc_hook;

#include "m68k.h"
#include "x86.h"
#include "arm.h"
#include "arm64.h"
#include "mips.h"
#include "sparc.h"
#include "ppc.h"
#include "riscv.h"
#include "s390x.h"
#include "tricore.h"

#ifdef __GNUC__
#define DEFAULT_VISIBILITY __attribute__((visibility("default")))
#else
#define DEFAULT_VISIBILITY
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4201)
#pragma warning(disable : 4100)
#ifdef UNICORN_SHARED
#define UNICORN_EXPORT __declspec(dllexport)
#else
#define UNICORN_EXPORT
#endif
#else
#ifdef __GNUC__
#define UNICORN_EXPORT __attribute__((visibility("default")))
#else
#define UNICORN_EXPORT
#endif
#endif

#ifdef __GNUC__
#define UNICORN_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define UNICORN_DEPRECATED __declspec(deprecated)
#else
#pragma message(                                                               \
    "WARNING: You need to implement UNICORN_DEPRECATED for this compiler")
#define UNICORN_DEPRECATED
#endif

#define UC_API_MAJOR 2
#define UC_API_MINOR 1
#define UC_API_PATCH 4
#define UC_API_EXTRA 255

#define UC_VERSION_MAJOR UC_API_MAJOR
#define UC_VERSION_MINOR UC_API_MINOR
#define UC_VERSION_PATCH UC_API_PATCH
#define UC_VERSION_EXTRA UC_API_EXTRA

#define UC_MAKE_VERSION(major, minor) (((major) << 24) + ((minor) << 16))

#define UC_SECOND_SCALE 1000000
#define UC_MILISECOND_SCALE 1000

typedef enum uc_arch {
    UC_ARCH_ARM = 1,
    UC_ARCH_ARM64,
    UC_ARCH_MIPS,
    UC_ARCH_X86,
    UC_ARCH_PPC,
    UC_ARCH_SPARC,
    UC_ARCH_M68K,
    UC_ARCH_RISCV,
    UC_ARCH_S390X,
    UC_ARCH_TRICORE,
    UC_ARCH_MAX,
} uc_arch;

typedef enum uc_mode {
    UC_MODE_LITTLE_ENDIAN = 0,
    UC_MODE_BIG_ENDIAN = 1 << 30,

    UC_MODE_ARM = 0,
    UC_MODE_THUMB = 1 << 4,
    UC_MODE_MCLASS = 1 << 5,
    UC_MODE_V8 = 1 << 6,
    UC_MODE_ARMBE8 = 1 << 10,

    UC_MODE_ARM926 = 1 << 7,
    UC_MODE_ARM946 = 1 << 8,
    UC_MODE_ARM1176 = 1 << 9,

    UC_MODE_MICRO = 1 << 4,
    UC_MODE_MIPS3 = 1 << 5,
    UC_MODE_MIPS32R6 = 1 << 6,
    UC_MODE_MIPS32 = 1 << 2,
    UC_MODE_MIPS64 = 1 << 3,

    UC_MODE_16 = 1 << 1,
    UC_MODE_32 = 1 << 2,
    UC_MODE_64 = 1 << 3,

    UC_MODE_PPC32 = 1 << 2,
    UC_MODE_PPC64 = 1 << 3,
    UC_MODE_QPX =
        1 << 4,

    UC_MODE_SPARC32 = 1 << 2,
    UC_MODE_SPARC64 = 1 << 3,
    UC_MODE_V9 = 1 << 4,

    UC_MODE_RISCV32 = 1 << 2,
    UC_MODE_RISCV64 = 1 << 3,

} uc_mode;

typedef enum uc_err {
    UC_ERR_OK = 0,
    UC_ERR_NOMEM,
    UC_ERR_ARCH,
    UC_ERR_HANDLE,
    UC_ERR_MODE,
    UC_ERR_VERSION,
    UC_ERR_READ_UNMAPPED,

    UC_ERR_WRITE_UNMAPPED,
    UC_ERR_FETCH_UNMAPPED,
    UC_ERR_HOOK,
    UC_ERR_INSN_INVALID,

    UC_ERR_MAP,
    UC_ERR_WRITE_PROT,

    UC_ERR_READ_PROT,

    UC_ERR_FETCH_PROT,

    UC_ERR_ARG,

    UC_ERR_READ_UNALIGNED,
    UC_ERR_WRITE_UNALIGNED,
    UC_ERR_FETCH_UNALIGNED,
    UC_ERR_HOOK_EXIST,
    UC_ERR_RESOURCE,
    UC_ERR_EXCEPTION,
    UC_ERR_OVERFLOW,
} uc_err;

typedef void (*uc_cb_hookcode_t)(uc_engine *uc, uint64_t address, uint32_t size,
                                 void *user_data);

typedef void (*uc_cb_hookintr_t)(uc_engine *uc, uint32_t intno,
                                 void *user_data);

typedef bool (*uc_cb_hookinsn_invalid_t)(uc_engine *uc, void *user_data);

typedef uint32_t (*uc_cb_insn_in_t)(uc_engine *uc, uint32_t port, int size,
                                    void *user_data);

typedef void (*uc_cb_insn_out_t)(uc_engine *uc, uint32_t port, int size,
                                 uint32_t value, void *user_data);

typedef enum uc_prot {
    UC_PROT_NONE = 0,
    UC_PROT_READ = 1,
    UC_PROT_WRITE = 2,
    UC_PROT_EXEC = 4,
    UC_PROT_ALL = 7,
} uc_prot;

struct uc_tlb_entry {
    uint64_t paddr;
    uc_prot perms;
};

typedef struct uc_tlb_entry uc_tlb_entry;

typedef enum uc_mem_type {
    UC_MEM_READ = 16,
    UC_MEM_WRITE,
    UC_MEM_FETCH,
    UC_MEM_READ_UNMAPPED,
    UC_MEM_WRITE_UNMAPPED,
    UC_MEM_FETCH_UNMAPPED,
    UC_MEM_WRITE_PROT,
    UC_MEM_READ_PROT,
    UC_MEM_FETCH_PROT,
    UC_MEM_READ_AFTER,
} uc_mem_type;

typedef bool (*uc_cb_tlbevent_t)(uc_engine *uc, uint64_t vaddr,
                                 uc_mem_type type, uc_tlb_entry *result,
                                 void *user_data);

typedef struct uc_tb {
    uint64_t pc;
    uint16_t icount;
    uint16_t size;
} uc_tb;

typedef void (*uc_hook_edge_gen_t)(uc_engine *uc, uc_tb *cur_tb, uc_tb *prev_tb,
                                   void *user_data);

typedef void (*uc_hook_tcg_op_2)(uc_engine *uc, uint64_t address, uint64_t arg1,
                                 uint64_t arg2, uint32_t size, void *user_data);

typedef uc_hook_tcg_op_2 uc_hook_tcg_sub_t;

typedef uint64_t (*uc_cb_mmio_read_t)(uc_engine *uc, uint64_t offset,
                                      unsigned size, void *user_data);

typedef void (*uc_cb_mmio_write_t)(uc_engine *uc, uint64_t offset,
                                   unsigned size, uint64_t value,
                                   void *user_data);

typedef enum uc_tcg_op_code {
    UC_TCG_OP_SUB = 0,
} uc_tcg_op_code;

typedef enum uc_tcg_op_flag {
    UC_TCG_OP_FLAG_CMP = 1 << 0,
    UC_TCG_OP_FLAG_DIRECT = 1 << 1
} uc_tcg_op_flag;

typedef enum uc_hook_type {
    UC_HOOK_INTR = 1 << 0,
    UC_HOOK_INSN = 1 << 1,
    UC_HOOK_CODE = 1 << 2,
    UC_HOOK_BLOCK = 1 << 3,
    UC_HOOK_MEM_READ_UNMAPPED = 1 << 4,
    UC_HOOK_MEM_WRITE_UNMAPPED = 1 << 5,
    UC_HOOK_MEM_FETCH_UNMAPPED = 1 << 6,
    UC_HOOK_MEM_READ_PROT = 1 << 7,
    UC_HOOK_MEM_WRITE_PROT = 1 << 8,
    UC_HOOK_MEM_FETCH_PROT = 1 << 9,
    UC_HOOK_MEM_READ = 1 << 10,
    UC_HOOK_MEM_WRITE = 1 << 11,
    UC_HOOK_MEM_FETCH = 1 << 12,
    UC_HOOK_MEM_READ_AFTER = 1 << 13,
    UC_HOOK_INSN_INVALID = 1 << 14,
    UC_HOOK_EDGE_GENERATED = 1 << 15,
    UC_HOOK_TCG_OPCODE = 1 << 16,
    UC_HOOK_TLB_FILL = 1 << 17,
} uc_hook_type;

#define UC_HOOK_MEM_UNMAPPED                                                   \
    (UC_HOOK_MEM_READ_UNMAPPED + UC_HOOK_MEM_WRITE_UNMAPPED +                  \
     UC_HOOK_MEM_FETCH_UNMAPPED)
#define UC_HOOK_MEM_PROT                                                       \
    (UC_HOOK_MEM_READ_PROT + UC_HOOK_MEM_WRITE_PROT + UC_HOOK_MEM_FETCH_PROT)
#define UC_HOOK_MEM_READ_INVALID                                               \
    (UC_HOOK_MEM_READ_PROT + UC_HOOK_MEM_READ_UNMAPPED)
#define UC_HOOK_MEM_WRITE_INVALID                                              \
    (UC_HOOK_MEM_WRITE_PROT + UC_HOOK_MEM_WRITE_UNMAPPED)
#define UC_HOOK_MEM_FETCH_INVALID                                              \
    (UC_HOOK_MEM_FETCH_PROT + UC_HOOK_MEM_FETCH_UNMAPPED)
#define UC_HOOK_MEM_INVALID (UC_HOOK_MEM_UNMAPPED + UC_HOOK_MEM_PROT)
#define UC_HOOK_MEM_VALID                                                      \
    (UC_HOOK_MEM_READ + UC_HOOK_MEM_WRITE + UC_HOOK_MEM_FETCH)

typedef void (*uc_cb_hookmem_t)(uc_engine *uc, uc_mem_type type,
                                uint64_t address, int size, int64_t value,
                                void *user_data);

typedef bool (*uc_cb_eventmem_t)(uc_engine *uc, uc_mem_type type,
                                 uint64_t address, int size, int64_t value,
                                 void *user_data);

typedef struct uc_mem_region {
    uint64_t begin;
    uint64_t end;
    uint32_t perms;
} uc_mem_region;

typedef enum uc_query_type {
    UC_QUERY_MODE = 1,
    UC_QUERY_PAGE_SIZE,
    UC_QUERY_ARCH,
    UC_QUERY_TIMEOUT,
} uc_query_type;

#define UC_CTL_IO_NONE (0)
#define UC_CTL_IO_WRITE (1)
#define UC_CTL_IO_READ (2)
#define UC_CTL_IO_READ_WRITE (UC_CTL_IO_WRITE | UC_CTL_IO_READ)

#define UC_CTL(type, nr, rw)                                                   \
    (uc_control_type)((type) | ((nr) << 26) | ((rw) << 30))
#define UC_CTL_NONE(type, nr) UC_CTL(type, nr, UC_CTL_IO_NONE)
#define UC_CTL_READ(type, nr) UC_CTL(type, nr, UC_CTL_IO_READ)
#define UC_CTL_WRITE(type, nr) UC_CTL(type, nr, UC_CTL_IO_WRITE)
#define UC_CTL_READ_WRITE(type, nr) UC_CTL(type, nr, UC_CTL_IO_READ_WRITE)

typedef enum uc_tlb_type {
    UC_TLB_CPU = 0,
    UC_TLB_VIRTUAL
} uc_tlb_type;

typedef enum uc_control_type {
    UC_CTL_UC_MODE = 0,
    UC_CTL_UC_PAGE_SIZE,
    UC_CTL_UC_ARCH,
    UC_CTL_UC_TIMEOUT,
    UC_CTL_UC_USE_EXITS,
    UC_CTL_UC_EXITS_CNT,
    UC_CTL_UC_EXITS,

    UC_CTL_CPU_MODEL,
    UC_CTL_TB_REQUEST_CACHE,
    UC_CTL_TB_REMOVE_CACHE,
    UC_CTL_TB_FLUSH,
    UC_CTL_TLB_FLUSH,
    UC_CTL_TLB_TYPE,
    UC_CTL_TCG_BUFFER_SIZE,
    UC_CTL_CONTEXT_MODE,
} uc_control_type;

#define uc_ctl_get_mode(uc, mode)                                              \
    uc_ctl(uc, UC_CTL_READ(UC_CTL_UC_MODE, 1), (mode))
#define uc_ctl_get_page_size(uc, ptr)                                          \
    uc_ctl(uc, UC_CTL_READ(UC_CTL_UC_PAGE_SIZE, 1), (ptr))
#define uc_ctl_set_page_size(uc, page_size)                                    \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_UC_PAGE_SIZE, 1), (page_size))
#define uc_ctl_get_arch(uc, arch)                                              \
    uc_ctl(uc, UC_CTL_READ(UC_CTL_UC_ARCH, 1), (arch))
#define uc_ctl_get_timeout(uc, ptr)                                            \
    uc_ctl(uc, UC_CTL_READ(UC_CTL_UC_TIMEOUT, 1), (ptr))
#define uc_ctl_exits_enable(uc)                                                \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_UC_USE_EXITS, 1), 1)
#define uc_ctl_exits_disable(uc)                                               \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_UC_USE_EXITS, 1), 0)
#define uc_ctl_get_exits_cnt(uc, ptr)                                          \
    uc_ctl(uc, UC_CTL_READ(UC_CTL_UC_EXITS_CNT, 1), (ptr))
#define uc_ctl_get_exits(uc, buffer, len)                                      \
    uc_ctl(uc, UC_CTL_READ(UC_CTL_UC_EXITS, 2), (buffer), (len))
#define uc_ctl_set_exits(uc, buffer, len)                                      \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_UC_EXITS, 2), (buffer), (len))
#define uc_ctl_get_cpu_model(uc, model)                                        \
    uc_ctl(uc, UC_CTL_READ(UC_CTL_CPU_MODEL, 1), (model))
#define uc_ctl_set_cpu_model(uc, model)                                        \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_CPU_MODEL, 1), (model))
#define uc_ctl_remove_cache(uc, address, end)                                  \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_TB_REMOVE_CACHE, 2), (address), (end))
#define uc_ctl_request_cache(uc, address, tb)                                  \
    uc_ctl(uc, UC_CTL_READ_WRITE(UC_CTL_TB_REQUEST_CACHE, 2), (address), (tb))
#define uc_ctl_flush_tb(uc) uc_ctl(uc, UC_CTL_WRITE(UC_CTL_TB_FLUSH, 0))
#define uc_ctl_flush_tlb(uc) uc_ctl(uc, UC_CTL_WRITE(UC_CTL_TLB_FLUSH, 0))
#define uc_ctl_tlb_mode(uc, mode)                                              \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_TLB_TYPE, 1), (mode))
#define uc_ctl_get_tcg_buffer_size(uc, size)                                   \
    uc_ctl(uc, UC_CTL_READ(UC_CTL_TCG_BUFFER_SIZE, 1), (size))
#define uc_ctl_set_tcg_buffer_size(uc, size)                                   \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_TCG_BUFFER_SIZE, 1), (size))
#define uc_ctl_context_mode(uc, mode)                                          \
    uc_ctl(uc, UC_CTL_WRITE(UC_CTL_CONTEXT_MODE, 1), (mode))

struct uc_context;
typedef struct uc_context uc_context;

UNICORN_EXPORT
unsigned int uc_version(unsigned int *major, unsigned int *minor);

UNICORN_EXPORT
bool uc_arch_supported(uc_arch arch);

UNICORN_EXPORT
uc_err uc_open(uc_arch arch, uc_mode mode, uc_engine **uc);

UNICORN_EXPORT
uc_err uc_close(uc_engine *uc);

UNICORN_EXPORT
uc_err uc_query(uc_engine *uc, uc_query_type type, size_t *result);

UNICORN_EXPORT
uc_err uc_ctl(uc_engine *uc, uc_control_type control, ...);

UNICORN_EXPORT
uc_err uc_errno(uc_engine *uc);

UNICORN_EXPORT
const char *uc_strerror(uc_err code);

UNICORN_EXPORT
uc_err uc_reg_write(uc_engine *uc, int regid, const void *value);

UNICORN_EXPORT
uc_err uc_reg_read(uc_engine *uc, int regid, void *value);

UNICORN_EXPORT
uc_err uc_reg_write2(uc_engine *uc, int regid, const void *value, size_t *size);

UNICORN_EXPORT
uc_err uc_reg_read2(uc_engine *uc, int regid, void *value, size_t *size);

UNICORN_EXPORT
uc_err uc_reg_write_batch(uc_engine *uc, int const *regs, void *const *vals,
                          int count);

UNICORN_EXPORT
uc_err uc_reg_read_batch(uc_engine *uc, int const *regs, void **vals,
                         int count);

UNICORN_EXPORT
uc_err uc_reg_write_batch2(uc_engine *uc, int const *regs,
                           const void *const *vals, size_t *sizes, int count);

UNICORN_EXPORT
uc_err uc_reg_read_batch2(uc_engine *uc, int const *regs, void *const *vals,
                          size_t *sizes, int count);

UNICORN_EXPORT
uc_err uc_mem_write(uc_engine *uc, uint64_t address, const void *bytes,
                    uint64_t size);

UNICORN_EXPORT
uc_err uc_mem_read(uc_engine *uc, uint64_t address, void *bytes, uint64_t size);

UNICORN_EXPORT
uc_err uc_vmem_read(uc_engine *uc, uint64_t address, uc_prot prot,
                           void *bytes, size_t size);

UNICORN_EXPORT
uc_err uc_vmem_write(uc_engine *uc, uint64_t address, uc_prot prot,
                           void *bytes, size_t size);

UNICORN_EXPORT
uc_err uc_vmem_translate(uc_engine *uc, uint64_t address, uc_prot prot,
                              uint64_t *paddress);

UNICORN_EXPORT
uc_err uc_emu_start(uc_engine *uc, uint64_t begin, uint64_t until,
                    uint64_t timeout, size_t count);

UNICORN_EXPORT
uc_err uc_emu_stop(uc_engine *uc);

UNICORN_EXPORT
uc_err uc_hook_add(uc_engine *uc, uc_hook *hh, int type, void *callback,
                   void *user_data, uint64_t begin, uint64_t end, ...);

UNICORN_EXPORT
uc_err uc_hook_del(uc_engine *uc, uc_hook hh);

typedef enum uc_context_content {
    UC_CTL_CONTEXT_CPU = 1,
    UC_CTL_CONTEXT_MEMORY = 2,
} uc_context_content;

UNICORN_EXPORT
uc_err uc_mem_map(uc_engine *uc, uint64_t address, uint64_t size,
                  uint32_t perms);

UNICORN_EXPORT
uc_err uc_mem_map_ptr(uc_engine *uc, uint64_t address, uint64_t size,
                      uint32_t perms, void *ptr);

UNICORN_EXPORT
uc_err uc_mmio_map(uc_engine *uc, uint64_t address, uint64_t size,
                   uc_cb_mmio_read_t read_cb, void *user_data_read,
                   uc_cb_mmio_write_t write_cb, void *user_data_write);

UNICORN_EXPORT
uc_err uc_mem_unmap(uc_engine *uc, uint64_t address, uint64_t size);

UNICORN_EXPORT
uc_err uc_mem_protect(uc_engine *uc, uint64_t address, uint64_t size,
                      uint32_t perms);

UNICORN_EXPORT
uc_err uc_mem_regions(uc_engine *uc, uc_mem_region **regions, uint32_t *count);

UNICORN_EXPORT
uc_err uc_context_alloc(uc_engine *uc, uc_context **context);

UNICORN_EXPORT
uc_err uc_free(void *mem);

UNICORN_EXPORT
uc_err uc_context_save(uc_engine *uc, uc_context *context);

UNICORN_EXPORT
uc_err uc_context_reg_write(uc_context *ctx, int regid, const void *value);

UNICORN_EXPORT
uc_err uc_context_reg_read(uc_context *ctx, int regid, void *value);

UNICORN_EXPORT
uc_err uc_context_reg_write2(uc_context *ctx, int regid, const void *value,
                             size_t *size);

UNICORN_EXPORT
uc_err uc_context_reg_read2(uc_context *ctx, int regid, void *value,
                            size_t *size);

UNICORN_EXPORT
uc_err uc_context_reg_write_batch(uc_context *ctx, int const *regs,
                                  void *const *vals, int count);

UNICORN_EXPORT
uc_err uc_context_reg_read_batch(uc_context *ctx, int const *regs, void **vals,
                                 int count);

UNICORN_EXPORT
uc_err uc_context_reg_write_batch2(uc_context *ctx, int const *regs,
                                   const void *const *vals, size_t *sizes,
                                   int count);

UNICORN_EXPORT
uc_err uc_context_reg_read_batch2(uc_context *ctx, int const *regs,
                                  void *const *vals, size_t *sizes, int count);

UNICORN_EXPORT
uc_err uc_context_restore(uc_engine *uc, uc_context *context);

UNICORN_EXPORT
size_t uc_context_size(uc_engine *uc);

UNICORN_EXPORT
uc_err uc_context_free(uc_context *context);

#ifdef __cplusplus
}
#endif

#endif

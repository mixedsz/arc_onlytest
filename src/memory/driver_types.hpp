#pragma once

#include <cstdint>

#if defined(__has_include)
#  if __has_include("astrum_build_config.hpp")
#    include "astrum_build_config.hpp"
#  endif
#endif


#ifndef ASTRUM_CMD_NULL
#define ASTRUM_CMD_NULL  0u
#endif
#ifndef ASTRUM_CMD_READ
#define ASTRUM_CMD_READ  1u
#endif
#ifndef ASTRUM_CMD_WRITE
#define ASTRUM_CMD_WRITE 2u
#endif
#ifndef ASTRUM_CMD_QUERY
#define ASTRUM_CMD_QUERY 3u
#endif
#ifndef ASTRUM_CMD_ALLOC
#define ASTRUM_CMD_ALLOC 4u
#endif

#define ASTRUM_CMD_READ_BATCH ((ASTRUM_CMD_READ) ^ 0xC0DEBA5Eu)
static_assert(ASTRUM_CMD_READ_BATCH != ASTRUM_CMD_NULL,  "batch cmd collides with NULL — re-roll cmd values");
static_assert(ASTRUM_CMD_READ_BATCH != ASTRUM_CMD_READ,  "batch cmd collides with READ — re-roll cmd values");
static_assert(ASTRUM_CMD_READ_BATCH != ASTRUM_CMD_WRITE, "batch cmd collides with WRITE — re-roll cmd values");
static_assert(ASTRUM_CMD_READ_BATCH != ASTRUM_CMD_QUERY, "batch cmd collides with QUERY — re-roll cmd values");
static_assert(ASTRUM_CMD_READ_BATCH != ASTRUM_CMD_ALLOC, "batch cmd collides with ALLOC — re-roll cmd values");

#ifndef ASTRUM_AUTH_TOKEN
#define ASTRUM_AUTH_TOKEN 0u
#endif

namespace astrum_cmd {
  inline uint32_t null  = ASTRUM_CMD_NULL;
  inline uint32_t read  = ASTRUM_CMD_READ;
  inline uint32_t write = ASTRUM_CMD_WRITE;
  inline uint32_t query = ASTRUM_CMD_QUERY;
  inline uint32_t alloc = ASTRUM_CMD_ALLOC;
  inline uint32_t token = ASTRUM_AUTH_TOKEN;

  inline void set_token(uint32_t t) { token = t; }
}

enum class e_command : uint32_t {
  null = ASTRUM_CMD_NULL,
  mem_read = ASTRUM_CMD_READ,
  mem_write = ASTRUM_CMD_WRITE,
  proc_query = ASTRUM_CMD_QUERY,
  mem_alloc = ASTRUM_CMD_ALLOC,
};

constexpr uint64_t kAstrumRequestMagic = 0xA5724DC0DEA11Eull;


class c_request {
public:
  c_request(e_command command, void *data, uint64_t length)
      : m_magic(kAstrumRequestMagic), m_data(data), m_length(length),
        m_command(command), m_token(astrum_cmd::token) {}

  c_request(e_command command, void *data, uint64_t length, uint32_t token)
      : m_magic(kAstrumRequestMagic), m_data(data), m_length(length),
        m_command(command), m_token(token) {}

  ~c_request() = default;

  template <typename T> T *get() const {
    if (!m_data || !m_length)
      return nullptr;
    if (sizeof(T) != m_length)
      return nullptr;
    return (T *)m_data;
  }

  e_command get_command() const { return m_command; }
  uint32_t  get_token()   const { return m_token; }
  uint64_t  get_magic()   const { return m_magic; }

private:
  uint64_t m_magic = 0;   // MUST be first — mirrors kernel layout
  void *m_data = nullptr;
  uint64_t m_length = 0;
  e_command m_command = e_command::null;
  uint32_t m_token = 0;
};


typedef struct _mem_copy_request {
  uint32_t process_id = 0;
  uint64_t source = 0;
  void *dest = nullptr;
  uint64_t size = 0;
} mem_copy_request, *pmem_copy_request;


typedef struct _proc_info_request {
  uint32_t process_id = 0;
  void *peb = nullptr;
  void *base_address = nullptr;
  uint64_t cr3 = 0;
} proc_info_request, *pproc_info_request;


typedef struct _mem_alloc_request {
  uint32_t process_id = 0;
  uint64_t size = 0;
  void    *address = nullptr;   // OUT: chosen base address
} mem_alloc_request, *pmem_alloc_request;

typedef struct _mem_read_batch_entry {
  uint32_t process_id;
  uint64_t source;
  void    *dest;
  uint64_t size;
  int32_t  status;          // OUT: 0 on success, NTSTATUS on failure
  uint32_t _pad;
} mem_read_batch_entry, *pmem_read_batch_entry;

typedef struct _mem_read_batch_request {
  uint32_t count;
  uint32_t _pad;
  mem_read_batch_entry *entries;
} mem_read_batch_request, *pmem_read_batch_request;

constexpr uint32_t kMaxBatchEntries    = 256u;
constexpr uint64_t kMaxBatchTotalBytes = 64ull * 1024 * 1024;

namespace astrum_cmd {
  inline uint32_t batch() { return ASTRUM_CMD_READ_BATCH; }
}

#ifndef SOLARAX_SCATTER_ENTRIES_DEFINED
#define SOLARAX_SCATTER_ENTRIES_DEFINED

typedef struct _SCATTER_WRITE_ENTRY {
  uint64_t address;
  unsigned int value;
  unsigned int _pad;
} SCATTER_WRITE_ENTRY, *PSCATTER_WRITE_ENTRY;

typedef struct _SCATTER_READ_ENTRY {
  uint64_t address;
  unsigned int size;
  unsigned int status;
  unsigned char data[16];
} SCATTER_READ_ENTRY, *PSCATTER_READ_ENTRY;

#endif /* SOLARAX_SCATTER_ENTRIES_DEFINED */

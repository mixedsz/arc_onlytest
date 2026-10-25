#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>
#include <winternl.h> // NTSTATUS

#include "driver_types.hpp"

// Must match the kernel shared-ring layout.
constexpr uint32_t kAstrumSharedMagic           = 0xA5720000u;
constexpr uint32_t kAstrumSharedProtocolVersion = 1u;
constexpr uint32_t kAstrumSharedRegionSize      = 16u * 1024u;

struct astrum_shared_region {
  uint32_t magic;
  uint32_t protocol_version;
  uint8_t  nonce[32];
  volatile uint32_t req_seq;
  volatile uint32_t rsp_seq;
  uint32_t opcode;
  int32_t  status;
  uint32_t payload_size;
  uint32_t _pad;
  uint8_t  payload[kAstrumSharedRegionSize - 64];
};
static_assert(sizeof(astrum_shared_region) == kAstrumSharedRegionSize,
              "astrum_shared_region size drifted");

class c_memory {
public:
  c_memory() = default;
  ~c_memory() {
    detach();
    if (m_ring_view) {
      UnmapViewOfFile(m_ring_view);
      m_ring_view = nullptr;
    }
    if (m_ring_section && m_ring_section != INVALID_HANDLE_VALUE) {
      CloseHandle(m_ring_section);
      m_ring_section = INVALID_HANDLE_VALUE;
    }
  }

  bool open_ring(const wchar_t* section_name, const char* nonce_hex);
  bool has_ring() const { return m_ring_ready; }

  bool attach(uint32_t process_id);
  bool attach(std::wstring process_name);
  void detach();

  bool read(uint64_t src, void *dest, uint64_t size);
  bool write(void *src, uint64_t dest, uint64_t size);

  // Check each entry's status after a successful batch submission.
  bool read_batch(mem_read_batch_entry *entries, uint32_t count);

  template <typename T> T read(uint64_t src) {
    T buf{};
    read(src, &buf, sizeof(T));
    return buf;
  }

  template <typename T> bool write(uint64_t dest, T val) {
    return write(&val, dest, sizeof(T));
  }

  uint64_t get_module_base(std::wstring module_name);

  uint64_t alloc(uint64_t size, uint32_t protect = PAGE_READWRITE);
  bool free(uint64_t address);
  bool protect(uint64_t address, uint64_t size, uint32_t prot);
  bool hide_process(uint32_t pid);
  bool unhide_process(uint32_t pid);
  bool is_hidden(uint32_t pid);

  void set_auth_token(uint32_t token) { astrum_cmd::set_token(token); }

  bool scatter_write(SCATTER_WRITE_ENTRY *entries, size_t count);
  bool scatter_read(SCATTER_READ_ENTRY *entries, size_t count);

  uint32_t get_pid() const { return m_pid; }
  uint32_t get_process_id(std::wstring process_name) const;

  void *get_peb() const { return m_proc_data.peb; }
  uint64_t get_dtb() const { return m_proc_data.cr3; }
  void *get_base_address() const { return m_proc_data.base_address; }

  bool is_driver_connected() const { return m_connected || m_ring_ready; }
  bool transport_lost() const { return m_transport_lost; }
  bool session_ended() const { return m_session_ended; }
  void clear_session_ended() { m_session_ended = false; }

  bool try_connect();

  bool check_alive();

private:
  NTSTATUS dispatch(c_request *request);
  bool resolve_transport();
  bool test_proc_query(uint32_t pid, proc_info_request *out, const char *label);
  bool check_transport_ready();
  bool reconnect_if_needed();

  NTSTATUS ring_submit(uint32_t opcode, const void* body, uint32_t body_size,
                       void* out_body, uint32_t out_body_size,
                       const void* in_bounce, uint32_t in_bounce_size,
                       void* out_bounce, uint32_t out_bounce_size);

  uint32_t m_pid = 0;
  bool m_connected = false;
  proc_info_request m_proc_data = {};

  using nt_call_t = NTSTATUS(__stdcall *)(void *, void *, void *, void *);
  nt_call_t m_transport_fn = nullptr;
  int m_transport_idx = -1;
  bool m_transport_lost = false;
  bool m_session_ended = false;

  std::mutex m_dispatch_mtx;

  HANDLE                m_ring_section  = INVALID_HANDLE_VALUE;
  astrum_shared_region* m_ring_view     = nullptr;
  uint8_t               m_ring_nonce[32] = {};
  uint32_t              m_ring_next_seq = 0;
  bool                  m_ring_ready    = false;
  std::mutex            m_ring_mtx;
};

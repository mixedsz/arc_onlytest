#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <string_view>
#include <windows.h>
#include <tlhelp32.h>
#include <winternl.h> // NTSTATUS

#include "driver_memory.hpp"
#include "config/build_config.hpp"

#ifndef STATUS_PROCEDURE_NOT_FOUND
#define STATUS_PROCEDURE_NOT_FOUND ((NTSTATUS)0xC000007AL)
#endif
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#endif
#ifndef STATUS_TIMEOUT
#define STATUS_TIMEOUT ((NTSTATUS)0x00000102L)
#endif

static constexpr uint32_t kAstrumBounceOffset = (uint32_t)sizeof(mem_copy_request);

static void drv_log(const char * /*fmt*/, ...) {
}

bool c_memory::resolve_transport() {
  if (m_transport_fn)
    return true;

  auto *ntdll = GetModuleHandleA("ntdll.dll");
  if (!ntdll) {
    drv_log("resolve_transport: ntdll.dll not found");
    return false;
  }

  static const char *k_candidates[] = {
      "NtQueryAuxiliaryCounterFrequency",
      "NtConvertBetweenAuxiliaryCounterAndPerformanceCounter",
  };

  for (int i = 0; i < _countof(k_candidates); ++i) {
    auto fn =
        reinterpret_cast<nt_call_t>(GetProcAddress(ntdll, k_candidates[i]));
    drv_log("resolve_transport: %s -> %p", k_candidates[i],
            reinterpret_cast<void *>(fn));
    if (fn) {
      m_transport_fn = fn;
      m_transport_idx = i;
      return true;
    }
  }

  drv_log("resolve_transport: no NT function found");
  return false;
}

using _dispatch_fn_t = NTSTATUS(__stdcall *)(void *, void *, void *, void *);
static NTSTATUS dispatch_inner(_dispatch_fn_t fn, int idx, c_request *request) {
  uint64_t dummy1 = 0, dummy2 = 0;
  NTSTATUS status = STATUS_UNSUCCESSFUL;
  __try {
    if (idx == 0) {
      status = fn(&dummy1, request, nullptr, nullptr);
    } else {
      status = fn(nullptr, request, &dummy1, &dummy2);
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    status = (NTSTATUS)0xC0000005L;
  }
  return status;
}

NTSTATUS c_memory::dispatch(c_request *request) {
  if (!request)
    return STATUS_ACCESS_VIOLATION;

  if (!m_transport_fn && !resolve_transport())
    return STATUS_PROCEDURE_NOT_FOUND;

  std::lock_guard<std::mutex> _lk(m_dispatch_mtx);
  return dispatch_inner(m_transport_fn, m_transport_idx, request);
}

bool c_memory::test_proc_query(uint32_t pid, proc_info_request *out,
                               const char *label) {
  *out = {};
  out->process_id = pid;

  out->peb = reinterpret_cast<void *>(0xDEAD);
  out->base_address = reinterpret_cast<void *>(0xDEAD);
  out->cr3 = 0xDEADDEADDEADDEADULL;

  auto request = c_request((e_command)astrum_cmd::query, out, sizeof(*out));
  NTSTATUS status = dispatch(&request);

  bool cr3_changed = (out->cr3 != 0xDEADDEADDEADDEADULL);
  bool any_changed = cr3_changed ||
                     out->peb != reinterpret_cast<void *>(0xDEAD) ||
                     out->base_address != reinterpret_cast<void *>(0xDEAD);

  drv_log("  %s pid=%u status=0x%08X peb=%p base=%p cr3=0x%llX changed=%d",
          label, pid, (unsigned)status, out->peb, out->base_address,
          (unsigned long long)out->cr3, any_changed);

  return any_changed && out->cr3 != 0 && out->cr3 != 0xDEADDEADDEADDEADULL;
}

bool c_memory::try_connect() {
  drv_log("=== try_connect ===");

  static constexpr int kMaxAttempts = 4;
  static constexpr DWORD kBackoffMs[kMaxAttempts - 1] = { 100, 500, 2000 };

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    if (attempt > 0) {
      drv_log("try_connect: retry %d/%d after %ums", attempt, kMaxAttempts - 1,
              kBackoffMs[attempt - 1]);
      Sleep(kBackoffMs[attempt - 1]);
    }

    if (!resolve_transport()) {
      drv_log("try_connect: resolve_transport failed on attempt %d", attempt + 1);
      continue;
    }

    proc_info_request info = {};
    bool ok = test_proc_query(4, &info, "System(4)");

    if (!ok) {
      DWORD self_pid = GetCurrentProcessId();
      ok = test_proc_query(self_pid, &info, "Self");
    }

    if (!ok && m_transport_idx == 0) {
      drv_log("try_connect: first transport failed, trying alternate...");
      auto *ntdll = GetModuleHandleA("ntdll.dll");
      auto fn =
          ntdll ? reinterpret_cast<nt_call_t>(GetProcAddress(
                      ntdll,
                      "NtConvertBetweenAuxiliaryCounterAndPerformanceCounter"))
                : nullptr;
      if (fn) {
        m_transport_fn = fn;
        m_transport_idx = 1;
        drv_log("try_connect: switched to NtConvertBetween... (%p)",
                reinterpret_cast<void *>(fn));
        ok = test_proc_query(4, &info, "System(4)-alt");
        if (!ok) {
          DWORD self_pid = GetCurrentProcessId();
          ok = test_proc_query(self_pid, &info, "Self-alt");
        }
      }
    }

    if (ok) {
      drv_log("try_connect: SUCCESS (transport_idx=%d)", m_transport_idx);
      m_connected = true;
      m_transport_lost = false;
      m_session_ended = false;
      return true;
    }
  }

  drv_log("try_connect: FAILED after %d attempts, driver did not modify proc_info fields", kMaxAttempts);
  m_connected = false;
  m_transport_lost = true;
  return false;
}

bool c_memory::check_alive() {
  if (!m_transport_fn)
    return false;

  proc_info_request info = {};
  info.process_id = 4;
  info.cr3 = 0xDEADDEADDEADDEADULL;

  auto request = c_request((e_command)astrum_cmd::query, &info, sizeof(info));
  dispatch(&request);

  m_connected = (info.cr3 != 0 && info.cr3 != 0xDEADDEADDEADDEADULL);
  return m_connected;
}

bool c_memory::check_transport_ready() {
  if (m_ring_ready)
    return true;

  if (m_session_ended)
    return false;

  if (!m_transport_fn) {
    m_transport_lost = true;
    return false;
  }

  if (m_connected)
    return true;

  return reconnect_if_needed();
}

bool c_memory::reconnect_if_needed() {
  if (m_session_ended)
    return false;

  if (try_connect()) {
    m_transport_lost = false;
    return true;
  }

  m_transport_lost = true;
  m_session_ended = true;
  return false;
}


static bool hex_to_bytes(const char* hex, uint8_t* out, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    auto h2 = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    int hi = h2(hex[i * 2]);
    int lo = h2(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

bool c_memory::open_ring(const wchar_t* section_name, const char* nonce_hex) {
  if (m_ring_ready) return true;
  if (!section_name || !*section_name) return false;

  HANDLE h = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, section_name);
  if (!h) return false;

  void* view = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, kAstrumSharedRegionSize);
  if (!view) {
    CloseHandle(h);
    return false;
  }

  auto* region = reinterpret_cast<astrum_shared_region*>(view);

  if (region->magic != kAstrumSharedMagic ||
      region->protocol_version != kAstrumSharedProtocolVersion) {
    UnmapViewOfFile(view);
    CloseHandle(h);
    return false;
  }

  if (nonce_hex && *nonce_hex) {
    if (strlen(nonce_hex) != 64) {
      UnmapViewOfFile(view);
      CloseHandle(h);
      return false;
    }
    uint8_t expected[32];
    if (!hex_to_bytes(nonce_hex, expected, 32)) {
      UnmapViewOfFile(view);
      CloseHandle(h);
      return false;
    }
    for (int i = 0; i < 32; ++i) {
      if (region->nonce[i] != expected[i]) {
        UnmapViewOfFile(view);
        CloseHandle(h);
        return false;
      }
      m_ring_nonce[i] = expected[i];
    }
  } else {
    for (int i = 0; i < 32; ++i) m_ring_nonce[i] = region->nonce[i];
  }

  m_ring_section  = h;
  m_ring_view     = region;
  m_ring_next_seq = region->req_seq + 1;
  m_ring_ready    = true;
  return true;
}

NTSTATUS c_memory::ring_submit(uint32_t opcode,
                               const void* body, uint32_t body_size,
                               void* out_body, uint32_t out_body_size,
                               const void* in_bounce, uint32_t in_bounce_size,
                               void* out_bounce, uint32_t out_bounce_size) {
  if (!m_ring_ready || !m_ring_view) return STATUS_UNSUCCESSFUL;
  if (body_size > sizeof(m_ring_view->payload)) return STATUS_INVALID_PARAMETER;
  if (kAstrumBounceOffset + in_bounce_size > sizeof(m_ring_view->payload))
    return STATUS_INVALID_PARAMETER;
  if (kAstrumBounceOffset + out_bounce_size > sizeof(m_ring_view->payload))
    return STATUS_INVALID_PARAMETER;

  std::lock_guard<std::mutex> _lk(m_ring_mtx);

  auto* r = m_ring_view;

  for (int i = 0; i < 32; ++i) r->nonce[i] = m_ring_nonce[i];

  for (uint32_t i = 0; i < body_size; ++i)
    r->payload[i] = ((const uint8_t*)body)[i];

  if (in_bounce && in_bounce_size) {
    for (uint32_t i = 0; i < in_bounce_size; ++i)
      r->payload[kAstrumBounceOffset + i] = ((const uint8_t*)in_bounce)[i];
  }

  r->opcode       = opcode;
  r->payload_size = body_size;
  r->status       = 0;

  _mm_sfence();
  const uint32_t my_seq = m_ring_next_seq++;
  r->req_seq = my_seq;

  uint32_t spins = 0;
  const DWORD startMs = GetTickCount64();
  const DWORD timeoutMs = 5000;
  while (r->rsp_seq != my_seq) {
    _mm_pause();
    if ((++spins & 0xFFF) == 0) SwitchToThread();
    if (spins > 0x3000000) return STATUS_UNSUCCESSFUL;
    if (GetTickCount64() - startMs > timeoutMs) {
      m_ring_ready = false;
      m_transport_lost = true;
      return STATUS_TIMEOUT;
    }
  }

  NTSTATUS s = (NTSTATUS)r->status;
  if (!(s >= 0)) return s;

  if (out_body && out_body_size) {
    const uint32_t n = out_body_size < body_size ? out_body_size : body_size;
    for (uint32_t i = 0; i < n; ++i)
      ((uint8_t*)out_body)[i] = r->payload[i];
  }
  if (out_bounce && out_bounce_size) {
    for (uint32_t i = 0; i < out_bounce_size; ++i)
      ((uint8_t*)out_bounce)[i] = r->payload[kAstrumBounceOffset + i];
  }
  return s;
}

bool c_memory::attach(uint32_t process_id) {
  if (!process_id)
    return false;

  proc_info_request info = {};
  info.process_id = process_id;

  if (m_ring_ready) {
    NTSTATUS s = ring_submit((uint32_t)e_command::proc_query,
                             &info, sizeof(info),
                             &info, sizeof(info),
                             nullptr, 0,
                             nullptr, 0);
    static uint32_t last_logged_pid = 0;
    if (process_id != last_logged_pid) {
        last_logged_pid = process_id;
        char dbgLine[256];
        std::snprintf(dbgLine, sizeof(dbgLine),
            "[drv] ring proc_query pid=%u status=0x%08X "
            "peb=0x%p base=0x%p cr3=0x%llX",
            process_id, (unsigned)s,
            info.peb, info.base_address,
            (unsigned long long)info.cr3);
        DBG_ERR(dbgLine);
    }
    if (s >= 0 && info.peb != nullptr) {
      m_connected = true;
      m_pid = process_id;
      m_proc_data = info;
      return true;
    }
    if (s >= 0 && info.base_address != nullptr && info.cr3 != 0) {
      m_connected = true;
      m_pid = process_id;
      m_proc_data = info;
      return true;
    }
    return false;
  }

  if (!m_connected && !try_connect())
    return false;

  auto request = c_request((e_command)astrum_cmd::query, &info, sizeof(info));
  NTSTATUS status = dispatch(&request);

  if (status < 0 || info.peb == nullptr)
    return false;

  m_pid = process_id;
  m_proc_data = info;
  return true;
}

bool c_memory::attach(std::wstring process_name) {
  return attach(get_process_id(process_name));
}

void c_memory::detach() {
  m_pid = 0;
  m_proc_data = {};
  m_transport_fn  = nullptr;
  m_transport_idx = 0;
  m_connected     = false;
}

bool c_memory::read(uint64_t src, void *dest, uint64_t size) {
  if (!src || !dest || !size || !m_pid)
    return false;

  if (src > 0x00007FFFFFFFFFFFULL && src < 0xFFFF800000000000ULL)
    return false;

  if (!check_transport_ready())
    return false;

  if (m_ring_ready) {
    constexpr uint32_t kBounceMax =
        (uint32_t)sizeof(m_ring_view->payload) - kAstrumBounceOffset;
    uint8_t* out       = reinterpret_cast<uint8_t*>(dest);
    uint64_t remaining = size;
    uint64_t cursor    = src;

    while (remaining) {
      uint32_t chunk = remaining > kBounceMax ? kBounceMax : (uint32_t)remaining;
      mem_copy_request input = {};
      input.process_id = m_pid;
      input.source     = cursor;
      input.dest       = reinterpret_cast<void*>((uintptr_t)kAstrumBounceOffset);
      input.size       = chunk;
      NTSTATUS s = ring_submit((uint32_t)e_command::mem_read,
                               &input, sizeof(input),
                               nullptr, 0,
                               nullptr, 0,
                               out, chunk);
      if (s < 0) return false;
      out       += chunk;
      cursor    += chunk;
      remaining -= chunk;
    }
    return true;
  }

  mem_copy_request input = {};
  input.process_id = m_pid;
  input.source = src;
  input.dest = dest;
  input.size = size;

  auto request = c_request((e_command)astrum_cmd::read, &input, sizeof(input));

  for (int attempt = 0; attempt < 3; ++attempt) {
    if (dispatch(&request) >= 0)
      return true;
  }
  return false;
}

bool c_memory::write(void *src, uint64_t dest, uint64_t size) {
  if (!src || !dest || !size || !m_pid)
    return false;

  if (dest > 0x00007FFFFFFFFFFFULL && dest < 0xFFFF800000000000ULL)
    return false;

  if (!check_transport_ready())
    return false;

  if (m_ring_ready) {
    constexpr uint32_t kBounceMax =
        (uint32_t)sizeof(m_ring_view->payload) - kAstrumBounceOffset;
    const uint8_t* in  = reinterpret_cast<const uint8_t*>(src);
    uint64_t remaining = size;
    uint64_t cursor    = dest;

    while (remaining) {
      uint32_t chunk = remaining > kBounceMax ? kBounceMax : (uint32_t)remaining;
      mem_copy_request input = {};
      input.process_id = m_pid;
      input.source     = kAstrumBounceOffset;
      input.dest       = reinterpret_cast<void*>(cursor);
      input.size       = chunk;
      NTSTATUS s = ring_submit((uint32_t)e_command::mem_write,
                               &input, sizeof(input),
                               nullptr, 0,
                               in, chunk,
                               nullptr, 0);
      if (s < 0) return false;
      in        += chunk;
      cursor    += chunk;
      remaining -= chunk;
    }
    return true;
  }

  mem_copy_request input = {};
  input.process_id = m_pid;
  input.source = reinterpret_cast<uint64_t>(src);
  input.dest = reinterpret_cast<void *>(dest);
  input.size = size;

  auto request = c_request((e_command)astrum_cmd::write, &input, sizeof(input));

  for (int attempt = 0; attempt < 3; ++attempt) {
    if (dispatch(&request) >= 0)
      return true;
  }
  return false;
}

uint64_t c_memory::get_module_base(std::wstring module_name) {
  if (!m_pid)
    return 0;

  if (m_proc_data.base_address) {
    static const wchar_t* self_names[] = {
        L"RustClient.exe", L"rustclient.exe",
        L"PioneerGame.exe", L"pioneerGame.exe", L"PIONEERGAME.EXE",
        L"PioneerGame-d.exe", L"PioneerGame-e.exe",
        L"PioneerGame-Win64-Shipping.exe",
    };
    for (auto* n : self_names) {
      const std::wstring_view needle(n);
      auto it = std::search(module_name.begin(), module_name.end(),
                            needle.begin(), needle.end(),
                            [](wchar_t a, wchar_t b){ return towlower(a) == towlower(b); });
      if (it != module_name.end())
        return reinterpret_cast<uint64_t>(m_proc_data.base_address);
    }
  }

  if (!m_proc_data.peb)
    return 0;

  PEB peb = {};
  if (!read(reinterpret_cast<uint64_t>(m_proc_data.peb), &peb, sizeof(peb)))
    return 0;

  if (!peb.Ldr)
    return 0;

  PEB_LDR_DATA ldr = {};
  if (!read(reinterpret_cast<uint64_t>(peb.Ldr), &ldr, sizeof(ldr)))
    return 0;

  const uint64_t list_head = reinterpret_cast<uint64_t>(peb.Ldr) +
                             offsetof(PEB_LDR_DATA, InMemoryOrderModuleList);
  uint64_t list_entry =
      reinterpret_cast<uint64_t>(ldr.InMemoryOrderModuleList.Flink);

  int max_iter = 512;
  while (list_entry && list_entry != list_head && --max_iter > 0) {
    const uint64_t entry_va =
        list_entry - offsetof(LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

    LDR_DATA_TABLE_ENTRY entry = {};
    if (!read(entry_va, &entry, sizeof(entry))) {
      list_entry = 0;
      break;
    }

    UNICODE_STRING name_str = {};
    if (!read(entry_va + offsetof(LDR_DATA_TABLE_ENTRY, FullDllName), &name_str,
              sizeof(name_str))) {
      list_entry = read<uint64_t>(list_entry);
      continue;
    }

    if (name_str.Length > 0 && name_str.Length < 520 && name_str.Buffer) {
      std::wstring dll_name;
      dll_name.resize(name_str.Length / sizeof(wchar_t));

      if (!dll_name.empty() &&
          read(reinterpret_cast<uint64_t>(name_str.Buffer), dll_name.data(),
               name_str.Length)) {
        auto it =
            std::search(dll_name.begin(), dll_name.end(), module_name.begin(),
                        module_name.end(), [](wchar_t a, wchar_t b) {
                          return towlower(a) == towlower(b);
                        });

        if (it != dll_name.end())
          return reinterpret_cast<uint64_t>(entry.DllBase);
      }
    }

    list_entry = read<uint64_t>(list_entry);
  }

  if (m_proc_data.base_address && module_name.size() >= 4) {
    const wchar_t* tail = module_name.c_str() + module_name.size() - 4;
    const bool is_exe =
        tail[0] == L'.' &&
        (tail[1] == L'e' || tail[1] == L'E') &&
        (tail[2] == L'x' || tail[2] == L'X') &&
        (tail[3] == L'e' || tail[3] == L'E');
    if (is_exe)
      return reinterpret_cast<uint64_t>(m_proc_data.base_address);
  }

  return 0;
}

uint64_t c_memory::alloc(uint64_t size, uint32_t /*protect*/) {
  if (!size || !m_pid)
    return 0;

  mem_alloc_request input = {};
  input.process_id = m_pid;
  input.size = size;
  input.address = nullptr;  // OUT: driver writes the chosen base here

  auto request = c_request((e_command)astrum_cmd::alloc, &input, sizeof(input));

  for (int attempt = 0; attempt < 3; ++attempt) {
    if (dispatch(&request) >= 0)
      return reinterpret_cast<uint64_t>(input.address);
  }
  return 0;
}

bool c_memory::free(uint64_t) { return false; }
bool c_memory::protect(uint64_t, uint64_t, uint32_t) { return false; }
bool c_memory::hide_process(uint32_t) { return false; }
bool c_memory::unhide_process(uint32_t) { return false; }
bool c_memory::is_hidden(uint32_t) { return false; }

bool c_memory::scatter_write(SCATTER_WRITE_ENTRY *entries, size_t count) {
  if (!entries || !count || !m_pid)
    return false;

  bool any_ok = false;
  for (size_t i = 0; i < count; ++i) {
    if (entries[i].address && entries[i].value) {
      if (write(&entries[i].value, entries[i].address, sizeof(uint32_t)))
        any_ok = true;
    }
  }
  return any_ok;
}

bool c_memory::scatter_read(SCATTER_READ_ENTRY *entries, size_t count) {
  if (!entries || !count || !m_pid)
    return false;

  bool any_ok = false;
  for (size_t i = 0; i < count; ++i) {
    entries[i].status = 0;
    if (!entries[i].address || !entries[i].size || entries[i].size > 16)
      continue;

    memset(entries[i].data, 0, 16);
    if (read(entries[i].address, entries[i].data, entries[i].size)) {
      entries[i].status = 1;
      any_ok = true;
    }
  }
  return any_ok;
}

uint32_t c_memory::get_process_id(std::wstring process_name) const {
  auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    return 0;

  PROCESSENTRY32W entry = {sizeof(entry)};
  if (!Process32FirstW(snapshot, &entry)) {
    CloseHandle(snapshot);
    return 0;
  }

  do {
    if (std::wstring(entry.szExeFile) == process_name) {
      CloseHandle(snapshot);
      return entry.th32ProcessID;
    }
  } while (Process32NextW(snapshot, &entry));

  CloseHandle(snapshot);
  return 0;
}

bool c_memory::read_batch(mem_read_batch_entry *entries, uint32_t count) {
  if (!entries || count == 0 || count > kMaxBatchEntries || !m_pid)
    return false;

  if (!check_transport_ready())
    return false;

  uint64_t total = 0;
  for (uint32_t i = 0; i < count; ++i) {
    if (entries[i].size > (16ull * 1024 * 1024)) {
      return false;
    }
    total += entries[i].size;
    if (total > kMaxBatchTotalBytes) {
      return false;
    }
    entries[i].process_id = m_pid;
    entries[i].status     = 0;       // start clean
    entries[i]._pad       = 0;
  }

  mem_read_batch_request req{};
  req.count   = count;
  req._pad    = 0;
  req.entries = entries;

  auto request = c_request((e_command)astrum_cmd::batch(), &req, sizeof(req));

  for (int attempt = 0; attempt < 3; ++attempt) {
    if (dispatch(&request) >= 0)
      return true;
  }
  return false;
}

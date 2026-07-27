#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <vector>
#include <fstream> // for settings persistence
#include <iterator>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <limits>
#include <thread>
#include <cstdint>
#include <cwctype>

// Unreal Engine commonly defines typedefs like int32, uint32, etc. through
// CoreMinimal.h.  Because this trainer is compiled independently from the
// game code and may not include those typedefs, we provide a local alias
// here.  Without this definition, signatures using `int32` will fail to
// compile with "identifier int32 is undefined".  We map int32 to the
// standard 32-bit signed integer type defined in <cstdint>.
using int32 = int32_t;

#include "MinHook.h"
#include "BackoffScheduler.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

#include <filesystem>
#include <sstream>
#include <ctime>
#include <winhttp.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

// Theme and compact UI widgets.
#include "OdysseyTheme.h"
#include "OdysseyWidgets.h"

#include "SDK.hpp"
#include "../CppSDK/SDK/UMG_parameters.hpp"
#include "../CppSDK/SDK/RallyHereIntegration_parameters.hpp"

static constexpr wchar_t kEmbeddedTranslationApiKey[] = L"";

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace render_ui {
    bool WantsInput();
    bool IsVisible();
    void ForceOpen();
    void ForceClose();
    void Render();
    void OnDeviceDestroyed();
}

static const char* TranslationStatusText();
static bool TranslationHasApiKey();
static void TranslationTryInstallHook();
static void TranslationTickGameThread();
static void TranslationRenderOverlay();
static void TranslationStopRuntime();
static bool SafeObjectIsA(SDK::UObject* object, SDK::UClass* objectClass);
static bool icontains(const std::string& hay, const char* needle);
static std::string LocalizeGodDisplayName(const std::string& name);

ImFont* poppins = nullptr;
ImFont* tab_title = nullptr;
ImFont* font_icon = nullptr;
float accent_colour[4] = { 0.45f, 0.72f, 1.00f, 1.0f };
float content_animation = 1.0f;

typedef HRESULT(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
typedef void(__stdcall* ExecuteCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
typedef HRESULT(__stdcall* CreateSwapChainFn)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
typedef HRESULT(__stdcall* CreateSwapChainForHwndFn)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT(__stdcall* CreateSwapChainForCoreWindowFn)(IDXGIFactory2*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT(__stdcall* CreateSwapChainForCompositionFn)(IDXGIFactory2*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);

struct Dx12FrameContext {
    ID3D12CommandAllocator* commandAllocator = nullptr;
    ID3D12Resource* renderTarget = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 fenceValue = 0;
};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static HRESULT __stdcall hkPresent(IDXGISwapChain*, UINT, UINT);
static void __stdcall hkExecuteCommandLists(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
static HRESULT __stdcall hkCreateSwapChain(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
static HRESULT __stdcall hkCreateSwapChainForHwnd(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
static HRESULT __stdcall hkCreateSwapChainForCoreWindow(IDXGIFactory2*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
static HRESULT __stdcall hkCreateSwapChainForComposition(IDXGIFactory2*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
static void InstallGameRuntimeTimer(HWND hWnd);
static void RemoveGameRuntimeTimer();

static Present g_originalPresent = nullptr;
static ExecuteCommandLists g_originalExecuteCommandLists = nullptr;
static CreateSwapChainFn g_originalCreateSwapChain = nullptr;
static CreateSwapChainForHwndFn g_originalCreateSwapChainForHwnd = nullptr;
static CreateSwapChainForCoreWindowFn g_originalCreateSwapChainForCoreWindow = nullptr;
static CreateSwapChainForCompositionFn g_originalCreateSwapChainForComposition = nullptr;
static WNDPROC g_originalWndProc = nullptr;
static ID3D12Device* g_device = nullptr;
static ID3D12CommandQueue* g_commandQueue = nullptr;
static ID3D12DescriptorHeap* g_rtvHeap = nullptr;
static ID3D12DescriptorHeap* g_srvHeap = nullptr;
static ID3D12GraphicsCommandList* g_commandList = nullptr;
static ID3D12Fence* g_frameFence = nullptr;
static HANDLE g_frameFenceEvent = nullptr;
static UINT64 g_nextFenceValue = 1;
static std::vector<Dx12FrameContext> g_frameContexts;
static UINT g_rtvDescriptorSize = 0;
static UINT g_srvDescriptorSize = 0;
static UINT g_bufferCount = 0;
static DXGI_FORMAT g_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
static constexpr UINT kDx12SrvDescriptorCount = 256;
static bool g_srvDescriptorUsed[kDx12SrvDescriptorCount] = {};
static constexpr UINT kDx12PresentWarmupFrames = 90;
static HWND g_hWnd = nullptr;
static constexpr UINT_PTR kGameRuntimeTimerId = 0x534D4954;
static HWND g_gameRuntimeTimerWindow = nullptr;
static DWORD g_gameWindowThreadId = 0;
static bool g_imguiInitialized = false;
static volatile LONG g_presentRenderLock = 0;
static bool g_needRTVRecreate = false;

static bool g_showLoadingPlayerRanks = true;
static bool g_draftStatsPositionEditEnabled = false;
static bool g_draftStatsOffsetCustom = false;
static ImVec2 g_draftStatsOffset(0.0f, 0.0f);

static bool g_translationEnabled = false;
static bool g_translationIncomingEnabled = true;
static bool g_translationOutgoingEnabled = true;
static bool g_translationShowOriginal = true;
static bool g_translationPositionEditEnabled = false;
static bool g_translationOverlayPositionCustom = false;
static ImVec2 g_translationOverlayPosition(0.0f, 0.0f);

static const int g_menuKeyCodes[] = {
    VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
    VK_INSERT, VK_DELETE, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
    VK_TAB, VK_CAPITAL, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LSHIFT, VK_RSHIFT,
    VK_ESCAPE, VK_RETURN, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
    VK_CONTROL, VK_MENU, VK_SHIFT, VK_SPACE, VK_BACK
};
static int g_selectedMenuKey = 12;
static int g_menuKeyVirtual = VK_INSERT;

static std::filesystem::path GetUserSmite2ConfigDirectory()
{
    std::error_code error;
    std::filesystem::path directory = std::filesystem::temp_directory_path(error) / L".oopz";
    std::filesystem::create_directories(directory, error);
    return directory;
}


// Colour palette for the custom UI.  Adjust these values to taste.
static ImVec4 g_colorBackground = ImVec4(0.12f, 0.14f, 0.18f, 0.95f);
static ImVec4 g_colorPanel = ImVec4(0.16f, 0.18f, 0.22f, 0.95f);
static ImVec4 g_colorAccent = ImVec4(0.80f, 0.20f, 0.30f, 1.00f);

static float GetLocalPingMillisecondsSafe()
{
    __try
    {
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->OwningGameInstance) return -1.0f;
        auto& locals = world->OwningGameInstance->LocalPlayers;
        if (!locals.IsValid() || locals.Num() <= 0) return -1.0f;
        SDK::ULocalPlayer* lp = locals[0];
        if (!lp || !lp->PlayerController || !lp->PlayerController->PlayerState) return -1.0f;

        auto* playerState = lp->PlayerController->PlayerState;
        float pingMs = playerState->GetPingInMilliseconds();
        if (pingMs <= 0.0f)
        {
            const unsigned char compressedPing = playerState->GetCompressedPing();
            if (compressedPing > 0)
                pingMs = static_cast<float>(compressedPing) * 4.0f;
        }
        return pingMs;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1.0f;
    }
}

struct MatchServerEndpointSnapshot
{
    std::string address;
    uint16_t port = 0;
    bool valid = false;
    bool ipv6 = false;
};

static MatchServerEndpointSnapshot GetMatchServerEndpointSnapshot();
static void Dx12DebugLog(const char* format, ...);

#include "ModernMenu.inl"

// -----------------------------------------------------------------------------
// Current match endpoint display
//
// Unreal keeps the native remote address inside non-reflected IpConnection
// fields. Read its reflected FURL layout structurally when available, then use
// a heavily sampled UDP destination capture as a fallback for SendTo-based net
// drivers. The fast path only increments a thread-local counter.
// -----------------------------------------------------------------------------
namespace
{
    struct CapturedUdpEndpoint
    {
        SOCKET socket = INVALID_SOCKET;
        sockaddr_storage remote{};
        int remoteLength = 0;
        ULONGLONG capturedAtMs = 0;
        ULONGLONG lastSeenMs = 0;
        uint32_t recentSamples = 0;
    };

    struct RawFStringHeader
    {
        const wchar_t* data = nullptr;
        int32_t count = 0;
        int32_t capacity = 0;
    };

    constexpr size_t kCapturedUdpEndpointCount = 16;
    SRWLOCK g_matchEndpointLock = SRWLOCK_INIT;
    CapturedUdpEndpoint g_capturedUdpEndpoints[kCapturedUdpEndpointCount]{};
    size_t g_nextCapturedUdpEndpoint = 0;

    using ConnectFn = int (WSAAPI*)(SOCKET, const sockaddr*, int);
    using WSAConnectFn = int (WSAAPI*)(SOCKET, const sockaddr*, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS);
    using SendToFn = int (WSAAPI*)(SOCKET, const char*, int, int, const sockaddr*, int);
    using WSASendToFn = int (WSAAPI*)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, const sockaddr*, int, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
    ConnectFn g_originalConnect = nullptr;
    WSAConnectFn g_originalWSAConnect = nullptr;
    SendToFn g_originalSendTo = nullptr;
    WSASendToFn g_originalWSASendTo = nullptr;

    bool IsUsableRemoteEndpoint(const sockaddr* address, int addressLength)
    {
        if (!address || addressLength <= 0)
            return false;

        if (address->sa_family == AF_INET && addressLength >= static_cast<int>(sizeof(sockaddr_in)))
        {
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address);
            const uint32_t hostAddress = ntohl(ipv4->sin_addr.s_addr);
            const uint8_t firstOctet = static_cast<uint8_t>(hostAddress >> 24);
            return ipv4->sin_port != 0 && hostAddress != INADDR_ANY &&
                hostAddress != INADDR_BROADCAST && firstOctet != 127 &&
                firstOctet < 224;
        }

        if (address->sa_family == AF_INET6 && addressLength >= static_cast<int>(sizeof(sockaddr_in6)))
        {
            const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address);
            return ipv6->sin6_port != 0 && !IN6_IS_ADDR_UNSPECIFIED(&ipv6->sin6_addr) &&
                !IN6_IS_ADDR_LOOPBACK(&ipv6->sin6_addr) &&
                !IN6_IS_ADDR_MULTICAST(&ipv6->sin6_addr);
        }

        return false;
    }

    bool SameRemoteEndpoint(const CapturedUdpEndpoint& candidate, SOCKET socket,
        const sockaddr* address, int addressLength)
    {
        return candidate.socket == socket && candidate.remoteLength == addressLength &&
            addressLength > 0 &&
            memcmp(&candidate.remote, address, static_cast<size_t>(addressLength)) == 0;
    }

    void CaptureUdpEndpoint(SOCKET socket, const sockaddr* address, int addressLength)
    {
        if (!IsUsableRemoteEndpoint(address, addressLength) ||
            addressLength > static_cast<int>(sizeof(sockaddr_storage)))
            return;

        int socketType = 0;
        int socketTypeLength = sizeof(socketType);
        if (getsockopt(socket, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&socketType), &socketTypeLength) == SOCKET_ERROR ||
            socketType != SOCK_DGRAM)
            return;

        CapturedUdpEndpoint captured{};
        captured.socket = socket;
        captured.remoteLength = addressLength;
        captured.capturedAtMs = GetTickCount64();
        captured.lastSeenMs = captured.capturedAtMs;
        captured.recentSamples = 1;
        memcpy(&captured.remote, address, static_cast<size_t>(addressLength));

        AcquireSRWLockExclusive(&g_matchEndpointLock);
        size_t slot = kCapturedUdpEndpointCount;
        for (size_t i = 0; i < kCapturedUdpEndpointCount; ++i)
        {
            if (SameRemoteEndpoint(g_capturedUdpEndpoints[i], socket, address, addressLength))
            {
                slot = i;
                break;
            }
        }
        if (slot != kCapturedUdpEndpointCount)
        {
            CapturedUdpEndpoint& existing = g_capturedUdpEndpoints[slot];
            if (captured.lastSeenMs - existing.lastSeenMs > 3000)
                existing.recentSamples = 1;
            else if (existing.recentSamples < 1000000)
                ++existing.recentSamples;
            existing.lastSeenMs = captured.lastSeenMs;
            ReleaseSRWLockExclusive(&g_matchEndpointLock);
            return;
        }
        if (slot == kCapturedUdpEndpointCount)
        {
            slot = g_nextCapturedUdpEndpoint;
            g_nextCapturedUdpEndpoint = (g_nextCapturedUdpEndpoint + 1) % kCapturedUdpEndpointCount;
        }
        g_capturedUdpEndpoints[slot] = captured;
        ReleaseSRWLockExclusive(&g_matchEndpointLock);
    }

    int WSAAPI HookedConnect(SOCKET socket, const sockaddr* address, int addressLength)
    {
        if (!g_originalConnect)
            return SOCKET_ERROR;

        const int result = g_originalConnect(socket, address, addressLength);
        const int error = (result == SOCKET_ERROR) ? WSAGetLastError() : 0;
        if (result == 0 || error == WSAEWOULDBLOCK || error == WSAEINPROGRESS)
            CaptureUdpEndpoint(socket, address, addressLength);
        if (result == SOCKET_ERROR)
            WSASetLastError(error);
        return result;
    }

    int WSAAPI HookedWSAConnect(SOCKET socket, const sockaddr* address, int addressLength,
        LPWSABUF callerData, LPWSABUF calleeData, LPQOS sqos, LPQOS gqos)
    {
        if (!g_originalWSAConnect)
            return SOCKET_ERROR;

        const int result = g_originalWSAConnect(socket, address, addressLength, callerData, calleeData, sqos, gqos);
        const int error = (result == SOCKET_ERROR) ? WSAGetLastError() : 0;
        if (result == 0 || error == WSAEWOULDBLOCK || error == WSAEINPROGRESS)
            CaptureUdpEndpoint(socket, address, addressLength);
        if (result == SOCKET_ERROR)
            WSASetLastError(error);
        return result;
    }

    int WSAAPI HookedSendTo(SOCKET socket, const char* buffer, int length, int flags,
        const sockaddr* destination, int destinationLength)
    {
        if (!g_originalSendTo)
            return SOCKET_ERROR;

        const int result = g_originalSendTo(socket, buffer, length, flags, destination, destinationLength);
        const int error = (result == SOCKET_ERROR) ? WSAGetLastError() : 0;
        static thread_local uint32_t sampleCounter = 0;
        if (result != SOCKET_ERROR && ((sampleCounter++ & 31u) == 0))
            CaptureUdpEndpoint(socket, destination, destinationLength);
        if (result == SOCKET_ERROR)
            WSASetLastError(error);
        return result;
    }

    int WSAAPI HookedWSASendTo(SOCKET socket, LPWSABUF buffers, DWORD bufferCount,
        LPDWORD bytesSent, DWORD flags, const sockaddr* destination, int destinationLength,
        LPWSAOVERLAPPED overlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE completionRoutine)
    {
        if (!g_originalWSASendTo)
            return SOCKET_ERROR;

        const int result = g_originalWSASendTo(socket, buffers, bufferCount, bytesSent, flags,
            destination, destinationLength, overlapped, completionRoutine);
        const int error = (result == SOCKET_ERROR) ? WSAGetLastError() : 0;
        static thread_local uint32_t sampleCounter = 0;
        if ((result == 0 || error == WSA_IO_PENDING) && ((sampleCounter++ & 31u) == 0))
            CaptureUdpEndpoint(socket, destination, destinationLength);
        if (result == SOCKET_ERROR)
            WSASetLastError(error);
        return result;
    }

    bool InstallMatchEndpointHooks()
    {
        HMODULE winsock = GetModuleHandleW(L"ws2_32.dll");
        if (!winsock)
            winsock = LoadLibraryW(L"ws2_32.dll");
        if (!winsock)
            return false;

        bool installed = false;
        void* connectAddress = reinterpret_cast<void*>(GetProcAddress(winsock, "connect"));
        if (connectAddress &&
            MH_CreateHook(connectAddress, &HookedConnect, reinterpret_cast<void**>(&g_originalConnect)) == MH_OK &&
            MH_EnableHook(connectAddress) == MH_OK)
        {
            installed = true;
        }

        void* wsaConnectAddress = reinterpret_cast<void*>(GetProcAddress(winsock, "WSAConnect"));
        if (wsaConnectAddress &&
            MH_CreateHook(wsaConnectAddress, &HookedWSAConnect, reinterpret_cast<void**>(&g_originalWSAConnect)) == MH_OK &&
            MH_EnableHook(wsaConnectAddress) == MH_OK)
        {
            installed = true;
        }

        void* sendToAddress = reinterpret_cast<void*>(GetProcAddress(winsock, "sendto"));
        if (sendToAddress &&
            MH_CreateHook(sendToAddress, &HookedSendTo, reinterpret_cast<void**>(&g_originalSendTo)) == MH_OK &&
            MH_EnableHook(sendToAddress) == MH_OK)
        {
            installed = true;
        }

        void* wsaSendToAddress = reinterpret_cast<void*>(GetProcAddress(winsock, "WSASendTo"));
        if (wsaSendToAddress &&
            MH_CreateHook(wsaSendToAddress, &HookedWSASendTo, reinterpret_cast<void**>(&g_originalWSASendTo)) == MH_OK &&
            MH_EnableHook(wsaSendToAddress) == MH_OK)
        {
            installed = true;
        }

        return installed;
    }

    SDK::UNetConnection* GetLiveMatchServerConnection()
    {
        __try
        {
            SDK::UWorld* world = SDK::UWorld::GetWorld();
            SDK::UNetDriver* driver = world ? world->NetDriver : nullptr;
            SDK::UNetConnection* connection = driver ? driver->ServerConnection : nullptr;
            return (connection && connection->Driver == driver) ? connection : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    bool IsReadableMemoryRange(const void* address, size_t byteCount)
    {
        if (!address || byteCount == 0)
            return false;

        const uintptr_t begin = reinterpret_cast<uintptr_t>(address);
        const uintptr_t end = begin + byteCount;
        if (end < begin)
            return false;

        uintptr_t cursor = begin;
        while (cursor < end)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &mbi, sizeof(mbi)) != sizeof(mbi) ||
                mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) != 0 ||
                (mbi.Protect & PAGE_NOACCESS) != 0)
                return false;

            const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (regionEnd <= cursor)
                return false;
            cursor = (regionEnd < end) ? regionEnd : end;
        }
        return true;
    }

    bool ReadRawFString(const RawFStringHeader& raw, std::wstring& value)
    {
        value.clear();
        if (!raw.data || raw.count <= 1 || raw.count > 256 ||
            raw.capacity < raw.count || raw.capacity > 1024)
            return false;

        const size_t byteCount = static_cast<size_t>(raw.count) * sizeof(wchar_t);
        if (!IsReadableMemoryRange(raw.data, byteCount))
            return false;

        __try
        {
            value.assign(raw.data, raw.data + raw.count);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value.clear();
            return false;
        }

        while (!value.empty() && value.back() == L'\0')
            value.pop_back();
        if (value.empty())
            return false;

        for (wchar_t ch : value)
        {
            if (ch < 0x20 || ch > 0x7E)
                return false;
        }
        return true;
    }

    MatchServerEndpointSnapshot MakeEndpointSnapshotFromHost(std::wstring host, int32_t port)
    {
        MatchServerEndpointSnapshot result{};
        if (port <= 0 || port > 65535)
            return result;

        if (host.size() >= 2 && host.front() == L'[' && host.back() == L']')
            host = host.substr(1, host.size() - 2);

        char addressBuffer[INET6_ADDRSTRLEN]{};
        IN_ADDR ipv4{};
        if (InetPtonW(AF_INET, host.c_str(), &ipv4) == 1)
        {
            if (!InetNtopA(AF_INET, &ipv4, addressBuffer, sizeof(addressBuffer)))
                return result;
        }
        else
        {
            IN6_ADDR ipv6{};
            if (InetPtonW(AF_INET6, host.c_str(), &ipv6) != 1 ||
                !InetNtopA(AF_INET6, &ipv6, addressBuffer, sizeof(addressBuffer)))
                return result;
            result.ipv6 = true;
        }

        result.address = addressBuffer;
        result.port = static_cast<uint16_t>(port);
        result.valid = !result.address.empty();
        return result;
    }

    bool TryReadUrlHeader(const uint8_t* bytes, size_t offset,
        RawFStringHeader* hostHeader, int32_t* port, int32_t* valid)
    {
        if (!bytes || !hostHeader || !port || !valid)
            return false;

        __try
        {
            memcpy(hostHeader, bytes + offset + 0x10, sizeof(*hostHeader));
            memcpy(port, bytes + offset + 0x20, sizeof(*port));
            memcpy(valid, bytes + offset + 0x24, sizeof(*valid));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    MatchServerEndpointSnapshot TryReadConnectionUrl(SDK::UNetConnection* connection)
    {
        MatchServerEndpointSnapshot result{};
        if (!connection || !IsReadableMemoryRange(connection, sizeof(SDK::UNetConnection)))
            return result;

        const auto* bytes = reinterpret_cast<const uint8_t*>(connection);
        constexpr size_t kUrlSize = sizeof(SDK::FURL);
        for (size_t offset = sizeof(SDK::UPlayer); offset + kUrlSize <= sizeof(SDK::UNetConnection); offset += 8)
        {
            RawFStringHeader hostHeader{};
            int32_t port = 0;
            int32_t valid = 0;
            if (!TryReadUrlHeader(bytes, offset, &hostHeader, &port, &valid))
                return {};

            if (valid != 1 || port <= 0 || port > 65535)
                continue;

            std::wstring host;
            if (!ReadRawFString(hostHeader, host))
                continue;

            result = MakeEndpointSnapshotFromHost(host, port);
            if (result.valid)
                return result;
        }
        return result;
    }

    MatchServerEndpointSnapshot MakeEndpointSnapshot(const sockaddr_storage& storage)
    {
        MatchServerEndpointSnapshot result{};
        char addressBuffer[INET6_ADDRSTRLEN]{};

        if (storage.ss_family == AF_INET)
        {
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&storage);
            if (!InetNtopA(AF_INET, &ipv4->sin_addr, addressBuffer, sizeof(addressBuffer)))
                return result;
            result.port = ntohs(ipv4->sin_port);
        }
        else if (storage.ss_family == AF_INET6)
        {
            const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(&storage);
            if (IN6_IS_ADDR_V4MAPPED(&ipv6->sin6_addr))
            {
                IN_ADDR mappedIpv4{};
                memcpy(&mappedIpv4, &ipv6->sin6_addr.u.Byte[12], sizeof(mappedIpv4));
                if (!InetNtopA(AF_INET, &mappedIpv4, addressBuffer, sizeof(addressBuffer)))
                    return result;
            }
            else
            {
                if (!InetNtopA(AF_INET6, &ipv6->sin6_addr, addressBuffer, sizeof(addressBuffer)))
                    return result;
                result.ipv6 = true;
            }
            result.port = ntohs(ipv6->sin6_port);
        }
        else
        {
            return result;
        }

        result.address = addressBuffer;
        result.valid = !result.address.empty() && result.port != 0;
        return result;
    }
}

static MatchServerEndpointSnapshot GetMatchServerEndpointSnapshot()
{
    static MatchServerEndpointSnapshot cached{};
    static ULONGLONG lastRefreshMs = 0;
    static int lastDiagnosticState = -1;
    const ULONGLONG nowMs = GetTickCount64();

    if (nowMs - lastRefreshMs < 750)
        return cached;
    lastRefreshMs = nowMs;

    SDK::UNetConnection* connection = GetLiveMatchServerConnection();
    if (!connection)
    {
        cached = {};
        if (lastDiagnosticState != 0)
        {
            Dx12DebugLog("Match endpoint: waiting for ServerConnection");
            lastDiagnosticState = 0;
        }
        return cached;
    }

    cached = TryReadConnectionUrl(connection);
    if (cached.valid)
    {
        if (lastDiagnosticState != 1)
        {
            Dx12DebugLog("Match endpoint: FURL source active %s:%u", cached.address.c_str(), static_cast<unsigned>(cached.port));
            lastDiagnosticState = 1;
        }
        return cached;
    }

    CapturedUdpEndpoint candidates[kCapturedUdpEndpointCount]{};
    AcquireSRWLockShared(&g_matchEndpointLock);
    memcpy(candidates, g_capturedUdpEndpoints, sizeof(candidates));
    ReleaseSRWLockShared(&g_matchEndpointLock);

    CapturedUdpEndpoint* best = nullptr;
    sockaddr_storage liveRemote{};
    for (auto& candidate : candidates)
    {
        if (candidate.socket == INVALID_SOCKET || candidate.lastSeenMs == 0 ||
            nowMs - candidate.lastSeenMs > 15000)
            continue;

        int socketType = 0;
        int socketTypeLength = sizeof(socketType);
        if (getsockopt(candidate.socket, SOL_SOCKET, SO_TYPE,
            reinterpret_cast<char*>(&socketType), &socketTypeLength) == SOCKET_ERROR ||
            socketType != SOCK_DGRAM)
            continue;

        if (best && (candidate.recentSamples < best->recentSamples ||
            (candidate.recentSamples == best->recentSamples && candidate.lastSeenMs <= best->lastSeenMs)))
            continue;

        best = &candidate;
        liveRemote = candidate.remote;
    }

    cached = best ? MakeEndpointSnapshot(liveRemote) : MatchServerEndpointSnapshot{};
    const int diagnosticState = cached.valid ? 2 : 3;
    if (lastDiagnosticState != diagnosticState)
    {
        if (cached.valid)
            Dx12DebugLog("Match endpoint: sampled UDP source active %s:%u", cached.address.c_str(), static_cast<unsigned>(cached.port));
        else
            Dx12DebugLog("Match endpoint: ServerConnection active, waiting for UDP destination");
        lastDiagnosticState = diagnosticState;
    }
    return cached;
}

// -----------------------------------------------------------------------------
// Optional DeepSeek chat translation
//
// Network work is kept on a private worker thread.  Unreal objects are only
// read from the chat ProcessEvent hook and written back from the existing
// window-thread runtime tick, so the translator never blocks Present or calls
// a game RPC from the HTTP worker.
// -----------------------------------------------------------------------------
static SDK::AHWPlayerState* SafeGetLocalHWPlayerState(SDK::APlayerController* controller)
{
    if (!controller)
        return nullptr;

    __try
    {
        SDK::APlayerState* playerState = controller->PlayerState;
        if (!SafeObjectIsA(playerState, SDK::AHWPlayerState::StaticClass()))
            return nullptr;
        return static_cast<SDK::AHWPlayerState*>(playerState);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

enum class TranslationDirection : uint8_t
{
    Incoming,
    Outgoing
};

struct TranslationRequest
{
    uint64_t id = 0;
    TranslationDirection direction = TranslationDirection::Incoming;
    std::wstring text;
    std::wstring senderLabel;
};

struct TranslationResult
{
    uint64_t id = 0;
    TranslationDirection direction = TranslationDirection::Incoming;
    std::wstring source;
    std::wstring translated;
    std::wstring senderLabel;
    bool success = false;
};

struct TranslationLine
{
    std::wstring source;
    std::wstring translated;
    std::wstring senderLabel;
    ULONGLONG createdAtMs = 0;
};

struct TranslationChatAnchor
{
    ImVec2 topLeft{};
    ImVec2 size{};
    bool valid = false;
};

static std::mutex g_translationChatAnchorMutex;
static TranslationChatAnchor g_translationChatAnchor;

static std::filesystem::path TranslationApiKeyPath()
{
    wchar_t userProfile[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", userProfile, static_cast<DWORD>(std::size(userProfile)));
    if (length > 0 && length < std::size(userProfile))
        return std::filesystem::path(userProfile) / L"Desktop" / L"DeepSeek_API_Key.txt";
    return std::filesystem::path(L"DeepSeek_API_Key.txt");
}

static std::wstring TranslationUtf8ToWide(const std::string& value)
{
    if (value.empty())
        return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
        return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

static std::string TranslationWideToUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0)
        return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

static std::wstring TranslationReadApiKey()
{
    if (kEmbeddedTranslationApiKey[0] != L'\0')
        return std::wstring(kEmbeddedTranslationApiKey);

    std::ifstream input(TranslationApiKeyPath(), std::ios::in | std::ios::binary);
    if (!input.is_open())
        return {};

    std::string line;
    while (std::getline(input, line))
    {
        if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
            line.erase(0, 3);

        const size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#')
            continue;
        const size_t last = line.find_last_not_of(" \t\r\n");
        std::string key = line.substr(first, last - first + 1);
        if (key.rfind("api_key=", 0) == 0)
            key.erase(0, 8);
        if (!key.empty())
            return TranslationUtf8ToWide(key);
    }
    return {};
}

static bool TranslationContainsCjk(const std::wstring& text)
{
    for (wchar_t ch : text)
    {
        if ((ch >= 0x3400 && ch <= 0x4DBF) ||
            (ch >= 0x4E00 && ch <= 0x9FFF) ||
            (ch >= 0xF900 && ch <= 0xFAFF))
            return true;
    }
    return false;
}

static bool TranslationLooksLikeEnglish(const std::wstring& text)
{
    int letters = 0;
    for (wchar_t ch : text)
    {
        if (ch >= L'A' && ch <= L'Z')
            ++letters;
        else if (ch >= L'a' && ch <= L'z')
            ++letters;
    }
    return !TranslationContainsCjk(text) && letters >= 2;
}

static bool TranslationLooksLikeChineseOutgoing(const std::wstring& text)
{
    if (!TranslationContainsCjk(text))
        return false;
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos)
        return false;
    return text[first] != L'/' && text[first] != L'!';
}

static bool TranslationLooksLikeValidIncomingResult(const std::wstring& text)
{
    return !text.empty() && text.size() <= 512 && TranslationContainsCjk(text);
}

static bool TranslationLooksLikeValidOutgoingResult(const std::wstring& text)
{
    if (text.empty() || text.size() > 256 || TranslationContainsCjk(text))
        return false;

    int asciiLetters = 0;
    for (wchar_t character : text)
    {
        if ((character >= L'A' && character <= L'Z') ||
            (character >= L'a' && character <= L'z'))
            ++asciiLetters;
        else if ((character >= L'0' && character <= L'9') || character == L' ')
            continue;
        else
            return false;
    }
    return asciiLetters > 0;
}

static std::string TranslationJsonEscape(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 16);
    for (unsigned char ch : value)
    {
        switch (ch)
        {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (ch < 0x20)
            {
                char buffer[7]{};
                sprintf_s(buffer, "\\u%04x", static_cast<unsigned>(ch));
                result += buffer;
            }
            else
                result.push_back(static_cast<char>(ch));
            break;
        }
    }
    return result;
}

static void TranslationAppendUtf8CodePoint(std::string& output, uint32_t codePoint)
{
    if (codePoint <= 0x7F)
        output.push_back(static_cast<char>(codePoint));
    else if (codePoint <= 0x7FF)
    {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint <= 0xFFFF)
    {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint <= 0x10FFFF)
    {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

static bool TranslationParseJsonString(const std::string& json, size_t start, std::string& output)
{
    if (start >= json.size() || json[start] != '"')
        return false;

    output.clear();
    for (size_t i = start + 1; i < json.size(); ++i)
    {
        const char ch = json[i];
        if (ch == '"')
            return true;
        if (ch != '\\')
        {
            output.push_back(ch);
            continue;
        }
        if (++i >= json.size())
            return false;
        switch (json[i])
        {
        case '"': output.push_back('"'); break;
        case '\\': output.push_back('\\'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        case 'u':
        {
            if (i + 4 >= json.size())
                return false;
            uint32_t codePoint = 0;
            for (size_t digit = 1; digit <= 4; ++digit)
            {
                const char hex = json[i + digit];
                codePoint <<= 4;
                if (hex >= '0' && hex <= '9') codePoint |= static_cast<uint32_t>(hex - '0');
                else if (hex >= 'a' && hex <= 'f') codePoint |= static_cast<uint32_t>(hex - 'a' + 10);
                else if (hex >= 'A' && hex <= 'F') codePoint |= static_cast<uint32_t>(hex - 'A' + 10);
                else return false;
            }
            i += 4;
            TranslationAppendUtf8CodePoint(output, codePoint);
            break;
        }
        default:
            return false;
        }
    }
    return false;
}

static bool TranslationExtractResponseText(const std::string& response, std::string& translated)
{
    size_t search = 0;
    while ((search = response.find("\"content\"", search)) != std::string::npos)
    {
        const size_t colon = response.find(':', search + 9);
        if (colon == std::string::npos)
            break;
        const size_t quote = response.find_first_not_of(" \t\r\n", colon + 1);
        if (quote != std::string::npos && TranslationParseJsonString(response, quote, translated) && !translated.empty())
            return true;
        search = colon + 1;
    }
    return false;
}

static bool TranslationHttpPost(const std::wstring& apiKey, const std::string& body, std::string& response)
{
    HINTERNET session = WinHttpOpen(L"Smite2ChatTranslation/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return false;
    WinHttpSetTimeouts(session, 3000, 3000, 8000, 8000);

    HINTERNET connection = WinHttpConnect(session, L"api.deepseek.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection)
    {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"POST", L"/chat/completions", nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request)
    {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " + apiKey;
    const BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(headers.size()),
        const_cast<char*>(body.data()), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
    bool ok = sent && WinHttpReceiveResponse(request, nullptr);
    response.clear();

    if (ok)
    {
        DWORD available = 0;
        do
        {
            available = 0;
            if (!WinHttpQueryDataAvailable(request, &available) || available == 0)
                break;
            std::string chunk(static_cast<size_t>(available), '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0)
            {
                ok = false;
                break;
            }
            chunk.resize(read);
            response += chunk;
            if (response.size() > 128 * 1024)
            {
                ok = false;
                break;
            }
        } while (available != 0);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}

static bool TranslationSendChatText(const std::wstring& text)
{
    if (text.empty())
        return false;

    SDK::UWorld* world = SDK::UWorld::GetWorld();
    if (!world || !world->OwningGameInstance || !world->OwningGameInstance->LocalPlayers.IsValid() ||
        world->OwningGameInstance->LocalPlayers.Num() <= 0)
        return false;
    SDK::ULocalPlayer* localPlayer = world->OwningGameInstance->LocalPlayers[0];
    SDK::APlayerController* controller = localPlayer ? localPlayer->PlayerController : nullptr;
    SDK::AHWPlayerState* playerState = SafeGetLocalHWPlayerState(controller);
    if (!playerState)
        return false;

    SDK::FHWChatEntry entry{};
    // The generated accessor lives in a SDK translation unit that is not part
    // of this DLL project; the replicated base field is public in the dump and
    // avoids adding another runtime dependency here.
    entry.SenderRHPlayerId = static_cast<SDK::ARHPlayerState*>(playerState)->RHPlayerUuid;
    entry.Message = SDK::FString(text.c_str());
    entry.ChatType = SDK::EHWChatEntryType::Player;
    __try
    {
        playerState->ServerSendChatEntry(entry);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TranslationGuidEquals(const SDK::FGuid& left, const SDK::FGuid& right)
{
    return left.A == right.A && left.B == right.B && left.C == right.C && left.D == right.D;
}

static std::wstring TranslationGuidDedupKey(const SDK::FGuid& guid)
{
    if (guid.A == 0 && guid.B == 0 && guid.C == 0 && guid.D == 0)
        return {};
    return std::to_wstring(guid.A) + L":" + std::to_wstring(guid.B) + L":" +
        std::to_wstring(guid.C) + L":" + std::to_wstring(guid.D);
}

static SDK::AHWPlayerState* TranslationFindPlayerState(const SDK::FGuid& senderId)
{
    __try
    {
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->GameState || !world->GameState->PlayerArray.IsValid())
            return nullptr;

        auto& players = world->GameState->PlayerArray;
        for (int index = 0; index < players.Num(); ++index)
        {
            SDK::APlayerState* player = players[index];
            if (!SafeObjectIsA(player, SDK::AHWPlayerState::StaticClass()))
                continue;
            auto* hwPlayer = static_cast<SDK::AHWPlayerState*>(player);
            const SDK::FGuid& playerId = static_cast<SDK::ARHPlayerState*>(hwPlayer)->RHPlayerUuid;
            if (TranslationGuidEquals(playerId, senderId))
                return hwPlayer;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
    return nullptr;
}

static void TranslationTrimLabel(std::wstring& value)
{
    const auto isWhitespace = [](wchar_t character)
    {
        return character == L' ' || character == L'\t' || character == L'\r' || character == L'\n';
    };
    while (!value.empty() && isWhitespace(value.front()))
        value.erase(value.begin());
    while (!value.empty() && isWhitespace(value.back()))
        value.pop_back();
}

static std::wstring TranslationStripRichTextTags(const std::wstring& value)
{
    std::wstring result;
    result.reserve(value.size());
    bool insideTag = false;
    for (wchar_t character : value)
    {
        if (character == L'<')
        {
            insideTag = true;
            continue;
        }
        if (insideTag)
        {
            if (character == L'>')
                insideTag = false;
            continue;
        }
        result.push_back(character);
    }
    return result;
}

static std::wstring TranslationDecoratedSenderPrefix(
    const std::wstring& decoratedMessage, const std::wstring& message)
{
    std::wstring prefix = TranslationStripRichTextTags(decoratedMessage);
    if (!message.empty())
    {
        const size_t messagePosition = prefix.rfind(message);
        if (messagePosition != std::wstring::npos)
            prefix.erase(messagePosition);
    }
    TranslationTrimLabel(prefix);
    while (!prefix.empty() &&
        (prefix.back() == L':' || prefix.back() == L'：' || prefix.back() == L'-'))
    {
        prefix.pop_back();
        TranslationTrimLabel(prefix);
    }
    return prefix;
}

static void TranslationSplitSenderPrefix(
    const std::wstring& prefix, std::wstring& playerName, std::wstring& godName)
{
    if (prefix.empty())
        return;

    size_t openParenthesis = prefix.rfind(L'(');
    const size_t fullWidthOpenParenthesis = prefix.rfind(L'（');
    if (fullWidthOpenParenthesis != std::wstring::npos &&
        (openParenthesis == std::wstring::npos || fullWidthOpenParenthesis > openParenthesis))
        openParenthesis = fullWidthOpenParenthesis;

    if (openParenthesis != std::wstring::npos)
    {
        size_t closeParenthesis = prefix.find(L')', openParenthesis + 1);
        const size_t fullWidthCloseParenthesis = prefix.find(L'）', openParenthesis + 1);
        if (fullWidthCloseParenthesis != std::wstring::npos &&
            (closeParenthesis == std::wstring::npos || fullWidthCloseParenthesis < closeParenthesis))
            closeParenthesis = fullWidthCloseParenthesis;
        if (closeParenthesis != std::wstring::npos && closeParenthesis > openParenthesis + 1)
            godName = prefix.substr(openParenthesis + 1, closeParenthesis - openParenthesis - 1);

        playerName = prefix.substr(0, openParenthesis);
        TranslationTrimLabel(playerName);
        while (!playerName.empty())
        {
            const wchar_t character = playerName.back();
            if (character == L' ' || character == L'\t' || character == L'\\' ||
                character == L'/' || character == L'|' || character == L'\x4E36')
                playerName.pop_back();
            else
                break;
        }
        TranslationTrimLabel(playerName);
        TranslationTrimLabel(godName);
        return;
    }

    playerName = prefix;
    TranslationTrimLabel(playerName);
}

static std::wstring TranslationGetDecoratedChatString(const SDK::UObject* object)
{
    if (!object || !SafeObjectIsA(
        const_cast<SDK::UObject*>(object), SDK::UHWChatEntryWidget::StaticClass()))
        return {};

    static SDK::UFunction* decoratedMessageFunction = nullptr;
    if (!decoratedMessageFunction)
        decoratedMessageFunction = SDK::UHWChatEntryWidget::StaticClass()->GetFunction(
            "HWChatEntryWidget", "GetDecoratedMessageString");
    if (!decoratedMessageFunction)
        return {};

    struct GetDecoratedMessageParams
    {
        SDK::FString ReturnValue;
    };
    static_assert(sizeof(GetDecoratedMessageParams) == 0x10);

    GetDecoratedMessageParams params{};
    const_cast<SDK::UObject*>(object)->ProcessEvent(decoratedMessageFunction, &params);
    return params.ReturnValue.ToWString();
}

static std::wstring TranslationBuildSenderLabel(
    const SDK::FHWChatEntry& entry, const std::wstring& decoratedMessage = {})
{
    std::string playerName;
    std::string godName;
    if (SDK::AHWPlayerState* playerState = TranslationFindPlayerState(entry.SenderRHPlayerId))
    {
        playerName = playerState->GetPlayerName().ToString();

        SDK::UObject* characterObject = playerState->GetCharacterBase();
        if (!characterObject)
            characterObject = playerState->DraftCharacterChoice;
        if (characterObject)
        {
            const std::string rawGodName = characterObject->GetName();
            const std::string localizedGodName = LocalizeGodDisplayName(rawGodName);
            if (!localizedGodName.empty() && localizedGodName != rawGodName)
                godName = localizedGodName;
        }
    }

    std::wstring decoratedPlayerName;
    std::wstring decoratedGodName;
    TranslationSplitSenderPrefix(
        TranslationDecoratedSenderPrefix(decoratedMessage, entry.Message.ToWString()),
        decoratedPlayerName, decoratedGodName);
    if (playerName.empty() && !decoratedPlayerName.empty())
        playerName = TranslationWideToUtf8(decoratedPlayerName);
    if (godName.empty() && !decoratedGodName.empty())
    {
        const std::string rawGodName = TranslationWideToUtf8(decoratedGodName);
        const std::string localizedGodName = LocalizeGodDisplayName(rawGodName);
        godName = localizedGodName.empty() ? rawGodName : localizedGodName;
    }

    if (godName.empty())
    {
        const std::wstring prompt = entry.PromptString.ToWString();
        if (!prompt.empty() && prompt != decoratedPlayerName)
        {
            const std::string rawGodName = TranslationWideToUtf8(prompt);
            const std::string localizedGodName = LocalizeGodDisplayName(rawGodName);
            godName = localizedGodName.empty() ? rawGodName : localizedGodName;
        }
    }

    std::string label = playerName;
    if (!godName.empty())
    {
        if (!label.empty())
            label += "(" + godName + ")";
        else
            label = godName;
    }
    if (!label.empty())
        return TranslationUtf8ToWide(label);

    return decoratedPlayerName.empty() ? entry.PromptString.ToWString() : decoratedPlayerName;
}

static std::wstring TranslationCleanModelOutput(std::wstring value, TranslationDirection direction)
{
    for (wchar_t& character : value)
    {
        if (character == L'\r' || character == L'\n' || character == L'\t')
            character = L' ';
    }

    std::wstring compact;
    compact.reserve(value.size());
    bool previousSpace = false;
    for (wchar_t character : value)
    {
        const bool isSpace = character == L' ';
        if (isSpace && previousSpace)
            continue;
        compact.push_back(character);
        previousSpace = isSpace;
    }
    value.swap(compact);
    TranslationTrimLabel(value);

    static const std::wstring prefixes[] = {
        L"Translation:", L"translation:", L"Translated text:",
        L"English:", L"english:", L"Chinese:", L"chinese:",
        L"Output:", L"output:", L"Answer:", L"answer:",
        L"翻译：", L"翻译:", L"译文：", L"译文:"
    };
    for (const std::wstring& prefix : prefixes)
    {
        if (value.rfind(prefix, 0) == 0)
        {
            value.erase(0, prefix.size());
            break;
        }
    }
    TranslationTrimLabel(value);

    while (value.size() >= 2)
    {
        const wchar_t first = value.front();
        const wchar_t last = value.back();
        const bool wrapped = (first == L'"' && last == L'"') ||
            (first == L'`' && last == L'`') || (first == L'“' && last == L'”') ||
            (first == L'‘' && last == L'’');
        if (!wrapped)
            break;
        value.erase(value.begin());
        value.pop_back();
        TranslationTrimLabel(value);
    }

    while (value.size() >= 2 &&
        ((value[0] == L'-' || value[0] == L'*' || value[0] == L'•') && value[1] == L' '))
    {
        value.erase(0, 2);
        TranslationTrimLabel(value);
    }

    if (direction == TranslationDirection::Outgoing)
    {
        std::wstring casual;
        casual.reserve(value.size());
        for (wchar_t character : value)
        {
            const bool asciiLetter = (character >= L'A' && character <= L'Z') ||
                (character >= L'a' && character <= L'z');
            const bool asciiDigit = character >= L'0' && character <= L'9';
            const bool cjk = (character >= 0x3400 && character <= 0x4DBF) ||
                (character >= 0x4E00 && character <= 0x9FFF) ||
                (character >= 0xF900 && character <= 0xFAFF);
            if (asciiLetter || asciiDigit)
            {
                casual.push_back(character);
            }
            else if (cjk)
            {
                // Keep CJK until validation so a failed translation is never
                // reduced to a partial English fragment and sent.
                casual.push_back(character);
            }
            else if (character == L'\'' || character == L'’')
            {
                // Drop apostrophes without splitting common chat contractions.
            }
            else
            {
                if (!casual.empty() && casual.back() != L' ')
                    casual.push_back(L' ');
            }
        }
        value.swap(casual);

        compact.clear();
        previousSpace = false;
        for (wchar_t character : value)
        {
            const bool isSpace = character == L' ';
            if (isSpace && previousSpace)
                continue;
            compact.push_back(character);
            previousSpace = isSpace;
        }
        value.swap(compact);
        TranslationTrimLabel(value);
    }

    return value;
}

static void TranslationClearChatInput(const SDK::UObject* object)
{
    if (!object || !SafeObjectIsA(const_cast<SDK::UObject*>(object), SDK::UHWChatWindowWidget::StaticClass()))
        return;

    auto* chatWindow = const_cast<SDK::UHWChatWindowWidget*>(
        static_cast<const SDK::UHWChatWindowWidget*>(object));
    if (!chatWindow->EditableText)
        return;

    static SDK::UFunction* setTextFunction = nullptr;
    if (!setTextFunction)
        setTextFunction = SDK::UEditableTextBox::StaticClass()->GetFunction("EditableTextBox", "SetText");
    if (!setTextFunction)
        return;

    struct SetTextParams
    {
        SDK::FText InText;
    };
    SetTextParams params{};
    params.InText = SDK::UKismetTextLibrary::Conv_StringToText(SDK::FString(L""));
    chatWindow->EditableText->ProcessEvent(setTextFunction, &params);
}

static void TranslationUpdateChatAnchor(const SDK::UObject* object)
{
    if (!object || !SafeObjectIsA(const_cast<SDK::UObject*>(object), SDK::UHWChatWindowWidget::StaticClass()))
        return;

    auto* chatWindow = const_cast<SDK::UHWChatWindowWidget*>(
        static_cast<const SDK::UHWChatWindowWidget*>(object));
    SDK::UWidget* anchorWidget = chatWindow;
    if (SafeObjectIsA(chatWindow, SDK::UWBP_G_V2_ChatWindow_C::StaticClass()))
    {
        auto* chatBlueprint = static_cast<SDK::UWBP_G_V2_ChatWindow_C*>(chatWindow);
        if (chatBlueprint->WindowSizeBox)
            anchorWidget = chatBlueprint->WindowSizeBox;
        else if (chatBlueprint->VisibleContainer)
            anchorWidget = chatBlueprint->VisibleContainer;
    }
    if (!anchorWidget)
        return;

    static SDK::UFunction* getCachedGeometryFunction = nullptr;
    static SDK::UFunction* getLocalSizeFunction = nullptr;
    static SDK::UFunction* localToViewportFunction = nullptr;
    if (!getCachedGeometryFunction)
        getCachedGeometryFunction = SDK::UWidget::StaticClass()->GetFunction("Widget", "GetCachedGeometry");
    if (!getLocalSizeFunction)
        getLocalSizeFunction = SDK::USlateBlueprintLibrary::StaticClass()->GetFunction(
            "SlateBlueprintLibrary", "GetLocalSize");
    if (!localToViewportFunction)
        localToViewportFunction = SDK::USlateBlueprintLibrary::StaticClass()->GetFunction(
            "SlateBlueprintLibrary", "LocalToViewport");
    SDK::USlateBlueprintLibrary* slateLibrary = SDK::USlateBlueprintLibrary::GetDefaultObj();
    if (!getCachedGeometryFunction || !getLocalSizeFunction || !localToViewportFunction || !slateLibrary)
        return;

    struct GetCachedGeometryParams
    {
        SDK::FGeometry ReturnValue;
    };
    struct GetLocalSizeParams
    {
        SDK::FGeometry Geometry;
        SDK::FVector2D ReturnValue;
    };
    struct LocalToViewportParams
    {
        SDK::UObject* WorldContextObject;
        SDK::FGeometry Geometry;
        SDK::FVector2D LocalCoordinate;
        SDK::FVector2D PixelPosition;
        SDK::FVector2D ViewportPosition;
    };
    static_assert(sizeof(GetCachedGeometryParams) == 0x38);
    static_assert(sizeof(GetLocalSizeParams) == 0x48);
    static_assert(sizeof(LocalToViewportParams) == 0x70);

    GetCachedGeometryParams geometryParams{};
    anchorWidget->ProcessEvent(getCachedGeometryFunction, &geometryParams);

    GetLocalSizeParams sizeParams{};
    sizeParams.Geometry = geometryParams.ReturnValue;
    slateLibrary->ProcessEvent(getLocalSizeFunction, &sizeParams);
    if (!std::isfinite(sizeParams.ReturnValue.X) || !std::isfinite(sizeParams.ReturnValue.Y) ||
        sizeParams.ReturnValue.X < 100.0 || sizeParams.ReturnValue.Y < 40.0)
        return;

    LocalToViewportParams topLeftParams{};
    topLeftParams.WorldContextObject = chatWindow;
    topLeftParams.Geometry = geometryParams.ReturnValue;
    slateLibrary->ProcessEvent(localToViewportFunction, &topLeftParams);

    LocalToViewportParams bottomRightParams{};
    bottomRightParams.WorldContextObject = chatWindow;
    bottomRightParams.Geometry = geometryParams.ReturnValue;
    bottomRightParams.LocalCoordinate = sizeParams.ReturnValue;
    slateLibrary->ProcessEvent(localToViewportFunction, &bottomRightParams);

    const double width = bottomRightParams.ViewportPosition.X - topLeftParams.ViewportPosition.X;
    const double height = bottomRightParams.ViewportPosition.Y - topLeftParams.ViewportPosition.Y;
    if (!std::isfinite(topLeftParams.ViewportPosition.X) || !std::isfinite(topLeftParams.ViewportPosition.Y) ||
        !std::isfinite(width) || !std::isfinite(height) || width < 100.0 || height < 40.0)
        return;

    std::lock_guard<std::mutex> lock(g_translationChatAnchorMutex);
    g_translationChatAnchor.topLeft = ImVec2(
        static_cast<float>(topLeftParams.ViewportPosition.X),
        static_cast<float>(topLeftParams.ViewportPosition.Y));
    g_translationChatAnchor.size = ImVec2(static_cast<float>(width), static_cast<float>(height));
    g_translationChatAnchor.valid = true;
}

class TranslationRuntime
{
public:
    void Start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_started)
            return;
        m_stop = false;
        m_started = true;
        m_nextDeliverId = m_nextId + 1;
        m_workers.reserve(2);
        for (size_t index = 0; index < 2; ++index)
            m_workers.emplace_back([this]() { WorkerMain(); });
    }

    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_started)
                return;
            m_stop = true;
            m_requests.clear();
            m_completedResults.clear();
        }
        m_cv.notify_all();
        for (std::thread& worker : m_workers)
        {
            if (worker.joinable())
                worker.join();
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_workers.clear();
        m_started = false;
        m_activeRequests = 0;
        m_recentIncoming.clear();
        m_nextDeliverId = m_nextId + 1;
    }

    bool Submit(TranslationDirection direction, const std::wstring& text,
        const std::wstring& senderLabel = {}, const std::wstring& incomingIdentity = {})
    {
        if (text.empty() || text.size() > 512)
            return false;

        // Hold Chinese outgoing text until a successful translation exists.
        // If the key is unavailable or the queue is full, suppress the
        // original rather than leaking Chinese into the game chat.
        if (!TranslationHasApiKey())
        {
            if (direction == TranslationDirection::Outgoing)
                m_status.store(3, std::memory_order_relaxed);
            return direction == TranslationDirection::Outgoing;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_requests.size() >= 256)
        {
            if (direction == TranslationDirection::Outgoing)
                m_status.store(3, std::memory_order_relaxed);
            return direction == TranslationDirection::Outgoing;
        }
        if (direction == TranslationDirection::Incoming)
        {
            const ULONGLONG now = GetTickCount64();
            // The same chat entry passes through several native and Blueprint
            // callbacks in one UI update.  Only merge those callbacks; do not
            // suppress a player who intentionally repeats the same short text.
            while (!m_recentIncoming.empty() && now - m_recentIncoming.front().second > 750)
                m_recentIncoming.pop_front();
            const std::wstring& identity = incomingIdentity.empty() ? senderLabel : incomingIdentity;
            const std::wstring dedupKey = identity + L"\x1F" + text;
            for (const auto& recent : m_recentIncoming)
            {
                if (recent.first == dedupKey)
                    return false;
            }
            m_recentIncoming.emplace_back(dedupKey, now);
        }

        if (!m_started)
        {
            m_stop = false;
            m_started = true;
            m_nextDeliverId = m_nextId + 1;
            m_workers.reserve(2);
            for (size_t index = 0; index < 2; ++index)
                m_workers.emplace_back([this]() { WorkerMain(); });
        }

        TranslationRequest request{};
        request.id = ++m_nextId;
        request.direction = direction;
        request.text = text;
        request.senderLabel = senderLabel;
        m_requests.push_back(std::move(request));
        m_status.store(2, std::memory_order_relaxed);
        m_cv.notify_one();
        return true;
    }

    void TickGameThread()
    {
        std::deque<TranslationResult> results;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (;;)
            {
                auto completed = m_completedResults.find(m_nextDeliverId);
                if (completed == m_completedResults.end())
                    break;
                results.push_back(std::move(completed->second));
                m_completedResults.erase(completed);
                ++m_nextDeliverId;
            }
        }

        for (TranslationResult& result : results)
        {
            if (result.direction == TranslationDirection::Incoming)
            {
                if (result.success)
                {
                    std::lock_guard<std::mutex> lock(m_lineMutex);
                    m_lines.push_back({ result.source, result.translated, result.senderLabel, GetTickCount64() });
                    while (m_lines.size() > 10)
                        m_lines.pop_front();
                }
            }
            else
            {
                // Never fall back to the original Chinese message.  The hook
                // suppresses it until a successful English translation exists.
                if (result.success)
                    TranslationSendChatText(result.translated);
            }
        }
    }

    void RenderOverlay()
    {
        const bool positionEditing = g_translationPositionEditEnabled && render_ui::IsVisible();
        if ((!g_translationEnabled || !g_translationIncomingEnabled) && !positionEditing)
            return;

        std::deque<TranslationLine> lines;
        {
            std::lock_guard<std::mutex> lock(m_lineMutex);
            const ULONGLONG now = GetTickCount64();
            while (!m_lines.empty() && now - m_lines.front().createdAtMs > 60000)
                m_lines.pop_front();
            lines = m_lines;
        }
        if (lines.empty() && !positionEditing)
            return;

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        TranslationChatAnchor chatAnchor{};
        {
            std::lock_guard<std::mutex> lock(g_translationChatAnchorMutex);
            chatAnchor = g_translationChatAnchor;
        }

        float overlayMaxWidth = std::min(760.0f, displaySize.x - 36.0f);
        ImVec2 overlayAnchor(18.0f, displaySize.y - 320.0f);
        if (chatAnchor.valid && chatAnchor.topLeft.x >= 0.0f && chatAnchor.topLeft.y >= 80.0f &&
            chatAnchor.topLeft.x < displaySize.x && chatAnchor.topLeft.y < displaySize.y)
        {
            overlayMaxWidth = std::clamp(chatAnchor.size.x, 320.0f, overlayMaxWidth);
            overlayAnchor.x = std::clamp(chatAnchor.topLeft.x, 12.0f, displaySize.x - overlayMaxWidth - 12.0f);
            overlayAnchor.y = chatAnchor.topLeft.y + 28.0f;
        }
        else
        {
            overlayMaxWidth = std::min(620.0f, overlayMaxWidth);
            overlayAnchor.y = std::max(210.0f, displaySize.y * 0.78f);
        }
        const bool hasCustomPosition = g_translationOverlayPositionCustom &&
            std::isfinite(g_translationOverlayPosition.x) && std::isfinite(g_translationOverlayPosition.y);
        if (hasCustomPosition)
        {
            g_translationOverlayPosition.x = std::clamp(
                g_translationOverlayPosition.x, 0.0f, std::max(0.0f, displaySize.x - 80.0f));
            g_translationOverlayPosition.y = std::clamp(
                g_translationOverlayPosition.y, 0.0f, std::max(0.0f, displaySize.y - 50.0f));
            ImGui::SetNextWindowPos(g_translationOverlayPosition, ImGuiCond_Always);
        }
        else
        {
            ImGui::SetNextWindowPos(overlayAnchor, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        }
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(320.0f, 0.0f),
            ImVec2(overlayMaxWidth, std::max(190.0f, std::min(420.0f, displaySize.y - 60.0f))));
        ImGui::SetNextWindowBgAlpha(0.72f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        if (!positionEditing)
            flags |= ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("##chat_translation_overlay", nullptr, flags))
        {
            static bool draggingTranslationWindow = false;
            if (positionEditing)
            {
                if (!g_translationOverlayPositionCustom)
                {
                    g_translationOverlayPosition = ImGui::GetWindowPos();
                    g_translationOverlayPositionCustom = true;
                }
                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    draggingTranslationWindow = true;
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    draggingTranslationWindow = false;
                if (draggingTranslationWindow)
                {
                    const ImVec2 windowSize = ImGui::GetWindowSize();
                    ImVec2 newPosition = ImGui::GetWindowPos() + ImGui::GetIO().MouseDelta;
                    newPosition.x = std::clamp(
                        newPosition.x, 0.0f, std::max(0.0f, displaySize.x - windowSize.x));
                    newPosition.y = std::clamp(
                        newPosition.y, 0.0f, std::max(0.0f, displaySize.y - windowSize.y));
                    ImGui::SetWindowPos(newPosition, ImGuiCond_Always);
                    g_translationOverlayPosition = newPosition;
                }
            }

            const auto& fonts = odyssey::theme::GetFonts();
            ImFont* translationFont = fonts.title ? fonts.title : (fonts.ui ? fonts.ui : ImGui::GetFont());
            ImGui::PushFont(translationFont);
            ImGui::SetWindowFontScale(1.18f);
            ImGui::TextColored(ImVec4(0.55f, 0.78f, 1.0f, 1.0f), "%s", "翻译");
            if (lines.empty())
                ImGui::TextUnformatted("翻译框位置");
            for (const TranslationLine& line : lines)
            {
                std::wstring displayLine;
                if (!line.senderLabel.empty())
                {
                    displayLine = line.senderLabel;
                    displayLine += L"：";
                }
                displayLine += line.translated;
                const std::string translated = TranslationWideToUtf8(displayLine);
                ImGui::TextWrapped("%s", translated.c_str());
            }
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
        }
        ImGui::End();
    }

    void SetStatus(int value)
    {
        m_status.store(value, std::memory_order_relaxed);
    }

    int Status() const
    {
        return m_status.load(std::memory_order_relaxed);
    }

private:
    void WorkerMain()
    {
        for (;;)
        {
            TranslationRequest request{};
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() { return m_stop || !m_requests.empty(); });
                if (m_stop)
                    return;
                request = std::move(m_requests.front());
                m_requests.pop_front();
                ++m_activeRequests;
            }

            TranslationResult result{};
            result.id = request.id;
            result.direction = request.direction;
            result.source = request.text;
            result.senderLabel = request.senderLabel;

            const std::wstring apiKey = TranslationReadApiKey();
            const std::string source = TranslationWideToUtf8(request.text);
            const std::string systemPrompt = (request.direction == TranslationDirection::Incoming
                ? "Translate exactly one live player message from SMITE 2 into concise natural Simplified Chinese. Infer the intended meaning from SMITE 2 and MOBA match context. Understand informal gamer slang, abbreviations, clipped words, misspellings, trash talk, roles, lanes, objectives, items and ability references. Translate the whole meaning instead of translating word by word. Treat unknown proper nouns as player or character names and keep them rather than inventing meanings. Output exactly one Chinese chat line only, with no source English, labels, quotes, Markdown, explanations, emojis or decorative symbols."
                : "Translate exactly one Simplified Chinese SMITE 2 team message into short natural everyday English used by players during a match. Preserve every standalone Latin letter, number, player ID, character name and key name exactly; never change A into B or substitute one letter for another. Express the complete intended meaning without literal web-translation wording. Output only ASCII English letters, digits and spaces. Do not output punctuation, apostrophes, symbols, emojis, labels, quotes, Markdown or explanations.");
            const std::string body =
                "{\"model\":\"deepseek-v4-flash\",\"thinking\":{\"type\":\"disabled\"},\"messages\":[{\"role\":\"system\",\"content\":\"" +
                TranslationJsonEscape(systemPrompt) + "\"},{\"role\":\"user\",\"content\":\"" +
                TranslationJsonEscape(source) + "\"}],\"temperature\":0.0,\"max_tokens\":160,\"stream\":false}";

            std::string response;
            std::string translatedUtf8;
            result.success = !apiKey.empty() && !source.empty() && TranslationHttpPost(apiKey, body, response) &&
                TranslationExtractResponseText(response, translatedUtf8);
            if (result.success)
            {
                result.translated = TranslationCleanModelOutput(
                    TranslationUtf8ToWide(translatedUtf8), request.direction);
                result.success = request.direction == TranslationDirection::Incoming
                    ? TranslationLooksLikeValidIncomingResult(result.translated)
                    : TranslationLooksLikeValidOutgoingResult(result.translated);
            }

            const bool requestSucceeded = result.success;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_activeRequests > 0)
                    --m_activeRequests;
                if (!m_stop)
                    m_completedResults.emplace(result.id, std::move(result));
                const bool stillBusy = !m_requests.empty() || m_activeRequests > 0;
                m_status.store(stillBusy ? 2 : (requestSucceeded ? 1 : 3), std::memory_order_relaxed);
            }
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<TranslationRequest> m_requests;
    std::map<uint64_t, TranslationResult> m_completedResults;
    std::deque<std::pair<std::wstring, ULONGLONG>> m_recentIncoming;
    std::vector<std::thread> m_workers;
    bool m_started = false;
    bool m_stop = false;
    size_t m_activeRequests = 0;
    uint64_t m_nextId = 0;
    uint64_t m_nextDeliverId = 1;
    std::atomic<int> m_status{ 0 };

    std::mutex m_lineMutex;
    std::deque<TranslationLine> m_lines;
};

static TranslationRuntime g_translationRuntime;

static bool TranslationHasApiKey()
{
    static std::mutex mutex;
    static ULONGLONG lastCheckMs = 0;
    static bool hasKey = false;
    const ULONGLONG now = GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex);
    if (now - lastCheckMs >= 1000)
    {
        hasKey = !TranslationReadApiKey().empty();
        lastCheckMs = now;
        if (hasKey && g_translationRuntime.Status() == 0)
            g_translationRuntime.SetStatus(1);
    }
    return hasKey;
}

static const char* TranslationStatusText()
{
    if (!g_translationEnabled)
        return "翻译：关闭";
    if (!TranslationHasApiKey())
        return "翻译：请在桌面 DeepSeek_API_Key.txt 填入密钥";
    switch (g_translationRuntime.Status())
    {
    case 2: return "翻译：请求中";
    case 3: return "翻译：上次请求失败，未发送消息";
    default: return "翻译：已就绪";
    }
}

using TranslationProcessEventFn = void(__fastcall*)(const SDK::UObject*, SDK::UFunction*, void*);
static TranslationProcessEventFn g_translationOriginalProcessEvent = nullptr;
static void* g_translationProcessEventTarget = nullptr;
static SDK::UFunction* g_translationIncomingFunction = nullptr;
static SDK::UFunction* g_translationIncomingAddFunction = nullptr;
static SDK::UFunction* g_translationIncomingBlueprintFunction = nullptr;
static SDK::UFunction* g_translationIncomingAddBlueprintFunction = nullptr;
static SDK::UFunction* g_translationChatEntrySetDataFunction = nullptr;
static SDK::UFunction* g_translationChatEntrySetDataBlueprintFunction = nullptr;
static SDK::UFunction* g_translationOutgoingFunction = nullptr;
static bool g_translationProcessEventHookInstalled = false;

struct TranslationChatEntryReceivedParams
{
    SDK::FHWChatEntry ChatEntry;
};

struct TranslationChatTextCommittedParams
{
    SDK::FText Text;
    SDK::ETextCommit CommitMethod;
};

static void __fastcall hkTranslationProcessEvent(const SDK::UObject* object, SDK::UFunction* function, void* params)
{
    if (!g_translationOriginalProcessEvent)
        return;

    bool suppressOriginal = false;
    bool updateChatAnchor = false;
    const TranslationChatEntryReceivedParams* incomingEntryParams = nullptr;
    if (g_translationEnabled && object && function && params)
    {
        const bool isIncomingEvent =
            function == g_translationIncomingFunction ||
            function == g_translationIncomingAddFunction ||
            function == g_translationIncomingBlueprintFunction ||
            function == g_translationIncomingAddBlueprintFunction ||
            function == g_translationChatEntrySetDataFunction ||
            function == g_translationChatEntrySetDataBlueprintFunction;
        if (isIncomingEvent && g_translationIncomingEnabled)
        {
            updateChatAnchor = SafeObjectIsA(
                const_cast<SDK::UObject*>(object), SDK::UHWChatWindowWidget::StaticClass());
            incomingEntryParams = static_cast<const TranslationChatEntryReceivedParams*>(params);
        }
        else if (function == g_translationOutgoingFunction && g_translationOutgoingEnabled)
        {
            updateChatAnchor = true;
            const auto* commitParams = static_cast<const TranslationChatTextCommittedParams*>(params);
            if (commitParams->CommitMethod == SDK::ETextCommit::OnEnter && commitParams->Text.TextData)
            {
                const std::wstring text = commitParams->Text.ToString().empty()
                    ? std::wstring{}
                    : TranslationUtf8ToWide(commitParams->Text.ToString());
                if (TranslationLooksLikeChineseOutgoing(text))
                {
                    suppressOriginal = g_translationRuntime.Submit(TranslationDirection::Outgoing, text);
                    if (suppressOriginal)
                        TranslationClearChatInput(object);
                }
            }
        }
    }

    if (!suppressOriginal)
        g_translationOriginalProcessEvent(object, function, params);
    if (updateChatAnchor)
        TranslationUpdateChatAnchor(object);
    if (incomingEntryParams && incomingEntryParams->ChatEntry.ChatType == SDK::EHWChatEntryType::Player)
    {
        const std::wstring text = incomingEntryParams->ChatEntry.Message.ToWString();
        if (TranslationLooksLikeEnglish(text))
        {
            const std::wstring decoratedMessage = TranslationGetDecoratedChatString(object);
            const std::wstring senderLabel = TranslationBuildSenderLabel(
                incomingEntryParams->ChatEntry, decoratedMessage);
            const std::wstring senderIdentity = TranslationGuidDedupKey(
                incomingEntryParams->ChatEntry.SenderRHPlayerId);
            g_translationRuntime.Submit(
                TranslationDirection::Incoming, text, senderLabel, senderIdentity);
        }
    }
}

static void TranslationTryInstallHook()
{
    if (!g_translationEnabled || g_translationProcessEventHookInstalled)
        return;

    static ULONGLONG lastAttemptMs = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - lastAttemptMs < 1000)
        return;
    lastAttemptMs = now;

    SDK::UClass* chatClass = SDK::UHWChatWindowWidget::StaticClass();
    if (!chatClass || !chatClass->ClassDefaultObject)
        return;
    SDK::UClass* chatBlueprintClass = SDK::UWBP_G_V2_ChatWindow_C::StaticClass();
    SDK::UClass* entryClass = SDK::UHWChatEntryWidget::StaticClass();
    SDK::UClass* entryBlueprintClass = SDK::UWBP_G_V2_ChatEntry_C::StaticClass();
    SDK::UFunction* incoming = chatClass->GetFunction("HWChatWindowWidget", "HandleChatEntryReceived");
    SDK::UFunction* incomingAdd = chatClass->GetFunction("HWChatWindowWidget", "AddChatEntryToWindow");
    SDK::UFunction* incomingBlueprint = chatBlueprintClass
        ? chatBlueprintClass->GetFunction("WBP_G_V2_ChatWindow_C", "HandleChatEntryReceived") : nullptr;
    SDK::UFunction* incomingAddBlueprint = chatBlueprintClass
        ? chatBlueprintClass->GetFunction("WBP_G_V2_ChatWindow_C", "AddChatEntryToWindow") : nullptr;
    SDK::UFunction* setData = entryClass
        ? entryClass->GetFunction("HWChatEntryWidget", "SetChatData") : nullptr;
    SDK::UFunction* setDataBlueprint = entryBlueprintClass
        ? entryBlueprintClass->GetFunction("WBP_G_V2_ChatEntry_C", "SetChatData") : nullptr;
    SDK::UFunction* outgoing = chatClass->GetFunction("HWChatWindowWidget", "HandleChatTextCommitted");
    if (!incoming && !incomingAdd && !incomingBlueprint && !incomingAddBlueprint && !setData && !setDataBlueprint && !outgoing)
        return;

    void* target = SDK::InSDKUtils::GetVirtualFunction<void*>(chatClass->ClassDefaultObject, SDK::Offsets::ProcessEventIdx);
    if (!target)
        return;

    if (MH_CreateHook(target, &hkTranslationProcessEvent, reinterpret_cast<void**>(&g_translationOriginalProcessEvent)) != MH_OK)
        return;
    if (MH_EnableHook(target) != MH_OK)
    {
        MH_RemoveHook(target);
        g_translationOriginalProcessEvent = nullptr;
        return;
    }

    g_translationProcessEventTarget = target;
    g_translationIncomingFunction = incoming;
    g_translationIncomingAddFunction = incomingAdd;
    g_translationIncomingBlueprintFunction = incomingBlueprint;
    g_translationIncomingAddBlueprintFunction = incomingAddBlueprint;
    g_translationChatEntrySetDataFunction = setData;
    g_translationChatEntrySetDataBlueprintFunction = setDataBlueprint;
    g_translationOutgoingFunction = outgoing;
    g_translationProcessEventHookInstalled = true;
}

static void TranslationTickGameThread()
{
    if (g_translationEnabled)
        TranslationTryInstallHook();
    g_translationRuntime.TickGameThread();
}

static void TranslationRenderOverlay()
{
    g_translationRuntime.RenderOverlay();
}

static void TranslationStopRuntime()
{
    if (g_translationProcessEventHookInstalled && g_translationProcessEventTarget)
    {
        MH_DisableHook(g_translationProcessEventTarget);
        MH_RemoveHook(g_translationProcessEventTarget);
    }
    g_translationProcessEventHookInstalled = false;
    g_translationProcessEventTarget = nullptr;
    g_translationOriginalProcessEvent = nullptr;
    g_translationIncomingFunction = nullptr;
    g_translationIncomingAddFunction = nullptr;
    g_translationIncomingBlueprintFunction = nullptr;
    g_translationIncomingAddBlueprintFunction = nullptr;
    g_translationChatEntrySetDataFunction = nullptr;
    g_translationChatEntrySetDataBlueprintFunction = nullptr;
    g_translationOutgoingFunction = nullptr;
    g_translationRuntime.Stop();
}

// Wrapper for the debug menu renderer
//
// The debug menu implementation defines its Render() function within the
// `render_ui` namespace.  Our trainer however calls a global Render()
// function when g_useModernMenu is enabled.  To satisfy that symbol
// reference and forward the call correctly, we provide this simple wrapper.
// Without this wrapper the linker will report an unresolved external symbol
// for Render().
void Render() {
    // Forward to the namespaced implementation.  Qualifying the call avoids
    // accidental recursion and clearly identifies which Render() we intend
    // to invoke.  This wrapper has no side effects; it merely delegates
    // drawing to the menu defined in ModernMenu.inl.
    render_ui::Render();
}

// -------------------- Utilities --------------------
template <typename T>
static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

static void Dx12DebugLog(const char* format, ...)
{
    char message[1024]{};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);

    OutputDebugStringA(message);
    OutputDebugStringA("\n");

    char tempPath[MAX_PATH]{};
    if (!GetTempPathA(static_cast<DWORD>(sizeof(tempPath)), tempPath))
        return;

    char logPath[MAX_PATH]{};
    sprintf_s(logPath, sizeof(logPath), "%s%s", tempPath, "oopz_dx12.log");
    HANDLE file = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    char line[1280]{};
    const int count = sprintf_s(line, sizeof(line),
        "[%02u:%02u:%02u.%03u] %s\r\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
    if (count > 0) {
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
    }
    CloseHandle(file);
}

static ID3D12CommandQueue* AtomicReadCommandQueue()
{
    return reinterpret_cast<ID3D12CommandQueue*>(
        InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&g_commandQueue), nullptr, nullptr));
}

struct ScopedPresentRenderLock
{
    bool acquired = false;

    ScopedPresentRenderLock()
    {
        acquired = (InterlockedCompareExchange(&g_presentRenderLock, 1, 0) == 0);
    }

    ~ScopedPresentRenderLock()
    {
        if (acquired)
            InterlockedExchange(&g_presentRenderLock, 0);
    }
};

static void ReleaseCapturedCommandQueue()
{
    ID3D12CommandQueue* queue = reinterpret_cast<ID3D12CommandQueue*>(
        InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(&g_commandQueue), nullptr));
    SafeRelease(queue);
}

static bool QueueBelongsToDevice(ID3D12CommandQueue* queue, ID3D12Device* device)
{
    if (!queue || !device)
        return false;

    ID3D12Device* queueDevice = nullptr;
    if (FAILED(queue->GetDevice(IID_PPV_ARGS(&queueDevice))) || !queueDevice)
        return false;

    const bool matches = (queueDevice == device);
    SafeRelease(queueDevice);
    return matches;
}

static bool TryCaptureDX12CommandQueue(ID3D12CommandQueue* queue)
{
    if (!queue)
        return false;

    const D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
    if (desc.Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
        return false;

    if (g_device && !QueueBelongsToDevice(queue, g_device))
        return false;

    queue->AddRef();
    void* previous = InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(&g_commandQueue),
        queue,
        nullptr);

    if (previous == nullptr) {
        Dx12DebugLog("Captured DX12 direct command queue: %p", queue);
        return true;
    }

    queue->Release();
    return previous == queue;
}

static bool TryCaptureDX12CommandQueueFromUnknown(IUnknown* deviceOrQueue)
{
    if (!deviceOrQueue)
        return false;

    ID3D12CommandQueue* queue = nullptr;
    if (FAILED(deviceOrQueue->QueryInterface(IID_PPV_ARGS(&queue))) || !queue)
        return false;

    const bool captured = TryCaptureDX12CommandQueue(queue);
    queue->Release();
    return captured;
}

static bool Dx12AllocateSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
{
    if (!g_device || !g_srvHeap || !outCpu || !outGpu)
        return false;

    if (g_srvDescriptorSize == 0)
        g_srvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    const D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = g_srvHeap->GetCPUDescriptorHandleForHeapStart();
    const D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = g_srvHeap->GetGPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kDx12SrvDescriptorCount; ++i) {
        if (!g_srvDescriptorUsed[i]) {
            g_srvDescriptorUsed[i] = true;
            outCpu->ptr = cpuStart.ptr + static_cast<SIZE_T>(i) * g_srvDescriptorSize;
            outGpu->ptr = gpuStart.ptr + static_cast<UINT64>(i) * g_srvDescriptorSize;
            return true;
        }
    }
    return false;
}

static void Dx12FreeSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpu)
{
    if (!g_srvHeap || g_srvDescriptorSize == 0 || cpu.ptr == 0)
        return;

    const D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = g_srvHeap->GetCPUDescriptorHandleForHeapStart();
    if (cpu.ptr < cpuStart.ptr)
        return;

    const SIZE_T offset = cpu.ptr - cpuStart.ptr;
    if ((offset % g_srvDescriptorSize) != 0)
        return;

    const UINT index = static_cast<UINT>(offset / g_srvDescriptorSize);
    if (index < kDx12SrvDescriptorCount)
        g_srvDescriptorUsed[index] = false;
}

static void Dx12SrvDescriptorAlloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
{
    if (!Dx12AllocateSrvDescriptor(outCpu, outGpu)) {
        if (outCpu) outCpu->ptr = 0;
        if (outGpu) outGpu->ptr = 0;
    }
}

static void Dx12SrvDescriptorFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
{
    Dx12FreeSrvDescriptor(cpu);
}

static bool icontains(const std::string& hay, const char* needle) {
    std::string h = hay, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Normalise a god name by converting to lower case, stripping blueprint
// prefixes/suffixes (e.g., "BP_GOD_", "BP_", "GOD_") and removing
// underscores.  This helper is used to look up predefined projectile
// speeds in g_projSpeedMap.
static std::string NormalizeGodName(const std::string& in) {
    std::string s = in;
    // Lowercase the string
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
        });
    // Remove known prefixes
    const char* prefixes[] = { "bp_god_", "bp_", "god_" };
    for (const char* pre : prefixes) {
        size_t len = strlen(pre);
        if (s.size() >= len && _strnicmp(s.c_str(), pre, len) == 0) {
            s.erase(0, len);
            break;
        }
    }
    // Strip trailing suffix after the last underscore if numeric or 'c'
    size_t last = s.find_last_of('_');
    if (last != std::string::npos) {
        std::string suf = s.substr(last + 1);
        bool numeric = !suf.empty() && std::all_of(suf.begin(), suf.end(), [](char c) {
            return isdigit(static_cast<unsigned char>(c));
            });
        if (numeric || suf == "c") {
            s.erase(last);
        }
    }
    // Remove remaining underscores
    s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
    return s;
}

static std::string CanonicalGodKey(const std::string& in)
{
    std::string s = NormalizeGodName(in);
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

static std::string LocalizeGodDisplayName(const std::string& name)
{
    struct GodNameEntry
    {
        const char* key;
        const char* en;
        const char* zh;
        const char* ru;
        const char* es;
        const char* pt;
        const char* ja;
        const char* ko;
    };

    static const GodNameEntry names[] = {
        {"xingtian", "Xing Tian", "刑天", "Син Тянь", "Xing Tian", "Xing Tian", "刑天", "싱톈"},
        {"achilles", "Achilles", "阿喀琉斯", "Ахиллес", "Aquiles", "Aquiles", "アキレウス", "아킬레우스"},
        {"agni", "Agni", "阿格尼", "Агни", "Agni", "Agni", "アグニ", "아그니"},
        {"aladdin", "Aladdin", "阿拉丁", "Аладдин", "Aladdín", "Aladdin", "アラジン", "알라딘"},
        {"amaterasu", "Amaterasu", "天照大神", "Аматэрасу", "Amaterasu", "Amaterasu", "アマテラス", "아마테라스"},
        {"anhur", "Anhur", "安赫", "Анхур", "Anhur", "Anhur", "アンハー", "안후르"},
        {"anubis", "Anubis", "阿努比斯", "Анубис", "Anubis", "Anúbis", "アヌビス", "아누비스"},
        {"aphrodite", "Aphrodite", "阿弗洛狄忒", "Афродита", "Afrodita", "Afrodite", "アフロディーテ", "아프로디테"},
        {"apollo", "Apollo", "阿波罗", "Аполлон", "Apolo", "Apolo", "アポロ", "아폴로"},
        {"ares", "Ares", "阿瑞斯", "Арес", "Ares", "Ares", "アレス", "아레스"},
        {"artemis", "Artemis", "阿尔忒弥斯", "Артемида", "Artemisa", "Ártemis", "アルテミス", "아르테미스"},
        {"artio", "Artio", "阿尔提奥", "Артио", "Artio", "Artio", "アルティオ", "아르티오"},
        {"arthur", "King Arthur", "亚瑟王", "Король Артур", "Rey Arturo", "Rei Arthur", "アーサー王", "아서 왕"},
        {"athena", "Athena", "雅典娜", "Афина", "Atenea", "Atena", "アテナ", "아테나"},
        {"awilix", "Awilix", "阿维利克斯", "Авиликс", "Awilix", "Awilix", "アウィリックス", "아윌릭스"},
        {"bacchus", "Bacchus", "巴克科斯", "Вакх", "Baco", "Baco", "バッカス", "바쿠스"},
        {"bari", "Bari", "大表哥", "Бари", "Bari", "Bari", "バリ", "바리"},
        {"bastet", "Bastet", "猫女", "Бастет", "Bastet", "Bastet", "バステト", "바스테트"},
        {"baron", "Baron Samedi", "星期六男爵", "Барон Самеди", "Baron Samedi", "Baron Samedi", "バロン・サメディ", "바론 사메디"},
        {"baronsamedi", "Baron Samedi", "星期六男爵", "Барон Самеди", "Baron Samedi", "Baron Samedi", "バロン・サメディ", "바론 사메디"},
        {"bellona", "Bellona", "贝罗娜", "Беллона", "Belona", "Belona", "ベローナ", "벨로나"},
        {"cabrakan", "Cabrakan", "卡布拉冈", "Кабракан", "Cabrakán", "Cabrakan", "カブラカン", "카브라칸"},
        {"cerberus", "Cerberus", "刻耳柏洛斯", "Цербер", "Cerbero", "Cérbero", "ケルベロス", "케르베로스"},
        {"cernunnos", "Cernunnos", "赛努诺斯", "Кернунн", "Cernunnos", "Cernunnos", "ケルヌンノス", "케르눈노스"},
        {"chaac", "Chaac", "恰克", "Чаак", "Chaac", "Chaac", "チャーク", "차악"},
        {"chiron", "Chiron", "凯戎", "Хирон", "Quirón", "Quíron", "ケイロン", "케이론"},
        {"cupid", "Cupid", "丘比特", "Купидон", "Cupido", "Cupido", "キューピッド", "큐피드"},
        {"daji", "Da Ji", "妲己", "Да Цзи", "Da Ji", "Da Ji", "妲己", "달기"},
        {"danzaborou", "Danzaburou", "团三郎狸", "Данзабуро", "Danzaburou", "Danzaburou", "団三郎", "단자부로"},
        {"danzaburou", "Danzaburou", "团三郎狸", "Данзабуро", "Danzaburou", "Danzaburou", "団三郎", "단자부로"},
        {"discordia", "Discordia", "狄斯科蒂亚", "Дискордия", "Discordia", "Discórdia", "ディスコルディア", "디스코르디아"},
        {"eset", "Eset", "伊西斯", "Исида", "Eset", "Eset", "エセット", "에셋"},
        {"fenrir", "Fenrir", "芬里尔", "Фенрир", "Fenrir", "Fenrir", "フェンリル", "펜리르"},
        {"freya", "Freya", "芙蕾雅", "Фрейя", "Freya", "Freya", "フレイヤ", "프레이야"},
        {"ganesha", "Ganesha", "伽内什", "Ганеша", "Ganesha", "Ganesha", "ガネーシャ", "가네샤"},
        {"geb", "Geb", "盖布", "Геб", "Geb", "Geb", "ゲブ", "게브"},
        {"gebtalent", "Geb", "盖布", "Геб", "Geb", "Geb", "ゲブ", "게브"},
        {"guanyu", "Guan Yu", "关羽", "Гуань Юй", "Guan Yu", "Guan Yu", "関羽", "관우"},
        {"hades", "Hades", "哈迪斯", "Аид", "Hades", "Hades", "ハデス", "하데스"},
        {"charon", "Charon", "卡戎", "Харон", "Caronte", "Caronte", "カロン", "카론"},
        {"gilgamesh", "Gilgamesh", "吉尔伽美什", "Гильгамеш", "Gilgamesh", "Gilgamesh", "ギルガメッシュ", "길가메시"},
        {"hecate", "Hecate", "赫卡特", "Геката", "Hécate", "Hécate", "ヘカテ", "헤카테"},
        {"hercules", "Hercules", "海格力斯", "Геркулес", "Hércules", "Hércules", "ヘラクレス", "헤라클레스"},
        {"houyi", "Hou Yi", "后羿", "Хоу И", "Hou Yi", "Hou Yi", "后羿", "후예"},
        {"horus", "Horus", "荷鲁斯", "Хорус", "Horus", "Hórus", "ホルス", "호루스"},
        {"huamulan", "Hua Mulan", "花木兰", "Хуа Мулань", "Hua Mulan", "Hua Mulan", "花木蘭", "화목란"},
        {"hunbatz", "Hun Batz", "胡恩・巴茨", "Хун Батц", "Hun Batz", "Hun Batz", "フンバッツ", "훈 바츠"},
        {"ishtar", "Ishtar", "伊什塔尔", "Иштар", "Ishtar", "Ishtar", "イシュタル", "이슈타르"},
        {"izanami", "Izanami", "伊邪那美", "Идзанами", "Izanami", "Izanami", "イザナミ", "이자나미"},
        {"janus", "Janus", "雅努斯", "Янус", "Jano", "Janus", "ヤヌス", "야누스"},
        {"jingwei", "Jing Wei", "精卫", "Цзин Вэй", "Jing Wei", "Jing Wei", "ジンウェイ", "정위"},
        {"jorm", "Jormungandr", "耶梦加得", "Ёрмунганд", "Jormungandr", "Jormungandr", "ヨルムンガンド", "요르문간드"},
        {"kali", "Kali", "迦梨", "Кали", "Kali", "Kali", "カーリー", "칼리"},
        {"khephri", "Khephri", "凯布利", "Хепри", "Khephri", "Khephri", "ケプリ", "케프리"},
        {"khepri", "Khepri", "凯布利", "Хепри", "Khepri", "Khepri", "ケプリ", "케프리"},
        {"kingarthur", "King Arthur", "亚瑟王", "Король Артур", "Rey Arturo", "Rei Arthur", "アーサー王", "아서 왕"},
        {"kukulkan", "Kukulkan", "库库尔坎", "Кукулькан", "Kukulkán", "Kukulkan", "ククルカン", "쿠쿨칸"},
        {"loki", "Loki", "洛基", "Локи", "Loki", "Loki", "ロキ", "로키"},
        {"medusa", "Medusa", "美杜莎", "Медуза", "Medusa", "Medusa", "メデューサ", "메두사"},
        {"merlin", "Merlin", "梅林", "Мерлин", "Merlín", "Merlin", "マーリン", "멀린"},
        {"mercury", "Mercury", "墨丘利", "Меркурий", "Mercurio", "Mercúrio", "マーキュリー", "머큐리"},
        {"mordred", "Mordred", "莫德雷德", "Мордред", "Mordred", "Mordred", "モードレッド", "모드레드"},
        {"morganlefay", "Morgan Le Fay", "摩根勒菲", "Моргана ле Фэй", "Morgana Le Fay", "Morgan Le Fay", "モーガン・ル・フェイ", "모건 르 페이"},
        {"morrigan", "Morrigan", "摩莉甘", "Морриган", "Morrigan", "Morrigan", "モリガン", "모리건"},
        {"mulan", "Hua Mulan", "花木兰", "Хуа Мулань", "Hua Mulan", "Hua Mulan", "花木蘭", "화목란"},
        {"neith", "Neith", "奈斯", "Нейт", "Neith", "Neith", "ネイト", "네이트"},
        {"nemesis", "Nemesis", "涅墨西斯", "Немезида", "Némesis", "Nêmesis", "ネメシス", "네메시스"},
        {"nezha", "Ne Zha", "哪吒", "Нэ Чжа", "Ne Zha", "Ne Zha", "哪吒", "나타"},
        {"nox", "Nox", "诺克斯", "Нокс", "Nox", "Nox", "ノックス", "녹스"},
        {"nuwa", "Nu Wa", "女娲", "Нюй Ва", "Nu Wa", "Nu Wa", "女媧", "여와"},
        {"nut", "Nut", "努特", "Нут", "Nut", "Nut", "ヌト", "누트"},
        {"odin", "Odin", "奥丁", "Один", "Odín", "Odin", "オーディン", "오딘"},
        {"osiris", "Osiris", "奥西里斯", "Осирис", "Osiris", "Osiris", "オシリス", "오시리스"},
        {"pele", "Pele", "裴蕾", "Пеле", "Pele", "Pele", "ペレ", "펠레"},
        {"poseidon", "Poseidon", "波塞冬", "Посейдон", "Poseidón", "Poseidon", "ポセイドン", "포세이돈"},
        {"ra", "Ra", "拉", "Ра", "Ra", "Ra", "ラー", "라"},
        {"rama", "Rama", "罗摩", "Рама", "Rama", "Rama", "ラーマ", "라마"},
        {"ratatoskr", "Ratatoskr", "拉塔托斯克", "Рататоскр", "Ratatoskr", "Ratatoskr", "ラタトスク", "라타토스크"},
        {"scylla", "Scylla", "斯库拉", "Сцилла", "Escila", "Scylla", "スキュラ", "스킬라"},
        {"scyllaprimaryskin01", "Scylla", "斯库拉", "Сцилла", "Escila", "Scylla", "スキュラ", "스킬라"},
        {"sobek", "Sobek", "索贝克", "Собек", "Sobek", "Sobek", "ソベク", "소벡"},
        {"sol", "Sol", "索尔", "Соль", "Sol", "Sol", "ソル", "솔"},
        {"sunwukong", "Sun Wukong", "孙悟空", "Сунь Укун", "Sun Wukong", "Sun Wukong", "孫悟空", "손오공"},
        {"susano", "Susano", "须佐之男", "Сусано", "Susano", "Susano", "スサノオ", "스사노오"},
        {"susanoo", "Susano", "须佐之男", "Сусано", "Susano", "Susano", "スサノオ", "스사노오"},
        {"sylvanus", "Sylvanus", "西尔瓦努斯", "Сильван", "Sylvanus", "Sylvanus", "シルヴァヌス", "실바누스"},
        {"thanatos", "Thanatos", "塔纳托斯", "Танатос", "Thanatos", "Thanatos", "タナトス", "타나토스"},
        {"thor", "Thor", "托尔", "Тор", "Thor", "Thor", "トール", "토르"},
        {"tsukuyomi", "Tsukuyomi", "月读", "Цукуёми", "Tsukuyomi", "Tsukuyomi", "ツクヨミ", "츠쿠요미"},
        {"tyr", "Tyr", "提尔", "Тюр", "Tyr", "Tyr", "テュール", "티르"},
        {"ullr", "Ullr", "吴老二", "Улль", "Ullr", "Ullr", "ウル", "울르"},
        {"vulcan", "Vulcan", "伏尔甘", "Вулкан", "Vulcano", "Vulcano", "バルカン", "불칸"},
        {"xbalanque", "Xbalanque", "希巴兰克", "Шбаланке", "Xbalanqué", "Xbalanque", "シバランケ", "슈발랑케"},
        {"yemoja", "Yemoja", "耶莫贾", "Йемоджа", "Yemoja", "Yemoja", "イエモジャ", "예모자"},
        {"ymir", "Ymir", "尤弥尔", "Имир", "Ymir", "Ymir", "ユミル", "이미르"},
        {"zeus", "Zeus", "宙斯", "Зевс", "Zeus", "Zeus", "ゼウス", "제우스"},
    };

    const std::string key = CanonicalGodKey(name);
    const GodNameEntry* matched = nullptr;
    for (const GodNameEntry& entry : names) {
        if (key == entry.key) {
            matched = &entry;
            break;
        }
    }
    if (!matched) {
        size_t bestLen = 0;
        for (const GodNameEntry& entry : names) {
            const size_t len = strlen(entry.key);
            if (len < 4) continue;
            if (key.find(entry.key) != std::string::npos && len > bestLen) {
                matched = &entry;
                bestLen = len;
            }
        }
    }
    if (matched) {
        switch (render_ui::current_language_index()) {
        case 0: return matched->zh;
        case 2: return matched->ru;
        case 3: return matched->es;
        case 4: return matched->pt;
        case 5: return matched->ja;
        case 6: return matched->ko;
        case 1:
        default:
            return matched->en;
        }
    }
    return name;
}

// ---------------------------------------------------------------------------
// -------------------- DX12 render targets --------------------
static void WaitForDX12Idle()
{
    ID3D12CommandQueue* commandQueue = AtomicReadCommandQueue();
    if (!commandQueue || !g_frameFence || !g_frameFenceEvent)
        return;

    const UINT64 fenceValue = g_nextFenceValue++;
    if (FAILED(commandQueue->Signal(g_frameFence, fenceValue)))
        return;

    if (g_frameFence->GetCompletedValue() < fenceValue) {
        if (SUCCEEDED(g_frameFence->SetEventOnCompletion(fenceValue, g_frameFenceEvent))) {
            GetDefaultBackoffScheduler().Wait();
            WaitForSingleObject(g_frameFenceEvent, 25);
        }
    }
}

static void DestroyDX12RenderTargets()
{
    WaitForDX12Idle();
    SafeRelease(g_commandList);
    for (Dx12FrameContext& frame : g_frameContexts) {
        SafeRelease(frame.renderTarget);
        SafeRelease(frame.commandAllocator);
        frame.rtv = {};
    }
    g_frameContexts.clear();
    SafeRelease(g_rtvHeap);
    g_rtvDescriptorSize = 0;
    g_bufferCount = 0;
}

static UINT GetCurrentBackBufferIndex(IDXGISwapChain* sc)
{
    IDXGISwapChain3* sc3 = nullptr;
    UINT index = 0;
    if (sc && SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc3))) && sc3) {
        index = sc3->GetCurrentBackBufferIndex();
        SafeRelease(sc3);
    }
    return index;
}

static bool CreateOrRecreateRTV(IDXGISwapChain* sc)
{
    if (!sc || !g_device)
        return false;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(sc->GetDesc(&desc)))
        return false;

    UINT bufferCount = desc.BufferCount;
    if (bufferCount == 0)
        bufferCount = 2;

    DXGI_FORMAT format = desc.BufferDesc.Format;
    if (format == DXGI_FORMAT_UNKNOWN)
        format = DXGI_FORMAT_R8G8B8A8_UNORM;

    DestroyDX12RenderTargets();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = bufferCount;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    if (FAILED(g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap))))
        return false;

    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    g_frameContexts.resize(bufferCount);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < bufferCount; ++i) {
        Dx12FrameContext& frame = g_frameContexts[i];
        if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.commandAllocator))))
            return false;
        if (FAILED(sc->GetBuffer(i, IID_PPV_ARGS(&frame.renderTarget))))
            return false;

        frame.rtv = rtvHandle;
        g_device->CreateRenderTargetView(frame.renderTarget, nullptr, frame.rtv);
        rtvHandle.ptr += g_rtvDescriptorSize;
    }

    if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContexts[0].commandAllocator, nullptr, IID_PPV_ARGS(&g_commandList))))
        return false;
    g_commandList->Close();

    if (!g_frameFence && FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_frameFence))))
        return false;
    if (!g_frameFenceEvent)
        g_frameFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_frameFenceEvent)
        return false;

    g_bufferCount = bufferCount;
    g_backBufferFormat = format;
    g_needRTVRecreate = false;
    return true;
}

// -------------------- Dummy window for swapchain bootstrap --------------------
struct DummyWindow {
    HWND hwnd = nullptr;
    WNDCLASSEXW wc{};
    DummyWindow() {
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW, 0L, 0L, hInst,
               nullptr, nullptr, nullptr, nullptr, L"DummyDX12Wnd", nullptr };
        RegisterClassExW(&wc);
        hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    }
    ~DummyWindow() {
        if (hwnd) DestroyWindow(hwnd);
        if (wc.lpszClassName) UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }
};

// -------------------- Bootstrap thread: install Present hook --------------------

static DWORD WINAPI BootstrapThread(LPVOID) {
    Dx12DebugLog("Bootstrap thread started");
    for (int i = 0; i < 200 && (!GetModuleHandleW(L"d3d12.dll") || !GetModuleHandleW(L"dxgi.dll")); ++i) {
        GetDefaultBackoffScheduler().Wait();
    }
    GetDefaultBackoffScheduler().Wait();

    MH_STATUS mhStatus = MH_Initialize();
    if (mhStatus != MH_OK && mhStatus != MH_ERROR_ALREADY_INITIALIZED)
        return 0;
    const bool matchEndpointHooksInstalled = InstallMatchEndpointHooks();
    Dx12DebugLog("Match endpoint capture hooks installed=%d", matchEndpointHooksInstalled ? 1 : 0);
#if 1 // WARP device creation for vtable hook discovery

    DummyWindow dummy;

    IDXGIFactory4* factory = nullptr;
    ID3D12Device* pDevice = nullptr;
    ID3D12CommandQueue* pQueue = nullptr;
    IDXGIAdapter* warpAdapter = nullptr;
    IDXGISwapChain1* pSwapChain1 = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;

    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr) && factory && SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)))) {
        hr = D3D12CreateDevice(warpAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice));
    } else if (SUCCEEDED(hr)) {
        hr = E_FAIL;
    }

    if (FAILED(hr) || !factory || !pDevice) {
        Dx12DebugLog("Bootstrap failed creating WARP D3D12 device hr=0x%08X", static_cast<unsigned>(hr));
        SafeRelease(warpAdapter);
        SafeRelease(factory);
        MH_Uninitialize();
        return 0;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    if (FAILED(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pQueue))) || !pQueue) {
        Dx12DebugLog("Bootstrap failed creating WARP command queue");
        SafeRelease(warpAdapter);
        SafeRelease(pDevice);
        SafeRelease(factory);
        MH_Uninitialize();
        return 0;
    }

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 100;
    sd.Height = 100;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Stereo = FALSE;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    hr = factory->CreateSwapChainForHwnd(pQueue, dummy.hwnd, &sd, nullptr, nullptr, &pSwapChain1);
    if (SUCCEEDED(hr) && pSwapChain1)
        hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&pSwapChain));

    if (FAILED(hr) || !pSwapChain) {
        Dx12DebugLog("Bootstrap failed creating WARP dummy swapchain hr=0x%08X", static_cast<unsigned>(hr));
        SafeRelease(pSwapChain1);
        SafeRelease(warpAdapter);
        SafeRelease(pQueue);
        SafeRelease(pDevice);
        SafeRelease(factory);
        MH_Uninitialize();
        return 0;
    }

    void** vTable = *reinterpret_cast<void***>(pSwapChain);
    void* presentAddr = vTable[8];
    void** queueVTable = *reinterpret_cast<void***>(pQueue);
    void* executeCommandListsAddr = queueVTable[10];
    void** factoryVTable = *reinterpret_cast<void***>(factory);
    void* createSwapChainAddr = factoryVTable[10];
    void* createSwapChainForHwndAddr = factoryVTable[15];
    void* createSwapChainForCoreWindowAddr = factoryVTable[16];
    void* createSwapChainForCompositionAddr = factoryVTable[24];

    SafeRelease(pSwapChain);
    SafeRelease(pSwapChain1);
    SafeRelease(warpAdapter);
    SafeRelease(pQueue);
    SafeRelease(pDevice);
    SafeRelease(factory);

    if (!presentAddr || !createSwapChainAddr || !createSwapChainForHwndAddr) { MH_Uninitialize(); return 0; }
#endif // WARP vtable discovery

#if 0
    // Dummy addrs - hooks disabled
    void* presentAddr = nullptr;
    void* createSwapChainAddr = nullptr;
    void* createSwapChainForHwndAddr = nullptr;
    void* createSwapChainForCoreWindowAddr = nullptr;
    void* createSwapChainForCompositionAddr = nullptr;
    void* executeCommandListsAddr = nullptr;
#endif

#if 1 // DXGI hooks enabled


    if (MH_CreateHook(presentAddr, &hkPresent, reinterpret_cast<void**>(&g_originalPresent)) != MH_OK ||
        MH_CreateHook(createSwapChainAddr, &hkCreateSwapChain, reinterpret_cast<void**>(&g_originalCreateSwapChain)) != MH_OK ||
        MH_CreateHook(createSwapChainForHwndAddr, &hkCreateSwapChainForHwnd, reinterpret_cast<void**>(&g_originalCreateSwapChainForHwnd)) != MH_OK) {
        Dx12DebugLog("Bootstrap failed creating core DXGI hooks");
        MH_Uninitialize();
        return 0;
    }

    if (createSwapChainForCoreWindowAddr) {
        MH_CreateHook(createSwapChainForCoreWindowAddr, &hkCreateSwapChainForCoreWindow, reinterpret_cast<void**>(&g_originalCreateSwapChainForCoreWindow));
    }
    if (createSwapChainForCompositionAddr) {
        MH_CreateHook(createSwapChainForCompositionAddr, &hkCreateSwapChainForComposition, reinterpret_cast<void**>(&g_originalCreateSwapChainForComposition));
    }
    if (executeCommandListsAddr) {
        MH_CreateHook(executeCommandListsAddr, &hkExecuteCommandLists, reinterpret_cast<void**>(&g_originalExecuteCommandLists));
    }

    if (MH_EnableHook(presentAddr) != MH_OK ||
        MH_EnableHook(createSwapChainAddr) != MH_OK ||
        MH_EnableHook(createSwapChainForHwndAddr) != MH_OK) {
        Dx12DebugLog("Bootstrap failed enabling core DXGI hooks");
        MH_Uninitialize();
        return 0;
    }
    if (createSwapChainForCoreWindowAddr) MH_EnableHook(createSwapChainForCoreWindowAddr);
    if (createSwapChainForCompositionAddr) MH_EnableHook(createSwapChainForCompositionAddr);
    if (executeCommandListsAddr && g_originalExecuteCommandLists) MH_EnableHook(executeCommandListsAddr);
#endif // DXGI hooks enabled


    Dx12DebugLog("DX12 Present/DXGI factory hooks installed present=%p", presentAddr);

    return 0;
}

static void __stdcall hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT numCommandLists, ID3D12CommandList* const* commandLists)
{
    if (g_originalExecuteCommandLists)
        g_originalExecuteCommandLists(queue, numCommandLists, commandLists);

    if (!AtomicReadCommandQueue() && g_device && queue && QueueBelongsToDevice(queue, g_device)) {
        if (TryCaptureDX12CommandQueue(queue)) {
            Dx12DebugLog("Captured DX12 command queue from ExecuteCommandLists fallback: %p", queue);
        }
    }
}

static HRESULT __stdcall hkCreateSwapChain(IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swapChain)
{
    const HRESULT hr = g_originalCreateSwapChain
        ? g_originalCreateSwapChain(factory, device, desc, swapChain)
        : E_FAIL;
    if (SUCCEEDED(hr) && swapChain && *swapChain) {
        TryCaptureDX12CommandQueueFromUnknown(device);
        Dx12DebugLog("CreateSwapChain captured swapchain=%p hr=0x%08X", *swapChain, static_cast<unsigned>(hr));
    }
    return hr;
}

static HRESULT __stdcall hkCreateSwapChainForHwnd(IDXGIFactory2* factory, IUnknown* device, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc, IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
{
    const HRESULT hr = g_originalCreateSwapChainForHwnd
        ? g_originalCreateSwapChainForHwnd(factory, device, hwnd, desc, fullscreenDesc, restrictToOutput, swapChain)
        : E_FAIL;
    if (SUCCEEDED(hr) && swapChain && *swapChain) {
        TryCaptureDX12CommandQueueFromUnknown(device);
        Dx12DebugLog("CreateSwapChainForHwnd captured swapchain=%p hwnd=%p hr=0x%08X",
            *swapChain, hwnd, static_cast<unsigned>(hr));
    }
    return hr;
}

static HRESULT __stdcall hkCreateSwapChainForCoreWindow(IDXGIFactory2* factory, IUnknown* device, IUnknown* window, const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
{
    const HRESULT hr = g_originalCreateSwapChainForCoreWindow
        ? g_originalCreateSwapChainForCoreWindow(factory, device, window, desc, restrictToOutput, swapChain)
        : E_FAIL;
    if (SUCCEEDED(hr) && swapChain && *swapChain) {
        TryCaptureDX12CommandQueueFromUnknown(device);
        Dx12DebugLog("CreateSwapChainForCoreWindow captured swapchain=%p hr=0x%08X",
            *swapChain, static_cast<unsigned>(hr));
    }
    return hr;
}

static HRESULT __stdcall hkCreateSwapChainForComposition(IDXGIFactory2* factory, IUnknown* device, const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
{
    const HRESULT hr = g_originalCreateSwapChainForComposition
        ? g_originalCreateSwapChainForComposition(factory, device, desc, restrictToOutput, swapChain)
        : E_FAIL;
    if (SUCCEEDED(hr) && swapChain && *swapChain) {
        TryCaptureDX12CommandQueueFromUnknown(device);
        Dx12DebugLog("CreateSwapChainForComposition captured swapchain=%p hr=0x%08X",
            *swapChain, static_cast<unsigned>(hr));
    }
    return hr;
}

// -------------------- ImGui init --------------------
static void InitializeImGui(IDXGISwapChain* pSwapChain)
{
    if (g_imguiInitialized) return;
    if (!pSwapChain) return;

    static bool s_loggedDeviceFailure = false;
    if (!g_device && FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_device)))) {
        if (!s_loggedDeviceFailure) {
            Dx12DebugLog("InitializeImGui waiting: swapchain did not expose an ID3D12Device");
            s_loggedDeviceFailure = true;
        }
        return;
    }

    ID3D12CommandQueue* capturedQueue = AtomicReadCommandQueue();
    static bool s_loggedNoQueue = false;
    if (!capturedQueue) {
        if (!s_loggedNoQueue) {
            Dx12DebugLog("InitializeImGui waiting: no DX12 command queue captured yet");
            s_loggedNoQueue = true;
        }
        return;
    }

    if (!QueueBelongsToDevice(capturedQueue, g_device)) {
        Dx12DebugLog("InitializeImGui rejected command queue from a different device");
        ReleaseCapturedCommandQueue();
        return;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    if (FAILED(pSwapChain->GetDesc(&sd)))
        return;
    if (!sd.OutputWindow || sd.BufferDesc.Width == 0 || sd.BufferDesc.Height == 0)
        return;
    g_hWnd = sd.OutputWindow;

    SafeRelease(g_srvHeap);
    memset(g_srvDescriptorUsed, 0, sizeof(g_srvDescriptorUsed));
    g_srvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = kDx12SrvDescriptorCount;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NodeMask = 0;
    if (FAILED(g_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_srvHeap))))
        return;

    if (!CreateOrRecreateRTV(pSwapChain))
        return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Apply the default dark theme then override style settings to match our custom palette.
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    // General spacing and rounding
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 4.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.FramePadding = ImVec2(10.0f, 4.0f);
    style.WindowPadding = ImVec2(14.0f, 14.0f);
    // Apply our palette to key colours
    style.Colors[ImGuiCol_WindowBg] = g_colorBackground;
    style.Colors[ImGuiCol_ChildBg] = g_colorPanel;
    style.Colors[ImGuiCol_PopupBg] = g_colorPanel;
    style.Colors[ImGuiCol_Header] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.25f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.35f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.55f);
    style.Colors[ImGuiCol_Button] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.35f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.45f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.65f);
    style.Colors[ImGuiCol_FrameBg] = g_colorPanel;
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.25f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.45f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.50f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.70f);
    style.Colors[ImGuiCol_CheckMark] = g_colorAccent;
    style.Colors[ImGuiCol_TitleBg] = g_colorBackground;
    style.Colors[ImGuiCol_TitleBgActive] = g_colorBackground;
    style.Colors[ImGuiCol_TitleBgCollapsed] = g_colorBackground;
    style.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
    // Tab colours remain defined in case nested tabs are used.
    style.Colors[ImGuiCol_Tab] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.30f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.40f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(g_colorAccent.x, g_colorAccent.y, g_colorAccent.z, 0.60f);
    style.Colors[ImGuiCol_TabUnfocused] = style.Colors[ImGuiCol_Tab];
    style.Colors[ImGuiCol_TabUnfocusedActive] = style.Colors[ImGuiCol_Tab];

    // Initialise the retained theme and font system.
    odyssey::theme::LoadFonts();
    odyssey::theme::SetupStyle();
    {
        const auto& odyFonts = odyssey::theme::GetFonts();
        const auto& odyPalette = odyssey::theme::GetPalette();
        ImGuiIO& io2 = ImGui::GetIO();
        poppins = odyFonts.ui ? odyFonts.ui : io2.FontDefault;
        tab_title = odyFonts.title ? odyFonts.title : poppins;
        font_icon = odyFonts.uiSmall ? odyFonts.uiSmall : poppins;
        accent_colour[0] = odyPalette.accent.x;
        accent_colour[1] = odyPalette.accent.y;
        accent_colour[2] = odyPalette.accent.z;
        accent_colour[3] = odyPalette.accent.w;
        content_animation = 1.0f;
    }
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.Device = g_device;
    initInfo.CommandQueue = capturedQueue;
    initInfo.NumFramesInFlight = static_cast<int>(g_bufferCount ? g_bufferCount : 2);
    initInfo.RTVFormat = g_backBufferFormat;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = g_srvHeap;
    initInfo.SrvDescriptorAllocFn = Dx12SrvDescriptorAlloc;
    initInfo.SrvDescriptorFreeFn = Dx12SrvDescriptorFree;
    if (!ImGui_ImplDX12_Init(&initInfo)) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        DestroyDX12RenderTargets();
        SafeRelease(g_srvHeap);
        return;
    }

    g_originalWndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
    InstallGameRuntimeTimer(g_hWnd);


    // Offset status initialisation removed.
    // Ability scanning and tag dumping removed.

    g_imguiInitialized = true;
    Dx12DebugLog("InitializeImGui completed hwnd=%p buffers=%u format=%u",
        g_hWnd, g_bufferCount, static_cast<unsigned>(g_backBufferFormat));
}

static bool SafeObjectIsA(SDK::UObject* object, SDK::UClass* objectClass)
{
    __try {
        return object && objectClass && object->IsA(objectClass);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

namespace
{
    struct LoadingRankCardState
    {
        std::string originalTitle;
        SDK::ESlateVisibility originalTitleVisibility = SDK::ESlateVisibility::Visible;
        SDK::ESlateVisibility originalBackgroundVisibility = SDK::ESlateVisibility::Visible;
        SDK::URH_PlayerInfo* ratingPlayerInfo = nullptr;
        int stableSkillRating = 0;
        int stableWinRatePercent = -1;
        int stableWinRateMatches = 0;
        bool captured = false;
        bool applied = false;
    };

    struct LoadingRankInventoryRequestState
    {
        ULONGLONG nextAttemptMs = 0;
        int attempts = 0;
    };

    struct LoadingRankRuntimeContext
    {
        SDK::UHWSkillRatingRankings* rankings = nullptr;
        SDK::UHWRankedConfig* config = nullptr;
        SDK::UHWRankedSubsystem* subsystem = nullptr;
        SDK::URH_PlayerInfoSubsystem* playerInfoSubsystem = nullptr;
        SDK::UDataTable* queueRankTypes = nullptr;
    };

    struct LoadingRankValue
    {
        int skillRating = 0;
        std::wstring rankId;
    };

    struct LoadingWinRateValue
    {
        int wins = 0;
        int matches = 0;
        int percent = -1;
        bool fromRankingData = false;
    };

    struct DraftPlayerStatsState
    {
        SDK::URH_PlayerInfo* playerInfo = nullptr;
        int stableSkillRating = 0;
        int stableWinRatePercent = -1;
        int stableWinRateMatches = 0;
        std::wstring stableDisplayName;
    };

    struct DraftPlayerStatsOverlayItem
    {
        ImVec2 topLeft{};
        ImVec2 size{};
        std::string playerName;
        std::string stats;
    };

    static std::mutex g_draftPlayerStatsOverlayMutex;
    static std::vector<DraftPlayerStatsOverlayItem> g_draftPlayerStatsOverlay;

    static std::wstring LoadingRankSafeText(const SDK::FText& text)
    {
        if (text.TextData)
            return text.GetStringRef().ToWString();
        return {};
    }

    static SDK::FText LoadingRankGetWidgetText(SDK::UHWTextBlock* textBlock)
    {
        SDK::Params::TextBlock_GetText params{};
        if (!textBlock || !textBlock->Class)
            return params.ReturnValue;
        static SDK::UFunction* function = nullptr;
        if (!function)
            function = textBlock->Class->GetFunction("TextBlock", "GetText");
        if (!function)
            return params.ReturnValue;
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        textBlock->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return params.ReturnValue;
    }

    static SDK::FText LoadingRankGetRichText(SDK::UHWRichTextBlock* textBlock)
    {
        SDK::Params::RichTextBlock_GetText params{};
        if (!textBlock || !textBlock->Class)
            return params.ReturnValue;
        static SDK::UFunction* function = nullptr;
        if (!function)
            function = textBlock->Class->GetFunction("RichTextBlock", "GetText");
        if (!function)
            return params.ReturnValue;
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        textBlock->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return params.ReturnValue;
    }

    static SDK::ESlateVisibility LoadingRankGetWidgetVisibility(SDK::UWidget* widget)
    {
        if (!widget || !widget->Class)
            return SDK::ESlateVisibility::Collapsed;
        static SDK::UFunction* function = nullptr;
        if (!function)
            function = widget->Class->GetFunction("Widget", "GetVisibility");
        if (!function)
            return SDK::ESlateVisibility::Collapsed;
        SDK::Params::Widget_GetVisibility params{};
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        widget->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return params.ReturnValue;
    }

    static void LoadingRankSetWidgetVisibility(SDK::UWidget* widget, SDK::ESlateVisibility visibility)
    {
        if (!widget || !widget->Class)
            return;
        static SDK::UFunction* function = nullptr;
        if (!function)
            function = widget->Class->GetFunction("Widget", "SetVisibility");
        if (!function)
            return;
        SDK::Params::Widget_SetVisibility params{};
        params.InVisibility = visibility;
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        widget->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
    }

    static bool LoadingRankIsWidgetRendered(SDK::UWidget* widget)
    {
        if (!widget || !widget->Class)
            return false;
        static SDK::UFunction* function = nullptr;
        if (!function)
            function = widget->Class->GetFunction("Widget", "IsRendered");
        if (!function)
            return false;
        SDK::Params::Widget_IsRendered params{};
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        widget->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return params.ReturnValue;
    }

    static bool LoadingRankContainsInsensitive(const std::wstring& value, const std::wstring& needle)
    {
        if (value.empty() || needle.empty())
            return false;
        std::wstring valueLower = value;
        std::wstring needleLower = needle;
        const auto asciiLower = [](wchar_t character) {
            return character >= L'A' && character <= L'Z'
                ? static_cast<wchar_t>(character + (L'a' - L'A'))
                : character;
        };
        std::transform(valueLower.begin(), valueLower.end(), valueLower.begin(), asciiLower);
        std::transform(needleLower.begin(), needleLower.end(), needleLower.begin(), asciiLower);
        return valueLower.find(needleLower) != std::wstring::npos;
    }

    static bool LoadingRankGuidEquals(const SDK::FGuid& left, const SDK::FGuid& right)
    {
        return left.A == right.A && left.B == right.B && left.C == right.C && left.D == right.D;
    }

    static SDK::URH_PlayerInfo* LoadingRankFindPlayerInfo(
        SDK::URH_PlayerInfoSubsystem* subsystem,
        const SDK::FGuid& playerUuid)
    {
        if (!subsystem || !subsystem->PlayerInfos.IsValid() || subsystem->PlayerInfos.Num() <= 0 ||
            subsystem->PlayerInfos.Num() > 256)
            return nullptr;

        for (auto& entry : subsystem->PlayerInfos) {
            if (!LoadingRankGuidEquals(entry.Key(), playerUuid))
                continue;
            SDK::URH_PlayerInfo* playerInfo = entry.Value();
            return SafeObjectIsA(playerInfo, SDK::URH_PlayerInfo::StaticClass()) ? playerInfo : nullptr;
        }
        return nullptr;
    }

    static bool LoadingRankRequestSkillRatingInventory(
        SDK::URH_PlayerInfo* playerInfo,
        const LoadingRankRuntimeContext& context)
    {
        if (!playerInfo || !context.config || context.config->SkillRatingItemId.LegacyId <= 0)
            return false;

        SDK::URH_PlayerInventory* inventory = playerInfo->PlayerInventory;
        if (!SafeObjectIsA(inventory, SDK::URH_PlayerInventory::StaticClass()))
            return false;

        static SDK::UFunction* function = nullptr;
        if (!function)
            function = inventory->Class->GetFunction("RH_PlayerInventory", "BLUEPRINT_GetInventory");
        if (!function)
            return false;

        int32_t skillRatingItemId = context.config->SkillRatingItemId.LegacyId;
        SDK::Params::RH_PlayerInventory_BLUEPRINT_GetInventory params{};
        params.bForce = true;
        params.FilterInventoryItemIds = SDK::TArray<int32_t>(&skillRatingItemId, 1, 1);
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        inventory->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return true;
    }

    static bool LoadingRankTryParseNonNegativeCount(const SDK::FString& value, int& result)
    {
        const std::wstring text = value.ToWString();
        if (text.empty())
            return false;

        wchar_t* end = nullptr;
        const long long parsed = std::wcstoll(text.c_str(), &end, 10);
        while (end && *end && iswspace(*end))
            ++end;
        if (!end || end == text.c_str() || *end != L'\0' || parsed < 0 || parsed > 10000000)
            return false;
        result = static_cast<int>(parsed);
        return true;
    }

    static std::wstring LoadingRankNormalizeStatKey(const SDK::FString& key)
    {
        std::wstring normalized;
        for (wchar_t character : key.ToWString()) {
            if (character >= L'A' && character <= L'Z')
                character = static_cast<wchar_t>(character + (L'a' - L'A'));
            if ((character >= L'a' && character <= L'z') ||
                (character >= L'0' && character <= L'9')) {
                normalized.push_back(character);
            }
        }
        return normalized;
    }

    static LoadingWinRateValue LoadingRankGetWinRateFromRankingData(SDK::URH_PlayerInfo* playerInfo)
    {
        LoadingWinRateValue best{};
        if (!playerInfo || !playerInfo->PlayerRankingsByRankingId.IsValid() ||
            playerInfo->PlayerRankingsByRankingId.Num() <= 0 ||
            playerInfo->PlayerRankingsByRankingId.Num() > 128) {
            return best;
        }

        for (auto& rankingEntry : playerInfo->PlayerRankingsByRankingId) {
            const SDK::FRHAPI_RankRankData& rank = rankingEntry.Value().Rank;
            if (!rank.CustomData_IsSet || !rank.CustomData_Optional.IsValid() ||
                rank.CustomData_Optional.Num() <= 0 || rank.CustomData_Optional.Num() > 128) {
                continue;
            }

            int wins = -1;
            int losses = -1;
            int matches = -1;
            for (auto& customEntry : rank.CustomData_Optional) {
                int value = 0;
                if (!LoadingRankTryParseNonNegativeCount(customEntry.Value(), value))
                    continue;
                const std::wstring key = LoadingRankNormalizeStatKey(customEntry.Key());
                if (key == L"wins" || key == L"wincount" || key == L"totalwins" ||
                    key == L"numwins" || key == L"gameswon" || key == L"matcheswon" ||
                    key == L"victories") {
                    wins = value;
                }
                else if (key == L"losses" || key == L"losscount" || key == L"totallosses" ||
                    key == L"numlosses" || key == L"gameslost" || key == L"matcheslost" ||
                    key == L"defeats") {
                    losses = value;
                }
                else if (key == L"matches" || key == L"matchcount" || key == L"matchesplayed" ||
                    key == L"totalmatches" || key == L"nummatches" || key == L"games" ||
                    key == L"gamecount" || key == L"gamesplayed" || key == L"totalgames") {
                    matches = value;
                }
            }

            if (matches < 0 && wins >= 0 && losses >= 0)
                matches = wins + losses;
            if (wins < 0 || matches <= 0 || wins > matches)
                continue;
            if (best.percent >= 0 && matches <= best.matches)
                continue;

            best.wins = wins;
            best.matches = matches;
            best.percent = static_cast<int>(std::lround(
                (static_cast<double>(wins) * 100.0) / static_cast<double>(matches)));
            best.fromRankingData = true;
        }
        return best;
    }

    static LoadingWinRateValue LoadingRankGetWinRate(SDK::URH_PlayerInfo* playerInfo)
    {
        LoadingWinRateValue result{};
        if (!playerInfo)
            return result;

        SDK::URH_PlayerMatches* matches = playerInfo->PlayerMatches;
        if (SafeObjectIsA(matches, SDK::URH_PlayerMatches::StaticClass()) &&
            matches->Matches.IsValid() && matches->Matches.Num() > 0 && matches->Matches.Num() <= 1000) {
            for (auto& entry : matches->Matches) {
                const SDK::FRHAPI_MatchPlayerWithMatch& match = entry.Value();
                if (!match.Placement_IsSet || match.Placement_IsNull || match.Placement_Optional <= 0)
                    continue;
                ++result.matches;
                if (match.Placement_Optional == 1)
                    ++result.wins;
            }
        }

        if (result.matches > 0) {
            result.percent = static_cast<int>(std::lround(
                (static_cast<double>(result.wins) * 100.0) / static_cast<double>(result.matches)));
            return result;
        }
        return LoadingRankGetWinRateFromRankingData(playerInfo);
    }

    static bool LoadingRankRequestRankingData(SDK::URH_PlayerInfo* playerInfo)
    {
        if (!SafeObjectIsA(playerInfo, SDK::URH_PlayerInfo::StaticClass()))
            return false;

        static SDK::UFunction* function = nullptr;
        if (!function)
            function = playerInfo->Class->GetFunction("RH_PlayerInfo", "BLUEPRINT_GetPlayerRankings");
        if (!function)
            return false;

        SDK::Params::RH_PlayerInfo_BLUEPRINT_GetPlayerRankings params{};
        params.bForceRefresh = true;
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        playerInfo->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return true;
    }

    static bool LoadingRankRequestMatchHistory(SDK::URH_PlayerInfo* playerInfo)
    {
        if (!playerInfo)
            return false;
        SDK::URH_PlayerMatches* matches = playerInfo->PlayerMatches;
        if (!SafeObjectIsA(matches, SDK::URH_PlayerMatches::StaticClass()))
            return false;

        static SDK::UFunction* function = nullptr;
        if (!function)
            function = SDK::URH_PlayerInfoSubobject::StaticClass()->GetFunction(
                "RH_PlayerInfoSubobject", "BLUEPRINT_RequestUpdate");
        if (!function)
            return false;

        SDK::Params::RH_PlayerInfoSubobject_BLUEPRINT_RequestUpdate params{};
        params.bForceUpdate = true;
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        matches->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return true;
    }

    static bool LoadingRankIsPlaceholderDisplayName(const std::wstring& name)
    {
        if (name.empty())
            return true;
        std::wstring normalized = name;
        while (!normalized.empty() && iswspace(normalized.front()))
            normalized.erase(normalized.begin());
        while (!normalized.empty() && iswspace(normalized.back()))
            normalized.pop_back();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](wchar_t value) {
            return static_cast<wchar_t>(towlower(value));
        });
        if (normalized.empty() || normalized == L"player" || normalized == L"unknown" ||
            normalized == L"\x73a9\x5bb6")
            return true;
        return normalized.rfind(L"enemy ", 0) == 0 || normalized.rfind(L"\x654c\x65b9", 0) == 0;
    }

    static bool LoadingRankObjectOwnedBy(SDK::UObject* object, SDK::UObject* owner)
    {
        for (int depth = 0; object && depth < 16; ++depth, object = object->Outer) {
            if (object == owner)
                return true;
        }
        return false;
    }

    static std::wstring LoadingRankGetDraftDisplayedName(
        SDK::UHWDraftLobbyPlayerEntryWidget* entry,
        SDK::AHWPlayerState* playerState,
        SDK::URH_PlayerInfo* playerInfo,
        const std::vector<SDK::UWBP_S_PlayerNameDisplay_C*>& nameDisplays)
    {
        auto readDisplay = [](SDK::UWBP_S_PlayerNameDisplay_C* display) -> std::wstring {
            if (!display || !display->PlayerName)
                return {};
            const SDK::FText text = LoadingRankGetRichText(display->PlayerName);
            std::wstring value = LoadingRankSafeText(text);
            return LoadingRankIsPlaceholderDisplayName(value) ? std::wstring{} : value;
        };

        // The widget owned by this draft entry is the most reliable source: the
        // stock UI may know the account name even when PlayerState says "Player".
        for (SDK::UWBP_S_PlayerNameDisplay_C* display : nameDisplays) {
            if (LoadingRankObjectOwnedBy(display, entry)) {
                std::wstring value = readDisplay(display);
                if (!value.empty())
                    return value;
            }
        }
        for (SDK::UWBP_S_PlayerNameDisplay_C* display : nameDisplays) {
            if ((playerState && display->MyPlayerState == playerState) ||
                (playerInfo && display->MyPlayerInfo == playerInfo)) {
                std::wstring value = readDisplay(display);
                if (!value.empty())
                    return value;
            }
        }
        return {};
    }

    static std::wstring LoadingRankGetCachedDisplayName(
        SDK::AHWPlayerState* playerState,
        SDK::URH_PlayerInfo* playerInfo)
    {
        if (playerState) {
            const SDK::FString playerName = playerState->GetPlayerName();
            const std::string utf8Name = playerName.ToString();
            const std::wstring wideName = TranslationUtf8ToWide(utf8Name);
            if (!LoadingRankIsPlaceholderDisplayName(wideName))
                return wideName;
        }

        if (!playerInfo)
            return {};

        static SDK::UFunction* function = nullptr;
        if (!function)
            function = SDK::URH_PlayerInfo::StaticClass()->GetFunction(
                "RH_PlayerInfo", "BLUEPRINT_GetLastKnownDisplayName");
        if (!function)
            return {};

        constexpr SDK::ERHAPI_Platform preferredPlatforms[] = {
            SDK::ERHAPI_Platform::Rallyhere,
            SDK::ERHAPI_Platform::Legacyname,
            SDK::ERHAPI_Platform::Steam,
            SDK::ERHAPI_Platform::Epic,
            SDK::ERHAPI_Platform::XboxLive,
            SDK::ERHAPI_Platform::Psn
        };
        for (SDK::ERHAPI_Platform platform : preferredPlatforms) {
            SDK::Params::RH_PlayerInfo_BLUEPRINT_GetLastKnownDisplayName params{};
            params.PreferredPlatformType = platform;
            const auto flags = function->FunctionFlags;
            function->FunctionFlags |= 0x400;
            playerInfo->ProcessEvent(function, &params);
            function->FunctionFlags = flags;
            if (params.ReturnValue) {
                const std::string utf8Name = params.OutDisplayName.ToString();
                const std::wstring wideName = TranslationUtf8ToWide(utf8Name);
                if (!LoadingRankIsPlaceholderDisplayName(wideName))
                    return wideName;
            }
        }
        return {};
    }

    static bool LoadingRankRequestDisplayName(SDK::URH_PlayerInfo* playerInfo)
    {
        if (!playerInfo)
            return false;

        static SDK::UFunction* function = nullptr;
        if (!function)
            function = SDK::URH_PlayerInfo::StaticClass()->GetFunction(
                "RH_PlayerInfo", "BLUEPRINT_GetLinkedPlatformInfo");
        if (!function)
            return false;

        SDK::Params::RH_PlayerInfo_BLUEPRINT_GetLinkedPlatformInfo params{};
        params.bForceRefresh = true;
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        playerInfo->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return true;
    }

    static SDK::URH_PlayerInfo* LoadingRankGetOrCreatePlayerInfo(
        SDK::URH_PlayerInfoSubsystem* subsystem,
        const SDK::FGuid& playerUuid)
    {
        if (!subsystem)
            return nullptr;
        static SDK::UFunction* function = nullptr;
        if (!function)
            function = SDK::URH_PlayerInfoSubsystem::StaticClass()->GetFunction(
                "RH_PlayerInfoSubsystem", "GetOrCreatePlayerInfo");
        if (!function)
            return nullptr;

        SDK::Params::RH_PlayerInfoSubsystem_GetOrCreatePlayerInfo params{};
        params.PlayerUuid = playerUuid;
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        subsystem->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
        return SafeObjectIsA(params.ReturnValue, SDK::URH_PlayerInfo::StaticClass())
            ? params.ReturnValue
            : nullptr;
    }

    static bool LoadingRankGetWidgetBounds(
        SDK::UWidget* widget,
        ImVec2& outTopLeft,
        ImVec2& outSize)
    {
        if (!widget)
            return false;

        static SDK::UFunction* getCachedGeometryFunction = nullptr;
        static SDK::UFunction* getLocalSizeFunction = nullptr;
        static SDK::UFunction* localToViewportFunction = nullptr;
        if (!getCachedGeometryFunction)
            getCachedGeometryFunction = SDK::UWidget::StaticClass()->GetFunction("Widget", "GetCachedGeometry");
        if (!getLocalSizeFunction)
            getLocalSizeFunction = SDK::USlateBlueprintLibrary::StaticClass()->GetFunction(
                "SlateBlueprintLibrary", "GetLocalSize");
        if (!localToViewportFunction)
            localToViewportFunction = SDK::USlateBlueprintLibrary::StaticClass()->GetFunction(
                "SlateBlueprintLibrary", "LocalToViewport");
        SDK::USlateBlueprintLibrary* slateLibrary = SDK::USlateBlueprintLibrary::GetDefaultObj();
        if (!getCachedGeometryFunction || !getLocalSizeFunction || !localToViewportFunction || !slateLibrary)
            return false;

        struct GetCachedGeometryParams { SDK::FGeometry ReturnValue; };
        struct GetLocalSizeParams { SDK::FGeometry Geometry; SDK::FVector2D ReturnValue; };
        struct LocalToViewportParams
        {
            SDK::UObject* WorldContextObject;
            SDK::FGeometry Geometry;
            SDK::FVector2D LocalCoordinate;
            SDK::FVector2D PixelPosition;
            SDK::FVector2D ViewportPosition;
        };
        static_assert(sizeof(GetCachedGeometryParams) == 0x38);
        static_assert(sizeof(GetLocalSizeParams) == 0x48);
        static_assert(sizeof(LocalToViewportParams) == 0x70);

        GetCachedGeometryParams geometryParams{};
        widget->ProcessEvent(getCachedGeometryFunction, &geometryParams);
        GetLocalSizeParams sizeParams{};
        sizeParams.Geometry = geometryParams.ReturnValue;
        slateLibrary->ProcessEvent(getLocalSizeFunction, &sizeParams);
        if (!std::isfinite(sizeParams.ReturnValue.X) || !std::isfinite(sizeParams.ReturnValue.Y) ||
            sizeParams.ReturnValue.X < 20.0 || sizeParams.ReturnValue.Y < 20.0)
            return false;

        LocalToViewportParams topLeftParams{};
        topLeftParams.WorldContextObject = widget;
        topLeftParams.Geometry = geometryParams.ReturnValue;
        slateLibrary->ProcessEvent(localToViewportFunction, &topLeftParams);
        LocalToViewportParams bottomRightParams{};
        bottomRightParams.WorldContextObject = widget;
        bottomRightParams.Geometry = geometryParams.ReturnValue;
        bottomRightParams.LocalCoordinate = sizeParams.ReturnValue;
        slateLibrary->ProcessEvent(localToViewportFunction, &bottomRightParams);

        const double width = bottomRightParams.ViewportPosition.X - topLeftParams.ViewportPosition.X;
        const double height = bottomRightParams.ViewportPosition.Y - topLeftParams.ViewportPosition.Y;
        if (!std::isfinite(topLeftParams.ViewportPosition.X) || !std::isfinite(topLeftParams.ViewportPosition.Y) ||
            !std::isfinite(width) || !std::isfinite(height) || width < 20.0 || height < 20.0)
            return false;

        outTopLeft = ImVec2(
            static_cast<float>(topLeftParams.ViewportPosition.X),
            static_cast<float>(topLeftParams.ViewportPosition.Y));
        outSize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        return true;
    }

    static SDK::UHWSkillRatingRankings* LoadingRankLoadRankingsAsset(SDK::UHWRankedConfig* config)
    {
        if (!config)
            return nullptr;

        SDK::UHWSkillRatingRankings* loaded = config->SkillRatingRankingsAsset.Get();
        if (SafeObjectIsA(loaded, SDK::UHWSkillRatingRankings::StaticClass()))
            return loaded;

        SDK::TSoftObjectPtr<SDK::UObject> softAsset{};
        static_cast<SDK::FSoftObjectPtr&>(softAsset) =
            static_cast<const SDK::FSoftObjectPtr&>(config->SkillRatingRankingsAsset);
        SDK::UObject* object = SDK::UKismetSystemLibrary::LoadAsset_Blocking(softAsset);
        return SafeObjectIsA(object, SDK::UHWSkillRatingRankings::StaticClass())
            ? static_cast<SDK::UHWSkillRatingRankings*>(object)
            : nullptr;
    }

    static SDK::UObject* LoadingRankLoadAssetByPath(const wchar_t* objectPath)
    {
        if (!objectPath || !*objectPath)
            return nullptr;
        const SDK::FSoftObjectPath softPath =
            SDK::UKismetSystemLibrary::MakeSoftObjectPath(SDK::FString(objectPath));
        SDK::TSoftObjectPtr<SDK::UObject> softAsset =
            SDK::UKismetSystemLibrary::Conv_SoftObjPathToSoftObjRef(softPath);
        return SDK::UKismetSystemLibrary::LoadAsset_Blocking(softAsset);
    }

    static void LoadingRankLoadDefaultAssets(LoadingRankRuntimeContext& context)
    {
        SDK::UObject* configObject = LoadingRankLoadAssetByPath(
            L"/Game/GameModes/Ranked/RankedConfig_Conquest.RankedConfig_Conquest");
        if (SafeObjectIsA(configObject, SDK::UHWRankedConfig::StaticClass()))
            context.config = static_cast<SDK::UHWRankedConfig*>(configObject);

        SDK::UObject* rankingsObject = LoadingRankLoadAssetByPath(
            L"/Game/GameModes/Ranked/SkillRating_Rankings.SkillRating_Rankings");
        if (SafeObjectIsA(rankingsObject, SDK::UHWSkillRatingRankings::StaticClass()))
            context.rankings = static_cast<SDK::UHWSkillRatingRankings*>(rankingsObject);

        if (!context.queueRankTypes) {
            SDK::UObject* object = LoadingRankLoadAssetByPath(
                L"/Game/GameModes/GameModeAssets/DT_QueueRankTypes.DT_QueueRankTypes");
            if (SafeObjectIsA(object, SDK::UDataTable::StaticClass()))
                context.queueRankTypes = static_cast<SDK::UDataTable*>(object);
        }
    }

    static int LoadingRankGetInventorySkillRating(
        SDK::URH_PlayerInfo* playerInfo,
        const LoadingRankRuntimeContext& context)
    {
        if (!playerInfo || !context.config || context.config->SkillRatingItemId.LegacyId <= 0)
            return 0;

        int amount = SDK::UHWLibrary_RallyHere::GetCachedOwnedAmountForItem(
            context.config->SkillRatingItemId,
            playerInfo);
        return amount > 0 && amount < 100000 ? amount : 0;
    }

    static std::string LoadingRankReadText(SDK::UHWTextBlock* textBlock)
    {
        if (!textBlock)
            return {};
        const SDK::FText text = LoadingRankGetWidgetText(textBlock);
        if (text.TextData)
            return text.ToString();
        return {};
    }

    static bool LoadingRankLooksLikeBot(
        SDK::UWBP_S_PlayerCard_C* playerCard,
        SDK::AHWPlayerState* playerState,
        SDK::URH_PlayerInfo* playerInfo)
    {
        if (playerState && playerState->IsABot())
            return true;

        if (playerCard && playerCard->PlayerNameDisplay &&
            SafeObjectIsA(playerCard->PlayerNameDisplay, SDK::UWBP_S_PlayerNameDisplay_C::StaticClass())) {
            auto* nameDisplay = static_cast<SDK::UWBP_S_PlayerNameDisplay_C*>(playerCard->PlayerNameDisplay);
            if (nameDisplay->PlayerName) {
                const SDK::FText nameText = LoadingRankGetRichText(nameDisplay->PlayerName);
                std::wstring name = LoadingRankSafeText(nameText);
                while (!name.empty() && (name.back() == L' ' || name.back() == L'\t' || name.back() == L'\n'))
                    name.pop_back();
                if (name.size() >= 3 && LoadingRankContainsInsensitive(name.substr(name.size() - 3), L"bot"))
                    return true;
            }
        }

        return !playerState && !playerInfo;
    }

    static std::wstring LoadingRankResolveTier(const SDK::UHWSkillRatingRankings* rankings, int skillRating)
    {
        if (!rankings || !rankings->SkillRatingRankings.IsValid())
            return {};

        const SDK::FHWSkillRatingRank* selectedRank = nullptr;
        float selectedMinimum = -FLT_MAX;
        const auto& ranks = rankings->SkillRatingRankings;
        for (int i = 0; i < ranks.Num(); ++i) {
            const auto& rank = ranks[i];
            const bool openUpperBound = rank.RankMaximum <= rank.RankMinimum || rank.RankMaximum <= 0.0f;
            if (skillRating >= rank.RankMinimum &&
                (openUpperBound || skillRating < rank.RankMaximum)) {
                selectedRank = &rank;
                break;
            }
            if (skillRating >= rank.RankMinimum && rank.RankMinimum >= selectedMinimum) {
                selectedMinimum = rank.RankMinimum;
                selectedRank = &rank;
            }
        }
        if (!selectedRank)
            return {};

        std::wstring rankName = LoadingRankSafeText(selectedRank->RankDisplayName);
        std::wstring divisionName;
        if (selectedRank->Divisions.IsValid()) {
            float divisionMinimum = -FLT_MAX;
            for (int i = 0; i < selectedRank->Divisions.Num(); ++i) {
                const auto& division = selectedRank->Divisions[i];
                const bool openUpperBound = division.DivisionMaximum <= division.DivisionMinimum ||
                    division.DivisionMaximum <= 0.0f;
                if (skillRating >= division.DivisionMinimum &&
                    (openUpperBound || skillRating < division.DivisionMaximum)) {
                    divisionName = LoadingRankSafeText(division.DivisionDisplayName);
                    break;
                }
                if (skillRating >= division.DivisionMinimum && division.DivisionMinimum >= divisionMinimum) {
                    divisionMinimum = division.DivisionMinimum;
                    divisionName = LoadingRankSafeText(division.DivisionDisplayName);
                }
            }
        }

        if (rankName.empty())
            return divisionName;
        if (divisionName.empty() || LoadingRankContainsInsensitive(rankName, divisionName) ||
            LoadingRankContainsInsensitive(divisionName, rankName))
            return rankName.empty() ? divisionName : rankName;
        return rankName + L" " + divisionName;
    }

    static LoadingRankValue LoadingRankGetSkillRating(
        SDK::AHWPlayerState* playerState,
        SDK::URH_PlayerInfo* playerInfo,
        const LoadingRankRuntimeContext& context)
    {
        LoadingRankValue result{};
        if (playerState)
            result.skillRating = playerState->GetStartingSkillRating();

        // Visible ranked SR is stored as the ranked config's inventory item count.
        // PlayerRankingsByRankingId contains TrueSkill Mu values and must not be shown as SR.
        if (result.skillRating <= 0)
            result.skillRating = LoadingRankGetInventorySkillRating(playerInfo, context);

        if (result.skillRating <= 0 || result.skillRating >= 100000)
            result.skillRating = 0;
        return result;
    }

    static std::wstring LoadingRankBuildLabel(
        bool isBot,
        int skillRating,
        int winRatePercent,
        const LoadingRankRuntimeContext& context)
    {
        if (isBot)
            return L"AI";

        std::wstring label;
        if (skillRating <= 0) {
            label = L"\x672a\x5b9a\x7ea7  |  \x79ef\x5206 --";
        }
        else {
            const std::wstring tier = LoadingRankResolveTier(context.rankings, skillRating);
            label = tier.empty()
                ? L"\x79ef\x5206 " + std::to_wstring(skillRating)
                : tier + L"  |  \x79ef\x5206 " + std::to_wstring(skillRating);
        }
        label += winRatePercent >= 0
            ? L"  |  \x80dc\x7387 " + std::to_wstring(winRatePercent) + L"%"
            : L"  |  \x80dc\x7387 --";
        return label;
    }

    static void LoadingRankSetText(SDK::UHWTextBlock* textBlock, const std::wstring& text)
    {
        if (!textBlock || !textBlock->Class)
            return;
        const SDK::FText displayText = SDK::UKismetTextLibrary::Conv_StringToText(SDK::FString(text.c_str()));
        static SDK::UFunction* function = nullptr;
        if (!function)
            function = textBlock->Class->GetFunction("TextBlock", "SetText");
        if (!function)
            return;
        SDK::Params::TextBlock_SetText params{};
        params.InText = displayText;
        const auto flags = function->FunctionFlags;
        function->FunctionFlags |= 0x400;
        textBlock->ProcessEvent(function, &params);
        function->FunctionFlags = flags;
    }

    static bool LoadingRankGuidValid(const SDK::FGuid& guid)
    {
        return guid.A != 0 || guid.B != 0 || guid.C != 0 || guid.D != 0;
    }

    static void DraftPlayerStatsUpdate(
        const std::vector<SDK::UHWDraftLobbyPlayerEntryWidget*>& draftEntries,
        const std::vector<SDK::UWBP_S_PlayerNameDisplay_C*>& nameDisplays,
        const LoadingRankRuntimeContext& context,
        ULONGLONG now)
    {
        static std::unordered_map<SDK::UHWDraftLobbyPlayerEntryWidget*, DraftPlayerStatsState> states;
        static std::unordered_map<SDK::URH_PlayerInfo*, LoadingRankInventoryRequestState> inventoryRequests;
        static std::unordered_map<SDK::URH_PlayerInfo*, LoadingRankInventoryRequestState> matchRequests;
        static std::unordered_map<SDK::URH_PlayerInfo*, LoadingRankInventoryRequestState> nameRequests;
        static int lastLoggedEntryCount = -1;

        std::vector<DraftPlayerStatsOverlayItem> overlay;
        if (!g_showLoadingPlayerRanks || draftEntries.empty()) {
            states.clear();
            inventoryRequests.clear();
            matchRequests.clear();
            nameRequests.clear();
            if (lastLoggedEntryCount > 0)
                Dx12DebugLog("Draft player stats inactive");
            lastLoggedEntryCount = 0;
            std::lock_guard<std::mutex> lock(g_draftPlayerStatsOverlayMutex);
            g_draftPlayerStatsOverlay.clear();
            return;
        }

        std::unordered_set<SDK::UHWDraftLobbyPlayerEntryWidget*> activeEntries;
        int inventoryRequestBudget = 2;
        int matchRequestBudget = 3;
        int nameRequestBudget = 3;
        for (SDK::UHWDraftLobbyPlayerEntryWidget* entry : draftEntries) {
            if (!SafeObjectIsA(entry, SDK::UHWDraftLobbyPlayerEntryWidget::StaticClass()) ||
                !LoadingRankIsWidgetRendered(entry))
                continue;

            const SDK::FHWPersistentPlayerId persistentId = entry->GetHWPlayerId();
            SDK::AHWPlayerState* playerState = entry->GetPlayerState();
            if (!SafeObjectIsA(playerState, SDK::AHWPlayerState::StaticClass()))
                playerState = nullptr;
            const bool isBot = (playerState && playerState->IsABot()) ||
                LoadingRankGuidValid(persistentId.PersistentBotId);

            SDK::URH_PlayerInfo* playerInfo = nullptr;
            if (!isBot && LoadingRankGuidValid(persistentId.RHPlayerId) && context.playerInfoSubsystem) {
                playerInfo = LoadingRankFindPlayerInfo(context.playerInfoSubsystem, persistentId.RHPlayerId);
                if (!playerInfo) {
                    playerInfo = LoadingRankGetOrCreatePlayerInfo(
                        context.playerInfoSubsystem, persistentId.RHPlayerId);
                }
            }

            activeEntries.insert(entry);
            auto& state = states[entry];
            if (state.playerInfo != playerInfo) {
                state = {};
                state.playerInfo = playerInfo;
            }

            std::wstring displayName = LoadingRankGetDraftDisplayedName(
                entry, playerState, playerInfo, nameDisplays);
            if (displayName.empty())
                displayName = LoadingRankGetCachedDisplayName(playerState, playerInfo);
            if (!displayName.empty())
                state.stableDisplayName = displayName;
            else if (!isBot && playerInfo && state.stableDisplayName.empty() && nameRequestBudget > 0) {
                auto& request = nameRequests[playerInfo];
                constexpr int kMaxNameAttempts = 3;
                constexpr ULONGLONG kNameRetryDelayMs = 3000ull;
                if (request.attempts < kMaxNameAttempts && now >= request.nextAttemptMs) {
                    const bool queued = LoadingRankRequestDisplayName(playerInfo);
                    request.nextAttemptMs = now + (queued ? kNameRetryDelayMs : 500ull);
                    if (queued) {
                        ++request.attempts;
                        --nameRequestBudget;
                    }
                }
            }

            if (!isBot && playerInfo) {
                LoadingRankValue rating = LoadingRankGetSkillRating(playerState, playerInfo, context);
                if (rating.skillRating <= 0 && state.stableSkillRating <= 0 && inventoryRequestBudget > 0) {
                    auto& request = inventoryRequests[playerInfo];
                    constexpr int kMaxInventoryAttempts = 4;
                    constexpr ULONGLONG kInventoryRetryDelayMs = 3000ull;
                    if (request.attempts < kMaxInventoryAttempts && now >= request.nextAttemptMs) {
                        const bool queued = LoadingRankRequestSkillRatingInventory(playerInfo, context);
                        request.nextAttemptMs = now + (queued ? kInventoryRetryDelayMs : 500ull);
                        if (queued) {
                            ++request.attempts;
                            --inventoryRequestBudget;
                            rating = LoadingRankGetSkillRating(playerState, playerInfo, context);
                        }
                    }
                }
                if (rating.skillRating > 0)
                    state.stableSkillRating = rating.skillRating;

                LoadingWinRateValue winRate = LoadingRankGetWinRate(playerInfo);
                if (winRate.percent >= 0) {
                    if (state.stableWinRatePercent != winRate.percent ||
                        state.stableWinRateMatches != winRate.matches) {
                        Dx12DebugLog("Draft win rate available player=%p percent=%d matches=%d source=%s",
                            playerInfo,
                            winRate.percent,
                            winRate.matches,
                            winRate.fromRankingData ? "ranking" : "history");
                    }
                    state.stableWinRatePercent = winRate.percent;
                    state.stableWinRateMatches = winRate.matches;
                }
                else if (matchRequestBudget > 0) {
                    auto& request = matchRequests[playerInfo];
                    constexpr int kMaxMatchAttempts = 3;
                    constexpr ULONGLONG kMatchRetryDelayMs = 5000ull;
                    if (request.attempts < kMaxMatchAttempts && now >= request.nextAttemptMs) {
                        const bool rankingQueued = request.attempts == 0
                            ? LoadingRankRequestRankingData(playerInfo)
                            : false;
                        const bool historyQueued = LoadingRankRequestMatchHistory(playerInfo);
                        const bool queued = rankingQueued || historyQueued;
                        request.nextAttemptMs = now + (queued ? kMatchRetryDelayMs : 750ull);
                        if (queued) {
                            ++request.attempts;
                            --matchRequestBudget;
                        }
                    }
                }
            }

            ImVec2 topLeft{};
            ImVec2 size{};
            if (!LoadingRankGetWidgetBounds(entry, topLeft, size))
                continue;

            DraftPlayerStatsOverlayItem item{};
            item.topLeft = topLeft;
            item.size = size;
            if (isBot) {
                item.playerName = "AI";
                item.stats = TranslationWideToUtf8(L"\x79ef\x5206 --  |  \x80dc\x7387 --");
            }
            else {
                item.playerName = state.stableDisplayName.empty()
                    ? TranslationWideToUtf8(L"ID \x8bfb\x53d6\x4e2d...")
                    : TranslationWideToUtf8(state.stableDisplayName);
                const std::wstring ratingText = state.stableSkillRating > 0
                    ? std::to_wstring(state.stableSkillRating)
                    : L"--";
                const std::wstring winRateText = state.stableWinRatePercent >= 0
                    ? std::to_wstring(state.stableWinRatePercent) + L"%"
                    : L"--";
                item.stats = TranslationWideToUtf8(
                    L"\x79ef\x5206 " + ratingText + L"  |  \x80dc\x7387 " + winRateText);
            }
            overlay.push_back(std::move(item));
        }

        for (auto it = states.begin(); it != states.end();) {
            if (activeEntries.find(it->first) == activeEntries.end())
                it = states.erase(it);
            else
                ++it;
        }

        if (static_cast<int>(overlay.size()) != lastLoggedEntryCount) {
            Dx12DebugLog("Draft player stats active entries=%d playerCache=%d",
                static_cast<int>(overlay.size()), context.playerInfoSubsystem ? 1 : 0);
            lastLoggedEntryCount = static_cast<int>(overlay.size());
        }
        std::lock_guard<std::mutex> lock(g_draftPlayerStatsOverlayMutex);
        g_draftPlayerStatsOverlay = std::move(overlay);
    }

    static void DraftPlayerStatsRenderOverlay()
    {
        if (!g_showLoadingPlayerRanks)
            return;

        std::vector<DraftPlayerStatsOverlayItem> overlay;
        {
            std::lock_guard<std::mutex> lock(g_draftPlayerStatsOverlayMutex);
            overlay = g_draftPlayerStatsOverlay;
        }
        if (overlay.empty())
            return;

        ImVec2 baseMin(FLT_MAX, FLT_MAX);
        ImVec2 baseMax(-FLT_MAX, -FLT_MAX);
        for (const DraftPlayerStatsOverlayItem& item : overlay) {
            baseMin.x = std::min(baseMin.x, item.topLeft.x);
            baseMin.y = std::min(baseMin.y, item.topLeft.y);
            baseMax.x = std::max(baseMax.x, item.topLeft.x + item.size.x);
            baseMax.y = std::max(baseMax.y, item.topLeft.y + item.size.y);
        }

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImVec2 offset = g_draftStatsOffsetCustom ? g_draftStatsOffset : ImVec2(0.0f, 0.0f);
        const auto clampOffset = [&](ImVec2 value) {
            const float minX = 2.0f - baseMin.x;
            const float maxX = displaySize.x - baseMax.x - 2.0f;
            const float minY = 2.0f - baseMin.y;
            const float maxY = displaySize.y - baseMax.y - 2.0f;
            value.x = minX <= maxX ? std::clamp(value.x, minX, maxX) : minX;
            value.y = minY <= maxY ? std::clamp(value.y, minY, maxY) : minY;
            return value;
        };
        offset = clampOffset(offset);

        const bool positionEditing = g_draftStatsPositionEditEnabled && render_ui::IsVisible();
        if (positionEditing) {
            ImGui::SetNextWindowPos(baseMin + offset, ImGuiCond_Always);
            ImGui::SetNextWindowSize(baseMax - baseMin, ImGuiCond_Always);
            const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoBackground;
            if (ImGui::Begin("##draft_player_stats_drag_surface", nullptr, flags)) {
                static bool dragging = false;
                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    dragging = true;
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    dragging = false;
                if (dragging) {
                    offset = clampOffset(offset + ImGui::GetIO().MouseDelta);
                    g_draftStatsOffset = offset;
                    g_draftStatsOffsetCustom = true;
                    ImGui::SetWindowPos(baseMin + offset, ImGuiCond_Always);
                }
            }
            ImGui::End();
        }
        g_draftStatsOffset = offset;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        constexpr float fontSize = 14.0f;
        if (positionEditing) {
            draw->AddRect(baseMin + offset, baseMax + offset, IM_COL32(88, 202, 255, 230), 4.0f, 0, 2.0f);
            static const std::string editLabel = TranslationWideToUtf8(
                L"\x62d6\x52a8\x6218\x7ee9\x6846");
            draw->AddText(font, fontSize, baseMin + offset + ImVec2(6.0f, 4.0f),
                IM_COL32(88, 202, 255, 255), editLabel.c_str());
        }
        for (const DraftPlayerStatsOverlayItem& item : overlay) {
            const float blockHeight = 38.0f;
            const float left = item.topLeft.x + offset.x + 4.0f;
            const float right = item.topLeft.x + offset.x + item.size.x - 4.0f;
            const float bottom = item.topLeft.y + offset.y + item.size.y - 3.0f;
            const float top = std::max(item.topLeft.y + offset.y + 2.0f, bottom - blockHeight);
            if (right <= left || bottom <= top)
                continue;

            draw->AddRectFilled(ImVec2(left, top), ImVec2(right, bottom), IM_COL32(7, 10, 14, 196), 3.0f);
            draw->AddRect(ImVec2(left, top), ImVec2(right, bottom), IM_COL32(102, 169, 255, 150), 3.0f);
            draw->PushClipRect(ImVec2(left + 3.0f, top + 2.0f), ImVec2(right - 3.0f, bottom - 2.0f), true);
            draw->AddText(font, fontSize, ImVec2(left + 6.0f, top + 3.0f),
                IM_COL32(245, 248, 252, 255), item.playerName.c_str());
            draw->AddText(font, fontSize, ImVec2(left + 6.0f, top + 19.0f),
                IM_COL32(255, 205, 105, 255), item.stats.c_str());
            draw->PopClipRect();
        }
    }

    static void LoadingRankDisplayTick()
    {
        static ULONGLONG nextScanMs = 0;
        static std::unordered_map<SDK::UHWPlayerCardWidget*, LoadingRankCardState> cardStates;
        static std::unordered_map<SDK::URH_PlayerInfo*, LoadingRankInventoryRequestState> inventoryRequests;
        static std::unordered_map<SDK::URH_PlayerInfo*, LoadingRankInventoryRequestState> matchRequests;
        static std::unordered_map<SDK::URH_PlayerInfo*, int> loggedSkillRatings;
        static std::unordered_set<SDK::UHWRankedConfig*> attemptedRankAssets;
        static bool attemptedDefaultAssets = false;
        static bool loggedQueueRankTypes = false;
        static bool loggedRankConfig = false;
        static int lastLoggedCardCount = -1;

        const ULONGLONG now = GetTickCount64();
        if (now < nextScanMs)
            return;
        nextScanMs = now + 500ull;

        auto* objects = SDK::UObject::GObjects.GetTypedPtr();
        if (!objects)
            return;

        SDK::UHWLoadingScreenWidget* loadingScreen = nullptr;
        std::vector<SDK::UHWDraftLobbyPlayerEntryWidget*> draftEntries;
        std::vector<SDK::UWBP_S_PlayerNameDisplay_C*> draftNameDisplays;
        LoadingRankRuntimeContext context{};
        const int32_t objectCount = objects->Num();
        for (int32_t i = 0; i < objectCount; ++i) {
            SDK::UObject* object = objects->GetByIndex(i);
            if (!object || (object->Flags & SDK::EObjectFlags::ClassDefaultObject) ||
                (object->Flags & (SDK::EObjectFlags::BeginDestroyed | SDK::EObjectFlags::FinishDestroyed)))
                continue;

            if (!context.rankings && SafeObjectIsA(object, SDK::UHWSkillRatingRankings::StaticClass())) {
                auto* candidate = static_cast<SDK::UHWSkillRatingRankings*>(object);
                if (candidate->SkillRatingRankings.IsValid() &&
                    candidate->SkillRatingRankings.Num() > 0 && candidate->SkillRatingRankings.Num() <= 32)
                    context.rankings = candidate;
            }

            if (!context.config && SafeObjectIsA(object, SDK::UHWRankedConfig::StaticClass())) {
                auto* candidate = static_cast<SDK::UHWRankedConfig*>(object);
                if (candidate->DefaultSkillRating > 0 || candidate->SkillRatingMmrRatio > 0.0f)
                    context.config = candidate;
            }

            if (!context.subsystem && SafeObjectIsA(object, SDK::UHWRankedSubsystem::StaticClass()))
                context.subsystem = static_cast<SDK::UHWRankedSubsystem*>(object);

            if (!context.playerInfoSubsystem && SafeObjectIsA(object, SDK::URH_PlayerInfoSubsystem::StaticClass())) {
                auto* candidate = static_cast<SDK::URH_PlayerInfoSubsystem*>(object);
                if (candidate->PlayerInfos.Num() >= 0 && candidate->PlayerInfos.Num() <= 256)
                    context.playerInfoSubsystem = candidate;
            }

            if (!loadingScreen && SafeObjectIsA(object, SDK::UHWLoadingScreenWidget::StaticClass())) {
                auto* candidate = static_cast<SDK::UHWLoadingScreenWidget*>(object);
                if (candidate->PlayerWidgets.IsValid() && candidate->PlayerWidgets.Num() > 0 &&
                    candidate->PlayerWidgets.Num() <= 20 && LoadingRankIsWidgetRendered(candidate))
                    loadingScreen = candidate;
            }

            if (SafeObjectIsA(object, SDK::UHWDraftLobbyPlayerEntryWidget::StaticClass())) {
                auto* candidate = static_cast<SDK::UHWDraftLobbyPlayerEntryWidget*>(object);
                if (LoadingRankIsWidgetRendered(candidate) && draftEntries.size() < 20)
                    draftEntries.push_back(candidate);
            }

            if (draftNameDisplays.size() < 256 &&
                SafeObjectIsA(object, SDK::UWBP_S_PlayerNameDisplay_C::StaticClass()))
                draftNameDisplays.push_back(static_cast<SDK::UWBP_S_PlayerNameDisplay_C*>(object));
        }

        if (!loadingScreen && draftEntries.empty()) {
            cardStates.clear();
            inventoryRequests.clear();
            matchRequests.clear();
            loggedSkillRatings.clear();
            attemptedRankAssets.clear();
            attemptedDefaultAssets = false;
            loggedQueueRankTypes = false;
            loggedRankConfig = false;
            if (lastLoggedCardCount > 0)
                Dx12DebugLog("Loading rank display inactive");
            lastLoggedCardCount = 0;
            DraftPlayerStatsUpdate(draftEntries, draftNameDisplays, context, now);
            return;
        }

        if (context.subsystem) {
            if (SafeObjectIsA(context.subsystem->CachedRankConfig, SDK::UHWRankedConfig::StaticClass()))
                context.config = context.subsystem->CachedRankConfig;
            if (SafeObjectIsA(context.subsystem->CachedSkillRatingRankings, SDK::UHWSkillRatingRankings::StaticClass()))
                context.rankings = context.subsystem->CachedSkillRatingRankings;
        }
        if (!context.rankings && context.config) {
            SDK::UHWSkillRatingRankings* loaded = context.config->SkillRatingRankingsAsset.Get();
            if (SafeObjectIsA(loaded, SDK::UHWSkillRatingRankings::StaticClass()))
                context.rankings = loaded;
            else if (attemptedRankAssets.insert(context.config).second)
                context.rankings = LoadingRankLoadRankingsAsset(context.config);
        }
        if (!context.queueRankTypes && context.config) {
            SDK::UDataTable* table = context.config->QueueRankTypesTable.Get();
            if (SafeObjectIsA(table, SDK::UDataTable::StaticClass()))
                context.queueRankTypes = table;
        }
        if (!attemptedDefaultAssets) {
            attemptedDefaultAssets = true;
            LoadingRankLoadDefaultAssets(context);
            Dx12DebugLog("Loading rank default assets rankAsset=%d rankConfig=%d queueTable=%d",
                context.rankings ? 1 : 0,
                context.config ? 1 : 0,
                context.queueRankTypes ? 1 : 0);
        }
        if (context.config && !loggedRankConfig) {
            loggedRankConfig = true;
            Dx12DebugLog("Loading rank config skillItem=%d defaultSR=%d profiles=%d",
                context.config->SkillRatingItemId.LegacyId,
                context.config->DefaultSkillRating,
                context.config->ProfilesWithSkillRating.Num());
        }
        if (context.queueRankTypes && !loggedQueueRankTypes && context.queueRankTypes->RowMap.IsValid() &&
            context.queueRankTypes->RowMap.Num() > 0 && context.queueRankTypes->RowMap.Num() <= 128) {
            loggedQueueRankTypes = true;
            for (auto& rowEntry : context.queueRankTypes->RowMap) {
                auto* row = reinterpret_cast<const SDK::FHWQueueRankTypes*>(rowEntry.Value());
                if (!row)
                    continue;
                Dx12DebugLog("Loading rank queue row=%s baseRankId=%s roles=%d",
                    rowEntry.Key().ToString().c_str(),
                    TranslationWideToUtf8(row->BaseRankType.ToWString()).c_str(),
                    row->RoleRankTypes.Num());
            }
        }

        DraftPlayerStatsUpdate(draftEntries, draftNameDisplays, context, now);

        if (!loadingScreen) {
            cardStates.clear();
            inventoryRequests.clear();
            matchRequests.clear();
            loggedSkillRatings.clear();
            if (lastLoggedCardCount > 0)
                Dx12DebugLog("Loading rank display inactive");
            lastLoggedCardCount = 0;
            return;
        }

        std::unordered_set<SDK::UHWPlayerCardWidget*> activeCards;
        int inventoryRequestBudget = 2;
        int matchRequestBudget = 3;
        for (auto& entry : loadingScreen->PlayerWidgets) {
            SDK::UHWPlayerCardWidget* card = entry.Value();
            if (!SafeObjectIsA(card, SDK::UWBP_S_PlayerCard_C::StaticClass()))
                continue;
            auto* playerCard = static_cast<SDK::UWBP_S_PlayerCard_C*>(card);
            if (!playerCard->PlayerTitle)
                continue;

            activeCards.insert(card);
            auto& state = cardStates[card];
            if (!state.captured) {
                state.originalTitle = LoadingRankReadText(playerCard->PlayerTitle);
                state.originalTitleVisibility = LoadingRankGetWidgetVisibility(playerCard->PlayerTitle);
                if (playerCard->RankBackground)
                    state.originalBackgroundVisibility = LoadingRankGetWidgetVisibility(playerCard->RankBackground);
                state.captured = true;
            }

            if (!g_showLoadingPlayerRanks) {
                if (state.applied) {
                    LoadingRankSetText(playerCard->PlayerTitle, TranslationUtf8ToWide(state.originalTitle));
                    LoadingRankSetWidgetVisibility(playerCard->PlayerTitle, state.originalTitleVisibility);
                    if (playerCard->RankBackground)
                        LoadingRankSetWidgetVisibility(playerCard->RankBackground, state.originalBackgroundVisibility);
                    state.applied = false;
                }
                continue;
            }

            SDK::AHWPlayerState* playerState = card->GetPlayerState();
            SDK::URH_PlayerInfo* playerInfo = card->GetPlayerInfo();
            if (!SafeObjectIsA(playerInfo, SDK::URH_PlayerInfo::StaticClass()))
                playerInfo = LoadingRankFindPlayerInfo(context.playerInfoSubsystem, entry.Key());

            const bool isBot = LoadingRankLooksLikeBot(playerCard, playerState, playerInfo);
            int displaySkillRating = 0;
            int displayWinRatePercent = -1;
            if (!isBot && playerInfo) {
                if (state.ratingPlayerInfo && state.ratingPlayerInfo != playerInfo) {
                    state.stableSkillRating = 0;
                    state.stableWinRatePercent = -1;
                    state.stableWinRateMatches = 0;
                }
                state.ratingPlayerInfo = playerInfo;

                LoadingRankValue rating = LoadingRankGetSkillRating(playerState, playerInfo, context);
                if (rating.skillRating <= 0 && state.stableSkillRating <= 0 && inventoryRequestBudget > 0) {
                    auto& request = inventoryRequests[playerInfo];
                    constexpr int kMaxInventoryAttempts = 4;
                    constexpr ULONGLONG kInventoryRetryDelayMs = 3000ull;
                    if (request.attempts < kMaxInventoryAttempts && now >= request.nextAttemptMs) {
                        const bool queued = LoadingRankRequestSkillRatingInventory(playerInfo, context);
                        request.nextAttemptMs = now + (queued ? kInventoryRetryDelayMs : 500ull);
                        if (queued)
                            ++request.attempts;
                        if (queued)
                            --inventoryRequestBudget;
                        Dx12DebugLog("Loading rank inventory request player=%p item=%d attempt=%d queued=%d",
                            playerInfo,
                            context.config ? context.config->SkillRatingItemId.LegacyId : 0,
                            request.attempts,
                            queued ? 1 : 0);

                        if (queued)
                            rating = LoadingRankGetSkillRating(playerState, playerInfo, context);
                    }
                }
                if (rating.skillRating > 0)
                    state.stableSkillRating = rating.skillRating;
                displaySkillRating = state.stableSkillRating;

                auto logged = loggedSkillRatings.find(playerInfo);
                if (rating.skillRating > 0 &&
                    (logged == loggedSkillRatings.end() || logged->second != rating.skillRating)) {
                    loggedSkillRatings[playerInfo] = rating.skillRating;
                    Dx12DebugLog("Loading rank inventory cache player=%p rating=%d",
                        playerInfo,
                        rating.skillRating);
                }

                LoadingWinRateValue winRate = LoadingRankGetWinRate(playerInfo);
                if (winRate.percent >= 0) {
                    if (state.stableWinRatePercent != winRate.percent ||
                        state.stableWinRateMatches != winRate.matches) {
                        Dx12DebugLog("Loading win rate available player=%p percent=%d matches=%d source=%s",
                            playerInfo,
                            winRate.percent,
                            winRate.matches,
                            winRate.fromRankingData ? "ranking" : "history");
                    }
                    state.stableWinRatePercent = winRate.percent;
                    state.stableWinRateMatches = winRate.matches;
                }
                else if (state.stableWinRatePercent < 0 && matchRequestBudget > 0) {
                    auto& request = matchRequests[playerInfo];
                    constexpr int kMaxMatchAttempts = 3;
                    constexpr ULONGLONG kMatchRetryDelayMs = 5000ull;
                    if (request.attempts < kMaxMatchAttempts && now >= request.nextAttemptMs) {
                        const bool rankingQueued = request.attempts == 0
                            ? LoadingRankRequestRankingData(playerInfo)
                            : false;
                        const bool historyQueued = LoadingRankRequestMatchHistory(playerInfo);
                        const bool queued = rankingQueued || historyQueued;
                        request.nextAttemptMs = now + (queued ? kMatchRetryDelayMs : 750ull);
                        if (queued) {
                            ++request.attempts;
                            --matchRequestBudget;
                        }
                        Dx12DebugLog("Loading win rate request player=%p attempt=%d history=%d ranking=%d",
                            playerInfo,
                            request.attempts,
                            historyQueued ? 1 : 0,
                            rankingQueued ? 1 : 0);
                    }
                }
                displayWinRatePercent = state.stableWinRatePercent;
            }
            else if (!isBot) {
                const LoadingRankValue rating = LoadingRankGetSkillRating(playerState, nullptr, context);
                if (rating.skillRating > 0)
                    state.stableSkillRating = rating.skillRating;
                displaySkillRating = state.stableSkillRating;
            }

            const std::wstring label = LoadingRankBuildLabel(
                isBot, displaySkillRating, displayWinRatePercent, context);
            const std::string currentText = LoadingRankReadText(playerCard->PlayerTitle);
            if (currentText != TranslationWideToUtf8(label))
                LoadingRankSetText(playerCard->PlayerTitle, label);
            LoadingRankSetWidgetVisibility(playerCard->PlayerTitle, SDK::ESlateVisibility::Visible);
            if (playerCard->RankBackground)
                LoadingRankSetWidgetVisibility(playerCard->RankBackground, SDK::ESlateVisibility::Visible);
            state.applied = true;
        }

        for (auto it = cardStates.begin(); it != cardStates.end();) {
            if (activeCards.find(it->first) == activeCards.end())
                it = cardStates.erase(it);
            else
                ++it;
        }

        const int activeCount = static_cast<int>(activeCards.size());
        if (activeCount != lastLoggedCardCount) {
            Dx12DebugLog("Loading rank display active cards=%d rankAsset=%d rankConfig=%d queueTable=%d playerCache=%d",
                activeCount,
                context.rankings ? 1 : 0,
                context.config ? 1 : 0,
                context.queueRankTypes ? 1 : 0,
                context.playerInfoSubsystem ? 1 : 0);
            lastLoggedCardCount = activeCount;
        }
    }
}

static void InstallGameRuntimeTimer(HWND hWnd)
{
    if (!hWnd)
        return;

    if (g_gameRuntimeTimerWindow && g_gameRuntimeTimerWindow != hWnd)
        KillTimer(g_gameRuntimeTimerWindow, kGameRuntimeTimerId);

    g_gameRuntimeTimerWindow = hWnd;
    g_gameWindowThreadId = GetWindowThreadProcessId(hWnd, nullptr);
    if (!SetTimer(hWnd, kGameRuntimeTimerId, 16, nullptr)) {
        g_gameRuntimeTimerWindow = nullptr;
        g_gameWindowThreadId = 0;
        Dx12DebugLog("Game runtime timer installation failed hwnd=%p error=%lu", hWnd, GetLastError());
    }
}

static void RemoveGameRuntimeTimer()
{
    if (g_gameRuntimeTimerWindow)
        KillTimer(g_gameRuntimeTimerWindow, kGameRuntimeTimerId);
    g_gameRuntimeTimerWindow = nullptr;
    g_gameWindowThreadId = 0;
}

static void RunGameThreadMutationTick()
{
    static bool s_running = false;
    static ULONGLONG s_lastTickMs = 0;

    if (s_running)
        return;
    if (g_gameWindowThreadId != 0 && GetCurrentThreadId() != g_gameWindowThreadId)
        return;

    const ULONGLONG now = GetTickCount64();
    if (now - s_lastTickMs < 8)
        return;
    s_lastTickMs = now;
    s_running = true;

    __try {
        TranslationTickGameThread();
        LoadingRankDisplayTick();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Dx12DebugLog("Game runtime timer tick rejected an invalid SDK object");
    }

    s_running = false;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TIMER && wParam == kGameRuntimeTimerId) {
        RunGameThreadMutationTick();
        return 0;
    }

    // Intercept the menu toggle key on key release. The compact menu
    // handles its own toggling internally via GetAsyncKeyState on
    // g_menuKeyVirtual.  Swallow the key up event so the game does not
    // receive it.  Do not toggle g_showMenu or g_useModernMenu here.
    if (msg == WM_KEYUP) {
        if (wParam == (WPARAM)g_menuKeyVirtual) {
            return TRUE;
        }
    }
    
    // Intercept WM_SETCURSOR for the client area.  Always hide the OS cursor
    // inside the game window to prevent it from appearing after alt‑tabbing.
    // When the overlay menu is open, ImGui draws its own software cursor.
    // When the menu is closed, the game renders its own crosshair.
    if (msg == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT) {
        ::SetCursor(nullptr);
        return TRUE;
    }

    if (msg == WM_SIZE && wParam != SIZE_MINIMIZED) {
        g_needRTVRecreate = true;
    }
    // Always forward messages to ImGui.  When the debug menu is visible it
    // will consume input; otherwise ImGui_ImplWin32_WndProcHandler returns false.
    // Process the message with the Dear ImGui Win32 backend.  This updates
    // internal state and returns true if ImGui wants the message to be
    // swallowed.  Do not early‑return here: even when ImGui does not
    // explicitly consume a message, we may still need to suppress it while
    // our overlay is open to prevent the underlying game from receiving
    // clicks/keypresses.
    const bool menuWantsInput = render_ui::WantsInput();
    const bool imgui_consumed = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) != 0;
    // If the compact menu is toggled on we want to absorb all
    // keyboard and mouse input (including messages that Dear ImGui may not
    // explicitly consume).  Without this check, certain messages such as
    // WM_MOUSEMOVE or WM_MOUSEWHEEL will propagate through to the game
    // when the menu is open, leading to accidental in‑game interactions.
    if (menuWantsInput) {
        // Range checks for keyboard and mouse messages. Keep raw input flowing
        // to the game because blocking WM_INPUT in draft/loading screens can
        // interfere with the engine's own input state.
        const bool is_keyboard = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || msg == WM_CHAR || msg == WM_SYSCHAR || msg == WM_UNICHAR;
        const bool is_mouse    = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL;
        if (is_keyboard || is_mouse) {
            return TRUE;
        }
    }
    // If ImGui signalled that it has consumed the message, return here to
    // avoid forwarding it to the original window procedure.
    if (menuWantsInput && imgui_consumed) {
        return TRUE;
    }
    // Guard against a missing original WndProc (e.g., SetWindowLongPtr
    // failed or the game swapped windows).  Falling back to DefWindowProc
    // prevents null-call crashes while still allowing the game to handle
    // messages normally.
    if (!g_originalWndProc) {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return CallWindowProc(g_originalWndProc, hWnd, msg, wParam, lParam);
}

// -------------------- hkPresent --------------------
static HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
#if 1 // Present hook body enabled
    if (!pSwapChain) return S_OK;
    if (!g_originalPresent) {
        // Hook didn't capture the original Present; fall back to the swap chain's vtable to avoid crashing.
        return pSwapChain->Present(SyncInterval, Flags);
    }


    static UINT s_presentWarmupFrames = 0;
    if (s_presentWarmupFrames < kDx12PresentWarmupFrames) {
        ++s_presentWarmupFrames;
        return g_originalPresent(pSwapChain, SyncInterval, Flags);
    }

    ScopedPresentRenderLock renderLock;
    if (!renderLock.acquired) {
        return g_originalPresent(pSwapChain, SyncInterval, Flags);
    }

    if (!g_imguiInitialized) {
        InitializeImGui(pSwapChain);
    }
    ID3D12CommandQueue* commandQueue = AtomicReadCommandQueue();
    if (!g_imguiInitialized || !commandQueue) {
        return g_originalPresent(pSwapChain, SyncInterval, Flags);
    }
    // Do not scan or mutate actor visibility from Present. Doing so can collide
    // with UE's D3D12/RHI frame submission and trigger PresentInternal failures.
    // Rebind WndProc if the window changed (e.g., on map load)
    DXGI_SWAP_CHAIN_DESC sd{};
    if (SUCCEEDED(pSwapChain->GetDesc(&sd))) {
        if (g_hWnd && g_hWnd != sd.OutputWindow) {
            RemoveGameRuntimeTimer();
            if (g_originalWndProc) {
                SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
            }
            g_hWnd = sd.OutputWindow;
            g_originalWndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
            InstallGameRuntimeTimer(g_hWnd);
            g_needRTVRecreate = true;
        }
    }

    // Recover the DX12 device if the swap chain was reset or recreated.
    if (!g_device) {
        if (FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_device)))) {
            return g_originalPresent(pSwapChain, SyncInterval, Flags);
        }
        g_needRTVRecreate = true;
    }

    if (g_needRTVRecreate || g_frameContexts.empty() || !g_rtvHeap || !g_commandList) {
        if (!CreateOrRecreateRTV(pSwapChain)) {
            return g_originalPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    Render();

    DraftPlayerStatsRenderOverlay();
    TranslationRenderOverlay();

    ImGui::Render();

// =========================================================================
// [OVERLAY DISABLED] Game-side DX12 ImGui rendering — preserved for rollback
// Active path: game-side DX12 ImGui rendering for the main menu.
// Standalone overlay is ticked above as a separate self-drawn status layer.
#if 1
    // Draw ImGui through the game's D3D12 swapchain. The standalone window
    // does not share the menu textures or the game swapchain resources.
    commandQueue = AtomicReadCommandQueue();
    if (g_frameContexts.empty() || !g_commandList || !commandQueue || !g_srvHeap) {
        return g_originalPresent(pSwapChain, SyncInterval, Flags);
    }
    const UINT frameIndex = GetCurrentBackBufferIndex(pSwapChain) % static_cast<UINT>(g_frameContexts.size());
    Dx12FrameContext& frame = g_frameContexts[frameIndex];
    if (!frame.commandAllocator || !frame.renderTarget) {
        return g_originalPresent(pSwapChain, SyncInterval, Flags);
    }
    if (g_frameFence && g_frameFenceEvent && frame.fenceValue != 0 &&
        g_frameFence->GetCompletedValue() < frame.fenceValue) {
        if (SUCCEEDED(g_frameFence->SetEventOnCompletion(frame.fenceValue, g_frameFenceEvent))) {
            if (WaitForSingleObject(g_frameFenceEvent, 0) != WAIT_OBJECT_0) {
                return g_originalPresent(pSwapChain, SyncInterval, Flags);
            }
        } else {
            return g_originalPresent(pSwapChain, SyncInterval, Flags);
        }
    }
    if (FAILED(frame.commandAllocator->Reset()) ||
        FAILED(g_commandList->Reset(frame.commandAllocator, nullptr))) {
        return g_originalPresent(pSwapChain, SyncInterval, Flags);
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = frame.renderTarget;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_commandList->ResourceBarrier(1, &barrier);
    g_commandList->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
    ID3D12DescriptorHeap* descriptorHeaps[] = { g_srvHeap };
    g_commandList->SetDescriptorHeaps(1, descriptorHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_commandList->ResourceBarrier(1, &barrier);
    if (SUCCEEDED(g_commandList->Close())) {
        ID3D12CommandList* commandLists[] = { g_commandList };
        commandQueue->ExecuteCommandLists(1, commandLists);
        if (g_frameFence) {
            const UINT64 signalValue = g_nextFenceValue++;
            if (SUCCEEDED(commandQueue->Signal(g_frameFence, signalValue))) {
                frame.fenceValue = signalValue;
            }
        }
    }
    else {
        return g_originalPresent(pSwapChain, SyncInterval, Flags);
    }
    return g_originalPresent(pSwapChain, SyncInterval, Flags);

#endif // [OVERLAY DISABLED] game-side DX12 rendering
#else
    // Passthrough: hooks disabled, just return S_OK
    return S_OK;
#endif // Present hook body enabled
}

// -------------------- Cleanup & DllMain --------------------
static void CleanupAll() {
    TranslationStopRuntime();
    RemoveGameRuntimeTimer();
    WaitForDX12Idle();
    if (g_imguiInitialized) {
        render_ui::OnDeviceDestroyed();
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if (g_hWnd && g_originalWndProc) {
            SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
            g_originalWndProc = nullptr;
        }
        g_imguiInitialized = false;
    }
    DestroyDX12RenderTargets();
    if (g_frameFenceEvent) {
        CloseHandle(g_frameFenceEvent);
        g_frameFenceEvent = nullptr;
    }
    SafeRelease(g_frameFence);
    SafeRelease(g_srvHeap);
    memset(g_srvDescriptorUsed, 0, sizeof(g_srvDescriptorUsed));
    g_srvDescriptorSize = 0;
    g_nextFenceValue = 1;
    ReleaseCapturedCommandQueue();
    SafeRelease(g_device);
#if 0 // [REMOVED] MH hook cleanup - hooks not installed
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    g_originalPresent = nullptr;
    g_originalExecuteCommandLists = nullptr;
    g_originalCreateSwapChain = nullptr;
    g_originalCreateSwapChainForHwnd = nullptr;
    g_originalCreateSwapChainForCoreWindow = nullptr;
    g_originalCreateSwapChainForComposition = nullptr;
#endif // [REMOVED]
    // [REMOVED] MH hooks cleanup

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE h = CreateThread(nullptr, 0, BootstrapThread, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        CleanupAll();
    }
    return TRUE;
}

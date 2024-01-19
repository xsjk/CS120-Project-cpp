#pragma once

#include <winsock2.h>
#include <Windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <mstcpip.h>
#include <winternl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "wintun.h"
#include <locale>
#include <codecvt>
#include <string>
#include <memory>
#include <unordered_set>
#include <asyncio.hpp>
#include <networkheaders.hpp>
#include <asyncio.hpp>


namespace WinTUN {


void RtlIpv4AddressToStringW(struct in_addr *Addr, PWSTR S) {
    swprintf_s(S, 16, L"%u.%u.%u.%u", Addr->S_un.S_un_b.s_b1, Addr->S_un.S_un_b.s_b2, Addr->S_un.S_un_b.s_b3,
            Addr->S_un.S_un_b.s_b4);
}

void RtlIpv6AddressToStringW(struct in6_addr *Addr, PWSTR S) {
    swprintf_s(S, 46, L"%x:%x:%x:%x:%x:%x:%x:%x", htons(Addr->u.Word[0]), htons(Addr->u.Word[1]),
            htons(Addr->u.Word[2]), htons(Addr->u.Word[3]), htons(Addr->u.Word[4]), htons(Addr->u.Word[5]),
            htons(Addr->u.Word[6]), htons(Addr->u.Word[7]));
}
void CALLBACK ConsoleLogger(
    _In_ WINTUN_LOGGER_LEVEL Level, 
    _In_ DWORD64 Timestamp, 
    _In_z_ const WCHAR *LogLine
) {
    SYSTEMTIME SystemTime;
    FileTimeToSystemTime((FILETIME *)&Timestamp, &SystemTime);
    WCHAR LevelMarker;
    switch (Level) {
        case WINTUN_LOG_INFO:
            LevelMarker = L'+';
            break;
        case WINTUN_LOG_WARN:
            LevelMarker = L'-';
            break;
        case WINTUN_LOG_ERR:
            LevelMarker = L'!';
            break;
        default:
            return;
    }
    fwprintf_s(
        stderr,
        L"%04u-%02u-%02u %02u:%02u:%02u.%04u [%c] %s\n",
        SystemTime.wYear,
        SystemTime.wMonth,
        SystemTime.wDay,
        SystemTime.wHour,
        SystemTime.wMinute,
        SystemTime.wSecond,
        SystemTime.wMilliseconds,
        LevelMarker,
        LogLine
    );
}

DWORD64 Now(VOID) {
    LARGE_INTEGER Timestamp;
    NtQuerySystemTime(&Timestamp);
    return Timestamp.QuadPart;
}

DWORD LogError(_In_z_ const WCHAR *Prefix, _In_ DWORD Error) {
    WCHAR *SystemMessage = NULL, *FormattedMessage = NULL;
    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL,
        HRESULT_FROM_SETUPAPI(Error),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&SystemMessage,
        0,
        NULL);

    DWORD_PTR args[3] = { (DWORD_PTR)Prefix, (DWORD_PTR)Error, (DWORD_PTR)SystemMessage };
    FormatMessageW(
        FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY |
        FORMAT_MESSAGE_MAX_WIDTH_MASK,
        SystemMessage ? L"%1: %3(Code 0x%2!08X!)" : L"%1: Code 0x%2!08X!",
        0,
        0,
        (LPWSTR)&FormattedMessage,
        0,
        (va_list *)args);
    if (FormattedMessage)
        ConsoleLogger(WINTUN_LOG_ERR, Now(), FormattedMessage);
    LocalFree(FormattedMessage);
    LocalFree(SystemMessage);
    return Error;
}

DWORD LogLastError(_In_z_ const WCHAR *Prefix) {
    DWORD LastError = GetLastError();
    LogError(Prefix, LastError);
    SetLastError(LastError);
    return LastError;
}

void Log(_In_ WINTUN_LOGGER_LEVEL Level, _In_z_ const WCHAR *Format, ...) {
    WCHAR LogLine[0x200];
    va_list args;
    va_start(args, Format);
    _vsnwprintf_s(LogLine, _countof(LogLine), _TRUNCATE, Format, args);
    va_end(args);
    ConsoleLogger(Level, Now(), LogLine);
}

void PrintPacket(_In_ const BYTE *Packet, _In_ DWORD PacketSize) {
    if (PacketSize < 20) {
        Log(WINTUN_LOG_INFO, L"Received packet without room for an IP header");
        return;
    }
    BYTE IpVersion = Packet[0] >> 4, Proto;
    WCHAR Src[46], Dst[46];
    if (IpVersion == 4) {
        RtlIpv4AddressToStringW((struct in_addr *)&Packet[12], Src);
        RtlIpv4AddressToStringW((struct in_addr *)&Packet[16], Dst);
        Proto = Packet[9];
        Packet += 20, PacketSize -= 20;
    }
    else if (IpVersion == 6 && PacketSize < 40) {
        Log(WINTUN_LOG_INFO, L"Received packet without room for an IP header");
        return;
    }
    else if (IpVersion == 6) {
        RtlIpv6AddressToStringW((struct in6_addr *)&Packet[8], Src);
        RtlIpv6AddressToStringW((struct in6_addr *)&Packet[24], Dst);
        Proto = Packet[6];
        Packet += 40, PacketSize -= 40;
    }
    else {
        Log(WINTUN_LOG_INFO, L"Received packet that was not IP");
        return;
    }
    if (Proto == 1 && PacketSize >= 8 && Packet[0] == 0)
        Log(WINTUN_LOG_INFO, L"Received IPv%d ICMP echo reply from %s to %s", IpVersion, Src, Dst);
    else
        Log(WINTUN_LOG_INFO, L"Received IPv%d proto 0x%x packet from %s to %s", IpVersion, Proto, Src, Dst);
}

USHORT IPChecksum(_In_reads_bytes_(Len) BYTE *Buffer, _In_ DWORD Len) {
    ULONG Sum = 0;
    for (; Len > 1; Len -= 2, Buffer += 2)
        Sum += *(USHORT *)Buffer;
    if (Len)
        Sum += *Buffer;
    Sum = (Sum >> 16) + (Sum & 0xffff);
    Sum += (Sum >> 16);
    return (USHORT)(~Sum);
}

void MakeICMP(_Out_writes_bytes_all_(28) BYTE Packet[28]) {
    memset(Packet, 0, 28);
    Packet[0] = 0x45;
    *(USHORT *)&Packet[2] = htons(28);
    Packet[8] = 255;
    Packet[9] = 1;
    *(ULONG *)&Packet[12] = htonl((10 << 24) | (6 << 16) | (7 << 8) | (8 << 0)); /* 10.6.7.8 */
    *(ULONG *)&Packet[16] = htonl((10 << 24) | (6 << 16) | (7 << 8) | (7 << 0)); /* 10.6.7.7 */
    *(USHORT *)&Packet[10] = IPChecksum(Packet, 20);
    Packet[20] = 8;
    *(USHORT *)&Packet[22] = IPChecksum(&Packet[20], 8);
    Log(WINTUN_LOG_INFO, L"Sending IPv4 ICMP echo request to 10.6.7.8 from 10.6.7.7");
}


class Device;

class Session : public std::enable_shared_from_this<Session> {

    std::shared_ptr<Device> device;
    WINTUN_SESSION_HANDLE handle;
    HANDLE ReadEvent;
    MIB_UNICASTIPADDRESS_ROW AddressRow;

    friend class Device;

    Context senderContext, receiverContext;


public:

    Session(std::shared_ptr<Device>, IPV4_addr);

    ~Session();

    template<std::size_t N>
    async def async_wait(auto &context, HANDLE (&handles)[N]);

    async def async_read(boost::asio::streambuf &buf);

    async def async_send(boost::asio::streambuf &buf);
};


namespace API {
    
    WINTUN_CREATE_ADAPTER_FUNC *WintunCreateAdapter;
    WINTUN_CLOSE_ADAPTER_FUNC *WintunCloseAdapter;
    WINTUN_OPEN_ADAPTER_FUNC *WintunOpenAdapter;
    WINTUN_GET_ADAPTER_LUID_FUNC *WintunGetAdapterLUID;
    WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC *WintunGetRunningDriverVersion;
    WINTUN_DELETE_DRIVER_FUNC *WintunDeleteDriver;
    WINTUN_SET_LOGGER_FUNC *WintunSetLogger;
    WINTUN_START_SESSION_FUNC *WintunStartSession;
    WINTUN_END_SESSION_FUNC *WintunEndSession;
    WINTUN_GET_READ_WAIT_EVENT_FUNC *WintunGetReadWaitEvent;
    WINTUN_RECEIVE_PACKET_FUNC *WintunReceivePacket;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC *WintunReleaseReceivePacket;
    WINTUN_ALLOCATE_SEND_PACKET_FUNC *WintunAllocateSendPacket;
    WINTUN_SEND_PACKET_FUNC *WintunSendPacket;


    struct Loader {
        HMODULE handle;
        
        Loader() {
            handle =
                LoadLibraryExW(L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!handle)
                return;
        #define X(Name) ((*(FARPROC *)&Name = GetProcAddress(handle, #Name)) == NULL)
            if (X(WintunCreateAdapter) || X(WintunCloseAdapter) || X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
                X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) || X(WintunSetLogger) || X(WintunStartSession) ||
                X(WintunEndSession) || X(WintunGetReadWaitEvent) || X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
                X(WintunAllocateSendPacket) || X(WintunSendPacket))
        #undef X
            {
                LogError(L"Failed to initialize Wintun", GetLastError());
                FreeLibrary(handle);
                exit(-1);
            }
        }

        ~Loader() {
            FreeLibrary(handle);
        }

    } loader;


};


class Device : public std::enable_shared_from_this<Device>{

    HANDLE QuitEvent;

    struct Config {
        std::string name;
        unsigned ip;
        GUID guid;
        static constexpr int prefix_len = 24;
    };

    static std::unordered_set<Device *> instances;

    WINTUN_ADAPTER_HANDLE Adapter;
    DWORD Version;

    friend class Session;

public:

    Device(std::string name, GUID guid) {
    
        DWORD LastError;
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

        instances.insert(this);

        API::WintunSetLogger(ConsoleLogger);
        Log(WINTUN_LOG_INFO, L"Wintun library loaded");

        QuitEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!QuitEvent) {
            LastError = LogError(L"Failed to create event", GetLastError());
            return;
        }

        Adapter = API::WintunCreateAdapter(converter.from_bytes(name).c_str(), L"wintun", &guid);
        if (!Adapter) {
            LastError = GetLastError();
            LogError(L"Failed to create adapter", LastError);
            goto cleanupQuit;
        }

        Version = API::WintunGetRunningDriverVersion();
        Log(WINTUN_LOG_INFO, L"Wintun v%u.%u loaded", (Version >> 16) & 0xff, (Version >> 0) & 0xff);

        return;

    cleanupQuit:
        CloseHandle(QuitEvent);
    }


    ~Device() {
        CloseHandle(QuitEvent);
        instances.erase(this);
    }


    auto open(IPV4_addr ip) {
        return std::make_shared<Session>(shared_from_this(), ip);
    }

    void request_stop() {
        SetEvent(QuitEvent);
    }

    static void request_stop_all() {
        for (auto d: instances)
            d->request_stop();
    }

};

std::unordered_set<Device*> Device::instances;



struct CtrlHandler {

    static BOOL WINAPI
    callback(_In_ DWORD CtrlType) {
        switch (CtrlType) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                Log(WINTUN_LOG_INFO, L"Cleaning up and shutting down...");
                Device::request_stop_all();
                asyncio.pause();
                return TRUE;
        }
        return FALSE;
    }

    CtrlHandler() {
        if (!SetConsoleCtrlHandler(callback, TRUE)) {
            LogError(L"Failed to set console handler", GetLastError());
            SetConsoleCtrlHandler(callback, FALSE);
        }
    }

    ~CtrlHandler() {
        SetConsoleCtrlHandler(callback, FALSE);
    }

} ctrlHandler;


Session::Session(std::shared_ptr<Device> device, IPV4_addr ip) : device(device) {

    InitializeUnicastIpAddressEntry(&AddressRow);
    API::WintunGetAdapterLUID(device->Adapter, &AddressRow.InterfaceLuid);
    AddressRow.Address.Ipv4.sin_family = AF_INET;
    AddressRow.Address.Ipv4.sin_addr.S_un.S_addr = std::uint32_t(ip);
    AddressRow.OnLinkPrefixLength = Device::Config::prefix_len; /* This is a /24 network */
    AddressRow.DadState = IpDadStatePreferred;
    DWORD LastError = CreateUnicastIpAddressEntry(&AddressRow);
    if (LastError != ERROR_SUCCESS && LastError != ERROR_OBJECT_ALREADY_EXISTS) {
        LogError(L"Failed to set IP address", LastError);
        goto cleanupAdapter;
    }
    handle = API::WintunStartSession(device->Adapter, 0x400000);
    if (!handle) {
        LastError = LogLastError(L"Failed to create adapter");
        goto cleanupAdapter;
    }
    ReadEvent = API::WintunGetReadWaitEvent(handle);
    if (!ReadEvent) {
        LastError = LogLastError(L"Failed to create ReadEvent");
        goto cleanupAdapter;
    }
    return;

cleanupAdapter:
    API::WintunCloseAdapter(device->Adapter);

}

Session::~Session() {
    API::WintunEndSession(handle);
    API::WintunCloseAdapter(device->Adapter);
}


template<std::size_t N>
async def Session::async_wait(auto &context, HANDLE (&handles)[N]) {
    return boost::asio::co_spawn(context, [&] () -> awaitable<DWORD> {
        co_return WaitForMultipleObjects(N, handles, FALSE, INFINITE);
    }, boost::asio::use_awaitable);
}


async def Session::async_read(boost::asio::streambuf &buf) {
    return boost::asio::co_spawn(receiverContext, [&] () -> awaitable<int> {

        HANDLE WaitHandles[] = { ReadEvent, device->QuitEvent };

        while (true) {
            DWORD PacketSize;
            BYTE *Packet = API::WintunReceivePacket(handle, &PacketSize);
            if (Packet) {
                auto p = buf.prepare(PacketSize).data();
                std::memcpy(p, Packet, PacketSize);
                buf.commit(PacketSize);
                API::WintunReleaseReceivePacket(handle, Packet);
                co_return PacketSize;
            } else {
                DWORD LastError = GetLastError();
                DWORD ret;
                switch (LastError) {
                    case ERROR_NO_MORE_ITEMS:
                        switch (ret = co_await async_wait(receiverContext, WaitHandles)) {
                            case WAIT_OBJECT_0:
                                continue;
                            case WAIT_OBJECT_0 + 1:
                                throw CancelledError();
                            default:
                                throw std::runtime_error(std::format("WaitForMultipleObjects failed {}", ret));
                        }
                        co_return 0;
                    default:
                        LogError(L"Packet read failed", LastError);
                        throw std::runtime_error(std::format("Packet read failed {}", LastError));
                }
            }
        }
    }, boost::asio::use_awaitable);
}



async def Session::async_send(boost::asio::streambuf &buf) {
    return boost::asio::co_spawn(senderContext, [&] () -> awaitable<void> {
        auto p = buf.data();
        BYTE *Packet = API::WintunAllocateSendPacket(handle, p.size());
        if (Packet) {
            std::memcpy(Packet, p.data(), p.size());
            API::WintunSendPacket(handle, Packet);
            buf.consume(p.size());
        }
        else if (GetLastError() != ERROR_BUFFER_OVERFLOW)
            throw std::runtime_error(std::format("Packet write failed {}", LogLastError(L"Packet write failed")));
        co_return;
    }, boost::asio::use_awaitable);
}



}
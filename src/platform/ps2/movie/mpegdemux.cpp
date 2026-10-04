#include "mpeg.h"

// libmpeg's PSS demultiplexer: an MPEG-2 program stream's packs and their PES packets, each handed to its stream's callback

extern "C"
{
    extern const char g_MpegPackHeaderInPes[] RETAIL(D_003075A0);
    // The bits the PES header's optional fields take by their 4 flags (ES rate, DSM trick mode, additional copy info, CRC)
    extern const u8 g_PesOptionalBits[16] RETAIL(D_00307590);
    // sceMpegAddStrCallback's stream types (M2V, IPU, PCM, ADPCM, DATA, MPEG audio, AC-3, LPCM, DTS, SDDS)
    extern const MpegStreamType g_MpegStreamTypes[MpegStreamTypes] RETAIL(D_002E8198);
}

namespace
{
// The bits loaded into the window until it holds at least 57 (the bytes taken from the ring)
void Refill(PssReader* reader)
{
    while (reader->loaded < 57)
    {
        reader->window |= static_cast<u64>(*reader->next) << (56 - reader->loaded);
        reader->next++;
        if (reader->next >= reader->ringEnd)
        {
            reader->next = reader->ringStart;
        }

        reader->loaded += 8;
    }
}

u32 Peek(PssReader* reader, s32 bits)
{
    return static_cast<u32>(reader->window >> (64 - bits));
}

void Skip(PssReader* reader, s32 bits)
{
    reader->window <<= bits;
    reader->loaded -= bits;
    Refill(reader);
    reader->position += bits;
}

u32 Read(PssReader* reader, s32 bits)
{
    u32 value = Peek(reader, bits);
    Skip(reader, bits);
    return value;
}

// The reader moved on to a byte position (in bits)
void SeekTo(PssReader* reader, u64 position)
{
    u8* next = reader->start + static_cast<s32>(position >> 3);
    reader->window = 0;
    reader->loaded = 0;
    reader->position = position;
    reader->next = next;
    if (next >= reader->ringEnd)
    {
        reader->next = next - reader->ringSize;
    }

    Refill(reader);
}

void SkipBytes(PssReader* reader, s32 bytes)
{
    SeekTo(reader, reader->position + static_cast<s32>(bytes << 3));
}

u32 StartCode(PssReader* reader)
{
    return static_cast<u32>(reader->window >> 32);
}

bool AtStartCode(PssReader* reader)
{
    return reader->window >> 40 == 1;
}

// A PES header's PTS or DTS after its 4 bits of prefix: 33 bits in parts of 3, 15 and 15, each followed by a marker
u64 ReadTimeStamp(PssReader* reader)
{
    u32 high = Read(reader, 3);
    Skip(reader, 1);
    u32 middle = Read(reader, 15);
    Skip(reader, 1);
    u32 low = Read(reader, 15);
    Skip(reader, 1);
    return static_cast<u64>(high >> 2) << 32 | static_cast<u32>(high << 30 | middle << 15 | low);
}

// Where a part of the packet is in the ring, by its position in bits
u8* InRing(PssReader* reader, s32 position)
{
    u8* data = reader->start + (position >> 3);
    if (data >= reader->ringEnd)
    {
        data -= reader->ringSize;
    }

    return data;
}

s32 CallStream(Mpeg* mpeg, PssReader* reader, PesPacket* packet, MpegCallback function, void* user, u32 globalPointer)
{
    MpegStreamData data;
    data.type = MpegCallbackStream;
    data.header = InRing(reader, packet->position);
    data.data = InRing(reader, packet->dataPosition);
    data.pts = packet->pts;
    data.length = packet->dataLength;
    data.dts = packet->dts;
    return CallWithGlobalPointer(globalPointer, function, mpeg, &data, user);
}
}

namespace Libmpeg
{
s32 SkipSystemHeader(PssReader* reader)
{
    // The start code, header_length, the rates and bounds and flags
    Skip(reader, 56);
    Skip(reader, 40);
    // Each stream's P-STD buffer bound
    while (reader->window >> 63 == 1)
    {
        Skip(reader, 24);
    }

    return 1;
}

s32 ReadPackHeader(PssReader* reader, PssPack* pack)
{
    // The start code and '01'
    Skip(reader, 34);
    u32 high = Read(reader, 3);
    Skip(reader, 1);
    u32 middle = Read(reader, 15);
    Skip(reader, 1);
    u32 low = Read(reader, 15);
    Skip(reader, 1);
    pack->scrExtension = Read(reader, 9);
    // A marker, program_mux_rate, two markers and the reserved bits
    Skip(reader, 30);
    u32 stuffing = Read(reader, 3);
    pack->scr = high << 30 | middle << 15 | low;
    pack->scrHigh = high >> 2;
    for (u32 i = 0; i < stuffing; i++)
    {
        Skip(reader, 8);
    }

    if (StartCode(reader) == SystemHeaderStartCode)
    {
        pack->hasSystemHeader = 1;
        SkipSystemHeader(reader);
    }
    else
    {
        pack->hasSystemHeader = 0;
    }

    return 1;
}

s32 ReadPesPacket(MpegSystem* sys, PssReader* reader, PesPacket* packet)
{
    packet->position = static_cast<s32>(reader->position);
    Skip(reader, 24);
    packet->streamId = static_cast<u64>(Read(reader, 8)) << 32;
    packet->length = Read(reader, 16);
    packet->pts = -1;
    packet->dts = -1;
    u64 streamId = packet->streamId;
    if (streamId != ProgramStreamMap && streamId != PaddingStream && streamId != PrivateStream2 && streamId != EcmStream &&
        streamId != EmmStream && streamId != ProgramStreamDirectory && streamId != DsmccStream &&
        streamId != H2221TypeEStream)
    {
        // '10'
        Skip(reader, 2);
        packet->scramblingControl = Read(reader, 2);
        // The priority, data alignment, copyright and original flags
        Skip(reader, 4);
        u32 ptsDtsFlags = Read(reader, 2);
        u32 escrFlag = Read(reader, 1);
        u32 optionalFlags = Read(reader, 4);
        u32 extensionFlag = Read(reader, 1);
        s32 headerLength = Read(reader, 8);
        s64 headerStart = static_cast<s32>(reader->position);
        if ((ptsDtsFlags & 0x2) != 0)
        {
            Skip(reader, 4);
            packet->pts = ReadTimeStamp(reader);
        }

        if (ptsDtsFlags == 0x3)
        {
            Skip(reader, 4);
            packet->dts = ReadTimeStamp(reader);
        }

        if (escrFlag == 1)
        {
            Skip(reader, 48);
        }

        if (optionalFlags != 0)
        {
            Skip(reader, g_PesOptionalBits[optionalFlags]);
        }

        if (extensionFlag == 1)
        {
            u32 privateDataFlag = Read(reader, 1);
            u32 packHeaderFieldFlag = Read(reader, 1);
            u32 sequenceCounterFlag = Read(reader, 1);
            u32 bufferFlag = Read(reader, 1);
            Skip(reader, 3);
            u32 extension2Flag = Read(reader, 1);
            if (privateDataFlag == 1)
            {
                Skip(reader, 48);
                Skip(reader, 48);
                Skip(reader, 32);
            }

            if (packHeaderFieldFlag == 1)
            {
                Error(sys, g_MpegPackHeaderInPes);
                return 0;
            }

            if (sequenceCounterFlag == 1)
            {
                Skip(reader, 16);
            }

            if (bufferFlag == 1)
            {
                Skip(reader, 16);
            }

            if (extension2Flag == 1)
            {
                Skip(reader, 1);
                u32 fieldLength = Read(reader, 7);
                for (u32 i = 0; i < fieldLength; i++)
                {
                    Skip(reader, 8);
                }
            }
        }

        // The stuffing bytes left of the header
        s32 stuffing = headerLength - static_cast<s32>((reader->position - headerStart) >> 3);
        if (stuffing != 0)
        {
            SkipBytes(reader, stuffing);
        }

        s32 dataLength = packet->length - headerLength - 3;
        packet->dataLength = dataLength;
        packet->dataPosition = static_cast<s32>(reader->position);
        // A private stream's sub-stream: its first 4 bytes, still in the callback's data
        if (packet->streamId == PrivateStream1)
        {
            u32 subStream = Peek(reader, 32);
            Skip(reader, 32);
            dataLength = packet->length - headerLength - 7;
            packet->streamId |= subStream;
        }

        if (dataLength != 0)
        {
            SkipBytes(reader, dataLength);
        }
    }
    else if (streamId == ProgramStreamMap || streamId == PrivateStream2 || streamId == EcmStream || streamId == EmmStream ||
             streamId == ProgramStreamDirectory || streamId == DsmccStream || streamId == H2221TypeEStream)
    {
        s32 length = packet->length;
        if (streamId == PrivateStream2)
        {
            u32 subStream = Peek(reader, 32);
            Skip(reader, 32);
            length -= 4;
            packet->streamId |= subStream;
        }

        if (length != 0)
        {
            SkipBytes(reader, length);
        }
    }
    else if (streamId == PaddingStream && packet->length != 0)
    {
        SkipBytes(reader, packet->length);
    }

    return 1;
}
}

using namespace Libmpeg;

s32 sceMpegDemuxPssRing(Mpeg* mpeg, u8* pss, s32 size, u8* ringStart, s32 ringSize)
{
    MpegSystem* sys = mpeg->sys;
    MpegStreamCallback* callbacks = sys->streamCallbacks;
    PssReader reader;
    reader.start = pss;
    reader.next = pss;
    reader.window = 0;
    reader.position = 0;
    reader.loaded = 0;
    reader.ringStart = ringStart;
    reader.ringEnd = ringStart + ringSize;
    reader.ringSize = ringSize;
    Refill(&reader);
    reader.position = 0;
    // The callback of private streams no other callback takes
    MpegCallback anyPrivate = nullptr;
    void* anyPrivateUser = nullptr;
    u32 anyPrivateGlobalPointer = 0;
    for (s32 i = 0; i < sys->streamCallbackCount; i++)
    {
        if (callbacks[i].id == MpegAnyPrivateStream)
        {
            anyPrivateGlobalPointer = callbacks[i].globalPointer;
            anyPrivate = callbacks[i].function;
            anyPrivateUser = callbacks[i].user;
        }

        if (anyPrivate != nullptr)
        {
            break;
        }
    }

    // Packets go to their callbacks until one refuses (returns 0), the data ends or something else than a packet comes
    s32 bits = size << 3;
    s32 taken = 1;
    s32 used = 0;
    PssPack pack;
    while (true)
    {
        if (StartCode(&reader) == PackStartCode)
        {
            ReadPackHeader(&reader, &pack);
        }

        while (AtStartCode(&reader) && StartCode(&reader) != PackStartCode && StartCode(&reader) != ProgramEndCode &&
               static_cast<s32>(reader.position) < bits && taken != 0)
        {
            ReadPesPacket(sys, &reader, &pack.packet);
            if (bits < static_cast<s32>(reader.position))
            {
                continue;
            }

            s32 i;
            for (i = 0; i < sys->streamCallbackCount; i++)
            {
                MpegStreamCallback* callback = &callbacks[i];
                if ((pack.packet.streamId & callback->mask) == callback->id)
                {
                    taken = CallStream(mpeg, &reader, &pack.packet, callback->function, callback->user,
                                       callback->globalPointer);
                    break;
                }
            }

            if (i == sys->streamCallbackCount && anyPrivate != nullptr)
            {
                taken = CallStream(mpeg, &reader, &pack.packet, anyPrivate, anyPrivateUser, anyPrivateGlobalPointer);
            }

            if (taken != 0)
            {
                used = static_cast<s32>(reader.position >> 3);
            }
        }

        if (bits < static_cast<s32>(reader.position) || StartCode(&reader) != PackStartCode)
        {
            return used;
        }
    }
}

s32 sceMpegDemuxPss(Mpeg* mpeg, u8* pss, s32 size)
{
    return sceMpegDemuxPssRing(mpeg, pss, size, nullptr, -1);
}

void* sceMpegAddStrCallback(Mpeg* mpeg, s32 type, s32 channel, MpegCallback callback, void* user)
{
    MpegSystem* sys = mpeg->sys;
    MpegStreamCallback* callbacks = sys->streamCallbacks;
    MpegCallback previous = nullptr;
    // The stream's ID with the channel in the byte below its mask's top one
    u64 id = 0;
    if (static_cast<u32>(type) < MpegStreamTypes)
    {
        const MpegStreamType* streamType = &g_MpegStreamTypes[type];
        u64 channelBits = static_cast<u64>(static_cast<s64>(channel));
        switch (streamType->mask)
        {
        case 0xFFFF000000ull:
            id = streamType->id | channelBits << 24;
            break;
        case 0xFF00000000ull:
            id = streamType->id | channelBits << 32;
            break;
        case 0xFFFFFFFFFFull:
            id = streamType->id | channelBits;
            break;
        }
    }

    s32 i;
    for (i = 0; i < sys->streamCallbackCount; i++)
    {
        if (callbacks[i].id == id)
        {
            previous = callbacks[i].function;
            break;
        }
    }

    if (i < MpegStreamCallbacks)
    {
        callbacks[i].id = id;
        callbacks[i].function = callback;
        callbacks[i].user = user;
        callbacks[i].mask = g_MpegStreamTypes[type].mask;
        callbacks[i].globalPointer = GlobalPointer();
        // Retail bug: a stream's callback set again replaces the old one and still counts as another, the callback past the
        // last one (zeros, which take every packet) with it
        sys->streamCallbackCount++;
    }

    return reinterpret_cast<void*>(previous);
}

#pragma once

// Returns the offset from `filedata` to a newline character (`\r` or `\n`).
// Returns STRINGS_EOF if reaches the end.
// Returns STRINGS_ZEROBYTE if finds `\0`.
const i32 STRINGS_EOF = -1;
const i32 STRINGS_ZEROBYTE = -2;

i32 Find_Newline(const u8* buffer, const i32 size)
{
    assert(size > 0);

    i32 offset = 0;
    while (offset < size) {
        u8 character = *(buffer + offset);

        if (character == '\0')
            return STRINGS_ZEROBYTE;

        if (character == '\n' || character == '\r')
            return offset;

        offset++;
    }

    return STRINGS_EOF;
}

i32 Find_Newline_Or_EOF(const u8* buffer, const i32 size)
{
    assert(size > 0);

    i32 offset = 0;
    while (offset < size) {
        u8 character = *(buffer + offset);

        if (character == '\0' || character == '\n' || character == '\r')
            return offset;

        offset++;
    }

    return offset;
}

// Returns the offset from `filedata` to a newline character (not `\r` and not `\n`).
// Returns STRINGS_EOF if reaches the end.
// Returns STRINGS_ZEROBYTE if finds `\0`.
i32 Find_Not_Newline(const u8* buffer, const i32 size)
{
    assert(size > 0);

    i32 offset = 0;
    while (offset < size) {
        u8 character = *(buffer + offset);

        if (character == '\0')
            return STRINGS_ZEROBYTE;

        if (character != '\n' && character != '\r')
            return offset;

        offset++;
    }

    return STRINGS_EOF;
}

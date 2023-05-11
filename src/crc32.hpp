// Copyright 2023 ShenMian
// License(Apache-2.0)

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

constexpr std::array<uint32_t, 256> generate_table()
{
	std::array<uint32_t, 256> table;

	const uint32_t polynomial = 0xEDB88320;
	for(uint32_t i = 0; i < 256; i++)
	{
		uint32_t c = i;
		for(size_t j = 0; j < 8; j++)
		{
			if(c & 1)
				c = polynomial ^ (c >> 1);
			else
				c >>= 1;
		}
		table[i] = c;
	}
	return table;
}

constexpr uint32_t crc32(uint32_t initial, const void* buf, size_t len)
{
	constexpr auto table = generate_table();

	uint32_t       c = initial ^ 0xFFFFFFFF;
	const uint8_t* u = static_cast<const uint8_t*>(buf);
	for(size_t i = 0; i < len; i++)
		c = table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFF;
}

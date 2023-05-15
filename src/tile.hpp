// Copyright 2023 ShenMian
// License(Apache-2.0)

#pragma once

enum Tile : uint8_t
{
	Floor  = 1 << 0,
	Wall   = 1 << 1,
	Crate  = 1 << 2,
	Target = 1 << 3,
	Player = 1 << 4,

	Deadlocked     = 1 << 5,
	PlayerMoveable = 1 << 6,
	CrateMoveable  = 1 << 7,

	Unmovable = Wall | Deadlocked,
};

// Copyright 2023 ShenMian
// License(Apache-2.0)

#pragma once

#include <SFML/Graphics.hpp>

struct Material
{
	Material()
	{
		floor_texture.loadFromFile("img/floor.png");
		wall_texture.loadFromFile("img/wall.png");
		crate_texture.loadFromFile("img/crate.png");
		target_texture.loadFromFile("img/target.png");

		player_up_texture.loadFromFile("img/player_up.png");
		player_down_texture.loadFromFile("img/player_down.png");
		player_left_texture.loadFromFile("img/player_left.png");
		player_right_texture.loadFromFile("img/player_right.png");

		floor_texture.setSmooth(true);
		wall_texture.setSmooth(true);
		crate_texture.setSmooth(true);
		target_texture.setSmooth(true);

		player_up_texture.setSmooth(true);
		player_down_texture.setSmooth(true);
		player_left_texture.setSmooth(true);
		player_right_texture.setSmooth(true);
	}

	sf::Texture floor_texture;
	sf::Texture wall_texture;
	sf::Texture crate_texture;
	sf::Texture target_texture;

	sf::Texture player_up_texture;
	sf::Texture player_down_texture;
	sf::Texture player_left_texture;
	sf::Texture player_right_texture;
};
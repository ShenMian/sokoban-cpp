// Copyright 2023 ShenMian
// License(Apache-2.0)

#pragma once

#include <SFML/Graphics.hpp>
#include <filesystem>

struct Material
{
	Material(const std::filesystem::path& path) { load_from_file(path); }

	void load_from_file(const std::filesystem::path& path)
	{
		texture.loadFromFile(path.string());
		texture.setSmooth(true);
	}

	void set_texture_floor(sf::Sprite& sprite) const
	{
		sprite.setTexture(texture);
		sprite.setTextureRect({0 * tile_size, 0 * tile_size, tile_size, tile_size});
	}

	void set_texture_wall(sf::Sprite& sprite) const
	{
		sprite.setTexture(texture);
		sprite.setTextureRect({1 * tile_size, 0 * tile_size, tile_size, tile_size});
	}

	void set_texture_crate(sf::Sprite& sprite) const
	{
		sprite.setTexture(texture);
		sprite.setTextureRect({2 * tile_size, 0 * tile_size, tile_size, tile_size});
	}

	void set_texture_target(sf::Sprite& sprite) const
	{
		sprite.setTexture(texture);
		sprite.setTextureRect({3 * tile_size, 0 * tile_size, tile_size, tile_size});
	}

	void set_texture_player(sf::Sprite& sprite, const sf::Vector2i& direction) const
	{
		if(direction.y == -1)
			set_texture_player_up(sprite);
		else if(direction.y == 1)
			set_texture_player_down(sprite);
		else if(direction.x == -1)
			set_texture_player_left(sprite);
		else if(direction.x == 1)
			set_texture_player_right(sprite);
	}

	void set_texture_player_up(sf::Sprite& sprite) const
	{
		sprite.setTexture(texture);
		sprite.setTextureRect({0 * tile_size, 1 * tile_size, tile_size, tile_size});
	}

	void set_texture_player_right(sf::Sprite& sprite) const
	{
		sprite.setTexture(texture);
		sprite.setTextureRect({1 * tile_size, 1 * tile_size, tile_size, tile_size});
	}

	void set_texture_player_down(sf::Sprite& sprite) const
	{
		sprite.setTexture(texture);
		sprite.setTextureRect({2 * tile_size, 1 * tile_size, tile_size, tile_size});
	}

	void set_texture_player_left(sf::Sprite& sprite) const
	{
		sprite.setTexture(texture);
		sprite.setTextureRect({3 * tile_size, 1 * tile_size, tile_size, tile_size});
	}

	sf::Texture texture;
	const int   tile_size = 64;
};
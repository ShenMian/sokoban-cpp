// Copyright 2023 ShenMian
// License(Apache-2.0)

#include <SFML/Graphics.hpp>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace std
{

template <class T>
struct hash<sf::Vector2<T>>
{
	std::size_t operator()(const sf::Vector2<T>& v) const
	{
		using std::hash;

		// Compute individual hash values for first
		// and second. Combine them using the Boost-func

		std::size_t tmp0 = hash<T>()(v.x);
		std::size_t tmp1 = hash<T>()(v.y);

		tmp0 ^= tmp1 + 0x9e3779b9 + (tmp0 << 6) + (tmp0 >> 2);

		return tmp0;
	}
};

} // namespace std

char to_char(const sf::Vector2i& direction)
{
	if(direction == sf::Vector2i(0, -1))
		return 'u';
	if(direction == sf::Vector2i(0, 1))
		return 'd';
	if(direction == sf::Vector2i(-1, 0))
		return 'l';
	if(direction == sf::Vector2i(1, 0))
		return 'r';
	throw std::invalid_argument("incorrect direction");
}

enum Tile : uint8_t
{
	Floor  = 1 << 0,
	Wall   = 1 << 1,
	Crate  = 1 << 2,
	Target = 1 << 3,
	Player = 1 << 4,
};

class Map
{
public:
	Map()
	{
		floor_texture_.loadFromFile("img/floor.png");
		wall_texture_.loadFromFile("img/wall.png");
		crate_texture_.loadFromFile("img/crate.png");
		target_texture_.loadFromFile("img/target.png");

		player_up_texture_.loadFromFile("img/player_up.png");
		player_down_texture_.loadFromFile("img/player_down.png");
		player_left_texture_.loadFromFile("img/player_left.png");
		player_right_texture_.loadFromFile("img/player_right.png");
	}

	void render(sf::RenderWindow& window) const
	{
		const auto window_size   = sf::Vector2f(window.getSize());
		const auto window_center = window_size / 2.f;

		auto         tile_size = sf::Vector2f(wall_texture_.getSize());
		sf::Vector2f map_size  = {tile_size.x * size().x, tile_size.y * size().y};

		// 调整地图缩放比例, 确保地图能被完整的展示在窗口中
		float scale = 1.f;
		scale       = std::min(window_size.x / map_size.x, scale);
		scale       = std::min(window_size.y / map_size.y, scale);

		map_size *= scale;
		tile_size *= scale;

		const auto offset = window_center - map_size / 2.f;
		for(size_t y = 0; y < size().y; y++)
		{
			for(size_t x = 0; x < size().x; x++)
			{
				sf::Sprite sprite;
				sprite.setScale(scale, scale);
				sprite.setOrigin(-offset);
				sprite.setPosition(x * tile_size.x, y * tile_size.y);

				auto tiles = map_[x][y];

				if(tiles & Tile::Floor)
				{
					sprite.setTexture(floor_texture_);
					window.draw(sprite);
					tiles &= ~Tile::Floor;
				}

				switch(tiles)
				{
				case Tile::Wall:
					sprite.setTexture(wall_texture_);
					break;

				case Tile::Target:
					sprite.setTexture(target_texture_);
					break;

				case Tile::Crate:
					sprite.setTexture(crate_texture_);
					break;

				case Tile::Target | Tile::Crate:
					sprite.setColor(sf::Color(180, 180, 180));
					sprite.setTexture(crate_texture_);
					break;

				case Tile::Target | Tile::Player:
					sprite.setTexture(target_texture_);
					window.draw(sprite);
					sprite.setTexture(*player_texture_);
					break;

				case Tile::Player:
					sprite.setTexture(*player_texture_);
					break;
				}
				window.draw(sprite);
			}
		}
	}

	void load(const std::filesystem::path& path)
	{
		player_texture_ = &player_down_texture_;
		map_.clear();
		metadata_.clear();
		crate_positions_.clear();
		target_positions_.clear();
		movements_.clear();

		assert(std::filesystem::exists(path));
		std::ifstream file(path);
		assert(file);

		size_t rows = 0, cols = 0;
		for(std::string line; std::getline(file, line);)
		{
			if(line.empty() || line.front() == ';' || line.front() == '[' || line.find(":") != std::string::npos)
				continue;
			rows++;
			cols = std::max(line.size(), cols);
		}

		map_.resize(cols, std::vector<uint8_t>(rows));

		file.clear();
		file.seekg(0);

		std::string line;
		for(unsigned int y = 0; std::getline(file, line);)
		{
			if(line.empty() || line.front() == ';' || line.front() == '[')
				continue;
			const auto it = line.find(":");
			if(it != std::string::npos)
			{
				metadata_.emplace(line.substr(0, it), line.substr(it + 1));
				continue;
			}
			for(unsigned int x = 0; x < static_cast<unsigned int>(line.size()); x++)
			{
				switch(line[x])
				{
				case ' ':
				case '-':
					break;

				case '#':
					map_[x][y] |= Tile::Wall;
					break;

				case 'X':
				case '$':
					map_[x][y] |= Tile::Crate;
					crate_positions_.emplace(x, y);
					break;

				case '.':
					map_[x][y] |= Tile::Target;
					target_positions_.emplace(x, y);
					break;

				case '@':
					map_[x][y] |= Tile::Player;
					player_position_ = sf::Vector2i(x, y);
					break;

				case '+':
					map_[x][y] |= Tile::Player | Tile::Target;
					player_position_ = sf::Vector2i(x, y);
					break;

				case '*':
					map_[x][y] |= Tile::Crate | Tile::Target;
					break;

				default:
					assert(false);
				}
			}
			y++;
		}
		fill(player_position_, Tile::Floor);
	}

	void move(const sf::Vector2i& direction)
	{
		if(direction.y == -1)
			player_texture_ = &player_up_texture_;
		else if(direction.y == 1)
			player_texture_ = &player_down_texture_;
		else if(direction.x == -1)
			player_texture_ = &player_left_texture_;
		else if(direction.x == 1)
			player_texture_ = &player_right_texture_;

		const auto player_next_pos = player_position_ + direction;
		if(map_[player_next_pos.x][player_next_pos.y] & Tile::Wall)
			return;
		if(map_[player_next_pos.x][player_next_pos.y] & Tile::Crate)
		{
			const auto crate_next_pos = player_next_pos + direction;
			if(map_[crate_next_pos.x][crate_next_pos.y] & (Tile::Wall | Tile::Crate))
				return;

			map_[player_next_pos.x][player_next_pos.y] &= ~Tile::Crate;
			crate_positions_.erase(player_next_pos);
			map_[crate_next_pos.x][crate_next_pos.y] |= Tile::Crate;
			crate_positions_.insert(crate_next_pos);
		}

		map_[player_position_.x][player_position_.y] &= ~Tile::Player;
		map_[player_next_pos.x][player_next_pos.y] |= Tile::Player;
		player_position_ = player_next_pos;

		movements_.push_back(to_char(direction));
	}

	void play(std::string movements, std::chrono::milliseconds delay = std::chrono::milliseconds(200))
	{
		for(const auto movement : movements)
		{
			switch(std::tolower(movement))
			{
			case 'u':
				move(sf::Vector2i(0, -1));
				break;

			case 'd':
				move(sf::Vector2i(0, 1));
				break;

			case 'l':
				move(sf::Vector2i(-1, 0));
				break;

			case 'r':
				move(sf::Vector2i(1, 0));
				break;

			default:
				assert(false);
			}
			std::this_thread::sleep_for(delay);
		}
	}

	bool is_passed() const noexcept { return crate_positions_ == target_positions_; }

	sf::Vector2u size() const
	{
		return {static_cast<unsigned int>(map_.size()), static_cast<unsigned int>(map_[0].size())};
	}

	void fill(const sf::Vector2i& position, Tile tile)
	{
		map_[position.x][position.y] |= tile;
		const sf::Vector2i directions[] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
		for(const auto offset : directions)
		{
			const auto pos = position + offset;
			if((map_[pos.x][pos.y] & (Tile::Wall | tile)) == 0)
				fill(pos, tile);
		}
	}

	const std::string get_metadata(const std::string& key) const noexcept
	{
		if(!metadata_.contains(key))
			return "";
		return metadata_.at(key);
	};
	const auto& get_movements() const noexcept { return movements_; }

private:
	std::vector<std::vector<uint8_t>>            map_;
	std::unordered_map<std::string, std::string> metadata_;

	sf::Texture  floor_texture_, wall_texture_, crate_texture_, crate_green_texture_, target_texture_;
	sf::Texture  player_up_texture_, player_down_texture_, player_left_texture_, player_right_texture_;
	sf::Texture* player_texture_;

	sf::Vector2i player_position_;

	std::unordered_set<sf::Vector2i> crate_positions_;
	std::unordered_set<sf::Vector2i> target_positions_;

	std::string movements_;
};

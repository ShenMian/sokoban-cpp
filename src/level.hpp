// Copyright 2023 ShenMian
// License(Apache-2.0)

#pragma once

#include "crc32.hpp"
#include "material.hpp"
#include "tile.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template <class T>
struct std::hash<sf::Vector2<T>>
{
	std::size_t operator()(const sf::Vector2<T>& v) const
	{
		std::size_t       tmp0 = std::hash<T>()(v.x);
		const std::size_t tmp1 = std::hash<T>()(v.y);
		tmp0 ^= tmp1 + 0x9e3779b9 + (tmp0 << 6) + (tmp0 >> 2);
		return tmp0;
	}
};

inline char direction_to_movement(const sf::Vector2i& dir)
{
	if(dir == sf::Vector2i(0, -1))
		return 'u';
	else if(dir == sf::Vector2i(0, 1))
		return 'd';
	else if(dir == sf::Vector2i(-1, 0))
		return 'l';
	else if(dir == sf::Vector2i(1, 0))
		return 'r';
	throw std::invalid_argument("invalid direction");
}

inline sf::Vector2i movement_to_direction(char move)
{
	switch(std::tolower(move))
	{
	case 'u':
		return {0, -1};

	case 'd':
		return {0, 1};

	case 'l':
		return {-1, 0};

	case 'r':
		return {1, 0};
	}
	throw std::invalid_argument("invalid movement");
}

inline sf::Vector2i rotate_direction(sf::Vector2i dir, int rotation)
{
	if(rotation > 0)
		for(int i = 0; i < rotation; i++)
			dir = {-dir.y, dir.x};
	else
		for(int i = 0; i < std::abs(rotation); i++)
			dir = {dir.y, -dir.x};
	return dir;
}

inline char rotate_movement(char move, int rotation)
{
	if(std::islower(move))
		return direction_to_movement(rotate_direction(movement_to_direction(move), rotation));
	else
		return std::toupper(direction_to_movement(rotate_direction(movement_to_direction(move), rotation)));
}

class Level
{
public:
	/**
	 * @brief 构造函数.
	 *
	 * @param data XSB 格式地图数据.
	 */
	Level(const std::string& data)
	{
		std::string  map, metadata;
		sf::Vector2i size;

		auto to_lowercase = [](auto str) {
			std::transform(str.cbegin(), str.cend(), str.begin(), [](auto c) { return std::tolower(c); });
			return str;
		};

		std::istringstream stream(data);
		for(std::string line; std::getline(stream, line);)
		{
			if(line.front() == ';')
				continue;

			if(line.find(":") != std::string::npos)
			{
				if(to_lowercase(line.substr(0, 8)) == "comment:")
				{
					do
					{
						metadata += line + '\n';
						if(!stream)
							throw std::runtime_error("unexpected end of stream");
						std::getline(stream, line);
					} while(to_lowercase(line.substr(0, 12)) != "comment-end:");
				}
				metadata += line + '\n';
				continue;
			}

			map += line + '\n';

			size.x = std::max(static_cast<int>(line.size()), size.x);
			size.y++;
		}

		*this = Level(map, size, metadata);
	}

	/**
	 * @brief 构造函数.
	 *
	 * @param ascii_map      XSB 格式地图数据.
	 * @param size     地图大小.
	 * @param metadata XSB 格式元数据.
	 */
	Level(const std::string& map, const sf::Vector2i& size, const std::string& metadata)
	{
		parse_map(map, size);
		parse_metadata(metadata);
	}

	/**
	 * @brief 移动角色.
	 *
	 * @param movements LURD 格式移动记录.
	 * @param interval  移动间隔.
	 */
	void play(const std::string& movements, std::chrono::milliseconds interval = std::chrono::milliseconds(0))
	{
		for(const auto movement : movements)
		{
			const auto direction       = movement_to_direction(movement);
			const auto player_next_pos = player_position_ + direction;
			if(at(player_next_pos) & Tile::Wall)
				continue;

			if(at(player_next_pos) & Tile::Crate)
			{
				const auto crate_next_pos = player_next_pos + direction;
				if(at(crate_next_pos) & (Tile::Wall | Tile::Crate))
					continue;

				at(player_next_pos) &= ~Tile::Crate;
				at(crate_next_pos) |= Tile::Crate;
				crate_positions_.erase(player_next_pos);
				crate_positions_.insert(crate_next_pos);
				check_deadlock(crate_next_pos);

				at(player_position_) &= ~Tile::Player;
				at(player_next_pos) |= Tile::Player;
				player_position_ = player_next_pos;

				movements_ += rotate_movement(std::toupper(movement), -rotation_);
			}
			else
			{
				at(player_position_) &= ~Tile::Player;
				at(player_next_pos) |= Tile::Player;
				player_position_ = player_next_pos;

				movements_ += rotate_movement(std::tolower(movement), -rotation_);
			}
			std::this_thread::sleep_for(interval);
		}
	}

	/**
	 * @brief 撤回上一步操作.
	 */
	void undo()
	{
		if(movements_.empty())
			return;

		const auto last_direction = movement_to_direction(get_last_rotated_movement());
		if(std::isupper(movements_.back()))
		{
			// 拉箱子
			const auto crate_pos = player_position_ + last_direction;
			at(crate_pos) &= ~Tile::Crate;
			at(player_position_) |= Tile::Crate;
			crate_positions_.erase(crate_pos);
			crate_positions_.insert(player_position_);
			refresh_deadlocks();
		}
		const auto player_last_pos = player_position_ - last_direction;
		at(player_position_) &= ~Tile::Player;
		at(player_last_pos) |= Tile::Player;
		player_position_ = player_last_pos;

		movements_.pop_back();
	}

	/**
	 * @brief 还原至最初状态.
	 */
	void reset()
	{
		clear(Tile::Deadlocked | Tile::PlayerMovable | Tile::CrateMovable);
		while(!movements_.empty())
			undo();
		while(rotation_)
			rotate();
	}

	/**
	 * @brief 是否通关.
	 *
	 * @return true  已通关.
	 * @return false 未通关.
	 */
	bool passed() const noexcept { return crate_positions_ == target_positions_; }

	/**
	 * @brief 渲染地图.
	 *
	 * @param target   渲染目标.
	 * @param material 材质.
	 */
	void render(sf::RenderTarget& target, const Material& material) const
	{
		const sf::Vector2i player_dir = get_player_direction();

		// TODO: 需要重构, 可读性较差, 非必要的重复计算
		const auto target_size   = sf::Vector2f(target.getSize());
		const auto target_center = target_size / 2.f;

		const auto origin_tile_size =
		    sf::Vector2f(static_cast<float>(material.tile_size), static_cast<float>(material.tile_size));
		const auto origin_map_size = sf::Vector2f(origin_tile_size.x * size().x, origin_tile_size.y * size().y);

		const auto scale     = std::min({target_size.x / origin_map_size.x, target_size.y / origin_map_size.y, 1.f});
		const auto tile_size = origin_tile_size * scale;
		const auto map_size  = origin_map_size * scale;

		const auto offset = target_center - map_size / 2.f;
		for(int y = 0; y < size().y; y++)
		{
			for(int x = 0; x < size().x; x++)
			{
				sf::Sprite sprite;
				sprite.setScale(scale, scale);
				sprite.setPosition(x * tile_size.x + offset.x, y * tile_size.y + offset.y);

				auto tiles = at(x, y);

				if(tiles & Tile::Floor)
				{
					material.set_texture(sprite, Tile::Floor);
					target.draw(sprite);
					tiles &= ~Tile::Floor;
				}

				switch(tiles & ~(Tile::PlayerMovable | Tile::CrateMovable))
				{
				case Tile::Wall:
					material.set_texture(sprite, Tile::Wall);
					break;

				case Tile::Target:
					material.set_texture(sprite, Tile::Target);
					break;

				case Tile::Crate:
					material.set_texture(sprite, Tile::Crate);
					break;

				case Tile::Target | Tile::Crate:
				case Tile::Target | Tile::Crate | Tile::Deadlocked:
					sprite.setColor(sf::Color(0, 255, 0));
					material.set_texture(sprite, Tile::Crate);
					break;

				case Tile::Target | Tile::Player:
					material.set_texture(sprite, Tile::Target);
					target.draw(sprite);
					material.set_texture_player(sprite, player_dir);
					break;

				case Tile::Crate | Tile::Deadlocked:
					sprite.setColor(sf::Color(255, 0, 0));
					material.set_texture(sprite, Tile::Crate);
					break;

				case Tile::Player:
					material.set_texture_player(sprite, player_dir);
					break;
				}
				target.draw(sprite);

				if(tiles & Tile::CrateMovable)
				{
					material.set_texture(sprite, Tile::Crate);
					sprite.setColor(sf::Color(255, 255, 255, 100));
					target.draw(sprite);
				}
			}
		}
	}

	void transpose()
	{
		std::vector<uint8_t> temp(map_.size());
		for(int n = 0; n < size().x * size().y; n++)
		{
			const int i = n / size().y;
			const int j = n % size().y;
			temp[n]     = map_[size().x * j + i];
		}
		map_ = temp;

		auto transpose = [](auto p) { return sf::Vector2i(p.y, p.x); };

		size_            = transpose(size());
		player_position_ = transpose(player_position_);
		{
			std::unordered_set<sf::Vector2i> temp;
			std::transform(crate_positions_.cbegin(), crate_positions_.cend(), std::inserter(temp, temp.begin()),
			               transpose);
			crate_positions_ = temp;
		}
		{
			std::unordered_set<sf::Vector2i> temp;
			std::transform(target_positions_.cbegin(), target_positions_.cend(), std::inserter(temp, temp.begin()),
			               transpose);
			target_positions_ = temp;
		}
	}

	void rotate()
	{
		transpose();
		flip();

		rotation_ = (rotation_ + 1) % 4;
	}

	void flip()
	{
		for(int y = 0; y < size().y; y++)
			std::reverse(map_.begin() + y * size().x, map_.begin() + (y + 1) * size().x);

		auto flip = [center_x = (size().x - 1) / 2.f](auto pos) {
			if(pos.x < center_x)
				pos.x = static_cast<int>(center_x + std::abs(pos.x - center_x));
			else
				pos.x = static_cast<int>(center_x - std::abs(pos.x - center_x));
			return pos;
		};

		player_position_ = flip(player_position_);
		{
			std::unordered_set<sf::Vector2i> temp;
			std::transform(crate_positions_.cbegin(), crate_positions_.cend(), std::inserter(temp, temp.begin()), flip);
			crate_positions_ = temp;
		}
		{
			std::unordered_set<sf::Vector2i> temp;
			std::transform(target_positions_.cbegin(), target_positions_.cend(), std::inserter(temp, temp.begin()),
			               flip);
			target_positions_ = temp;
		}

		// flipped_ = !flipped_;
	}

	/**
	 * @brief 寻找最短路径.
	 *
	 * @param start  起始点.
	 * @param end    终止点.
	 * @param border 障碍物.
	 *
	 * @return std::vector<sf::Vector2i> 最短路径.
	 */
	std::vector<sf::Vector2i> find_path(const sf::Vector2i& start, const sf::Vector2i& end, uint8_t border)
	{
		struct Node
		{
			sf::Vector2i data;
			long         priority;

			bool operator==(const Node& rhs) const noexcept { return data == rhs.data; }
			bool operator>(const Node& rhs) const noexcept { return priority > rhs.priority; }
		};

		auto manhattan_distance = [](auto a, auto b) {
			return std::abs(static_cast<long>(a.x) - static_cast<long>(b.x)) +
			       std::abs(static_cast<long>(a.y) - static_cast<long>(b.y));
		};

		auto euclidean_distance = [](auto a, auto b) {
			return std::sqrt(std::pow(static_cast<long>(a.x) - static_cast<long>(b.x), 2) +
			                 std::pow(static_cast<long>(a.y) - static_cast<long>(b.y), 2));
		};

		std::priority_queue<Node, std::vector<Node>, std::greater<Node>> queue;
		std::unordered_map<sf::Vector2i, sf::Vector2i>                   came_from;
		std::unordered_map<sf::Vector2i, int>                            cost;

		queue.push({start, 0});
		cost[start] = 0;

		const sf::Vector2i directions[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
		while(!queue.empty())
		{
			const auto [current, _] = queue.top();
			queue.pop();
			if(current == end)
				break;
			for(const auto direction : directions)
			{
				const auto neighbor = current + direction;
				if(at(neighbor) & border)
					continue;

				const auto neighbor_cost = cost[current] + manhattan_distance(neighbor, end);
				if(!cost.contains(neighbor) || neighbor_cost < cost[neighbor])
				{
					cost[neighbor]      = neighbor_cost;
					came_from[neighbor] = current;
					queue.push({neighbor, neighbor_cost});
				}
			}
		}

		auto it = came_from.find(end);
		if(it == came_from.end())
			return {};

		std::vector<sf::Vector2i> path;
		path.emplace_back(end);
		while(it->second != start)
		{
			path.emplace_back(it->second);
			it = came_from.find(it->second);
		}
		path.emplace_back(it->second);
		std::reverse(path.begin(), path.end());
		return path;
	}

	sf::Vector2i to_map_position(sf::Vector2i pos, const sf::RenderWindow& window, const Material& material)
	{
		// TODO: 需要重构, 可读性较差, 非必要的重复计算
		const auto window_size   = sf::Vector2f(window.getSize());
		const auto window_center = window_size / 2.f;

		const auto origin_tile_size =
		    sf::Vector2f(static_cast<float>(material.tile_size), static_cast<float>(material.tile_size));
		const auto origin_map_size = sf::Vector2f(origin_tile_size.x * size().x, origin_tile_size.y * size().y);

		const auto scale     = std::min({window_size.x / origin_map_size.x, window_size.y / origin_map_size.y, 1.f});
		const auto tile_size = origin_tile_size * scale;
		const auto map_size  = origin_map_size * scale;

		const auto offset = window_center - map_size / 2.f;

		pos -= sf::Vector2i(static_cast<int>(std::round(offset.x)), static_cast<int>(std::round(offset.y)));
		return sf::Vector2i(static_cast<int>(pos.x / tile_size.x), static_cast<int>(pos.y / tile_size.y));
	}

	uint8_t& at(const sf::Vector2i& pos)
	{
		if(pos.x < 0 || pos.x >= size_.x || pos.y < 0 && pos.y >= size_.y)
			throw std::out_of_range("");
		return map_[pos.y * size_.x + pos.x];
	}

	uint8_t at(const sf::Vector2i& pos) const
	{
		if(pos.x < 0 || pos.x >= size_.x || pos.y < 0 && pos.y >= size_.y)
			throw std::out_of_range("");
		return map_[pos.y * size_.x + pos.x];
	}

	uint8_t& at(int x, int y) { return at({x, y}); }
	uint8_t  at(int x, int y) const { return at({x, y}); }

	const auto&         map() const noexcept { return map_; };
	const auto&         metadata() const noexcept { return metadata_; }
	const sf::Vector2i& size() const noexcept { return size_; };
	const auto&         movements() const noexcept { return movements_; }
	const auto&         player_position() const noexcept { return player_position_; }

	uint32_t crc32() const noexcept
	{
		// TODO: 计算旋转和镜像共 8 种地图变种的 CRC32
		Level    level(*this);
		uint32_t crc = std::numeric_limits<uint32_t>::max();
		level.reset();
		for(int i = 0; i < 4; i++)
		{
			crc = std::min(::crc32(0, level.map().data(), level.map().size()), crc);
			level.rotate();
		}
		return crc;
	};

	/**
	 * @brief 获取 XSB 格式的地图数据.
	 *
	 * @return std::string XSB 格式的地图数据.
	 */
	std::string ascii_map() const
	{
		std::string map;
		for(int y = 0; y < size().y; y++)
		{
			for(int x = 0; x < size().x; x++)
			{
				switch(at({x, y}) & (Tile::Wall | Tile::Crate | Tile::Target | Tile::Player))
				{
				case Tile::Wall:
					map.push_back('#');
					break;

				case Tile::Crate:
					map.push_back('$');
					break;

				case Tile::Target:
					map.push_back('.');
					break;

				case Tile::Player:
					map.push_back('@');
					break;

				case Tile::Crate | Tile::Target:
					map.push_back('*');
					break;

				case Tile::Player | Tile::Target:
					map.push_back('+');
					break;

				default:
					map.push_back('_');
					break;
				}
			}
			map.push_back('\n');
		}
		return map;
	}

	void fill(const sf::Vector2i& position, uint8_t value, uint8_t border)
	{
		std::vector<sf::Vector2i> vector;
		std::vector<bool>         visited(map_.size(), false);

		vector.emplace_back(position);

		while(!vector.empty())
		{
			const auto pos = vector.back();
			vector.pop_back();
			at(pos) |= value;

			const sf::Vector2i directions[] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
			for(const auto offset : directions)
			{
				const auto new_pos = pos + offset;
				if(!(at(new_pos) & (border | value)) && !visited[new_pos.y * size().x + new_pos.x])
				{
					vector.emplace_back(new_pos);
					visited[new_pos.y * size().x + new_pos.x] = true;
				}
			}
		}
	}

	void clear(uint8_t tiles)
	{
		std::transform(map_.cbegin(), map_.cend(), map_.begin(), [tiles](auto t) { return t & ~tiles; });
	}

	auto calc_crate_movable(const sf::Vector2i& crate_pos)
	{
		// TODO: 记录访问过的箱子位置, 需要记录可推动的位置, 不能只是箱子位置
		struct Node
		{
			sf::Vector2i crate_pos;
			sf::Vector2i player_pos;
		};
		std::vector<Node> visited;

		// FIXME: 不能走回头路, 有回头路会死循环
		std::unordered_map<sf::Vector2i, sf::Vector2i> came_from;
		calc_crate_movable(crate_pos, player_position_, came_from);
		return came_from;
	}

	void calc_crate_movable(const sf::Vector2i& crate_pos, const sf::Vector2i& player_pos,
	                        std::unordered_map<sf::Vector2i, sf::Vector2i>& came_from)
	{
		clear(Tile::PlayerMovable);
		fill(player_pos, Tile::PlayerMovable, Tile::Crate | Tile::Wall);

		const sf::Vector2i directions[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
		for(const auto& direction : directions)
		{
			if(!(at(crate_pos - direction) & Tile::PlayerMovable))
				continue;

			for(auto pos = crate_pos + direction; !(at(pos) & (Tile::Unmovable | Tile::Crate | Tile::CrateMovable));
			    pos += direction)
			{
				if(came_from.contains(pos))
					continue;

				// 防止 came_from 形成闭环
				bool skip = false;
				for(auto prev = crate_pos; came_from.contains(prev);)
				{
					prev = came_from[prev];
					if(prev == pos)
					{
						skip = true;
						break;
					}
				}
				if(skip)
					continue;

				came_from[pos] = crate_pos;
				at(pos) |= Tile::CrateMovable;

				at(pos - direction) &= ~Tile::Crate;
				at(pos) |= Tile::Crate;

				calc_crate_movable(pos, pos - direction, came_from);

				at(pos) &= ~Tile::Crate;
				at(pos - direction) |= Tile::Crate;

				clear(Tile::PlayerMovable);
				fill(player_pos, Tile::PlayerMovable, Tile::Crate | Tile::Wall);
			}
		}
	}

	/**
	 * @brief 从 XSB 文件加载关卡.
	 *
	 * @param path XSB 文件路径.
	 *
	 * @return std::vector<Level> 从文件中加载的关卡.
	 */
	static std::vector<Level> load(const std::filesystem::path& path)
	{
		if(!exists(path))
			throw std::runtime_error("file does not exist");
		if(path.extension() != ".txt" && path.extension() != ".xsb")
			throw std::runtime_error("file format not supported");

		std::ifstream file(path);
		if(!file)
			throw std::runtime_error("failed to open file");

		auto to_lowercase = [](auto str) {
			std::transform(str.cbegin(), str.cend(), str.begin(), [](auto c) { return std::tolower(c); });
			return str;
		};

		std::vector<Level> levels;
		while(!file.eof())
		{
			std::string data;
			for(std::string line; std::getline(file, line);)
			{
				// 创建关卡, 关卡以空行分割
				if(line.empty())
				{
					levels.emplace_back(data);

					// 仅保留有地图数据的关卡
					if(levels.back().ascii_map().empty())
						levels.pop_back();
					data.clear();
					continue;
				}
				if(to_lowercase(line.substr(0, 8)) == "comment:")
				{
					do
					{
						data += line + '\n';
						if(file.eof())
							throw std::runtime_error("unexpected end of file");
						std::getline(file, line);
					} while(to_lowercase(line.substr(0, 12)) != "comment-end:");
				}
				data += line + '\n';
			}
			levels.emplace_back(data);
			if(levels.back().ascii_map().empty())
				levels.pop_back();
		}

		return levels;
	}

private:
	/**
	 * @brief 解析地图.
	 *
	 * @param ascii_map XSB 格式地图数据.
	 */
	void parse_map(const std::string& map, const sf::Vector2i& size)
	{
		map_.clear();
		map_.resize(size.x * size.y);
		size_ = size;

		int                y = 0;
		std::istringstream stream(map);
		for(std::string line; std::getline(stream, line);)
		{
			for(int x = 0; x < static_cast<int>(line.size()); x++)
			{
				switch(line[x])
				{
				case ' ':
				case '-':
				case '_':
					break;

				case '#':
					at(x, y) |= Tile::Wall;
					break;

				case 'X':
				case '$':
					at(x, y) |= Tile::Crate;
					crate_positions_.emplace(x, y);
					break;

				case '.':
					at(x, y) |= Tile::Target;
					target_positions_.emplace(x, y);
					break;

				case '@':
					at(x, y) |= Tile::Player;
					player_position_ = {x, y};
					break;

				case '*':
					at(x, y) |= Tile::Crate | Tile::Target;
					crate_positions_.emplace(x, y);
					target_positions_.emplace(x, y);
					break;

				case '+':
					at(x, y) |= Tile::Player | Tile::Target;
					player_position_ = {x, y};
					target_positions_.emplace(x, y);
					break;

				case '\r':
					break;

				default:
					throw std::runtime_error("unknown symbol");
				}
			}
			y++;
		}

		// 填充地板
		if(size.x + size.y > 0)
			fill(player_position_, Tile::Floor, Tile::Wall);
	}

	/**
	 * @brief 解析元数据.
	 *
	 * @param data XSB 格式元数据.
	 */
	void parse_metadata(const std::string& metadata)
	{
		std::istringstream stream(metadata);
		for(std::string line; std::getline(stream, line);)
		{
			const auto it = line.find(":");
			assert(it != std::string::npos);

			auto to_lowercase = [](auto str) {
				std::transform(str.cbegin(), str.cend(), str.begin(), [](auto c) { return std::tolower(c); });
				return str;
			};

			const auto key   = to_lowercase(line.substr(0, it));
			auto       value = line.substr(it + 1);

			value.erase(value.find_last_not_of(' ') + 1);
			value.erase(0, value.find_first_not_of(' '));

			if(key == "comment")
			{
				std::getline(stream, line);
				while(to_lowercase(line.substr(0, 12)) != "comment-end:")
				{
					value += line + '\n';
					if(!stream)
						throw std::runtime_error("unexpected end of stream");
					std::getline(stream, line);
				}
			}

			metadata_.emplace(key, value);
		}
	}

	sf::Vector2i get_player_direction() const
	{
		if(movements_.empty())
			return {0, 1};
		return movement_to_direction(rotate_movement(movements_.back(), rotation_));
	}

	char get_last_rotated_movement() const { return rotate_movement(movements_.back(), rotation_); }

	/**
	 * @brief 检查箱子死否一定锁死.
	 *
	 * @param position 箱子位置.
	 *
	 * @return true  箱子一定锁死.
	 * @return false 箱子不一定锁死.
	 */
	bool is_crate_deadlocked(const sf::Vector2i& position) const
	{
		assert(at(position) & Tile::Crate);

		// #$
		//  #
		{
			const sf::Vector2i directions[4] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
			for(size_t i = 0; i < 4; i++)
				if((at(position + directions[i]) & Tile::Unmovable) &&
				   (at(position + directions[(i + 1) % 4]) & Tile::Unmovable))
					return true;
		}

		// $$
		// ##
		{
			const sf::Vector2i directions[8] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
			for(size_t i = 0; i < 8; i += 2)
			{
				if((at(position + directions[i]) & Tile::Crate) &&
				   (at(position + directions[i + 1]) & at(position + directions[(i + 2) % 8]) & Tile::Unmovable))
					return true;
				if((at(position + directions[i]) & at(position + directions[i + 1]) & Tile::Unmovable) &&
				   (at(position + directions[(i + 2) % 8]) & Tile::Crate))
					return true;
			}
		}

		// FIXME
		// #      #
		// $$  $# $$   $#
		//  # #$   #  #$
		{
			// clang-format off
			const sf::Vector2i directions[24] = {
				{0,  -1}, { 1, 1}, { 1,  0},
				{1,   0}, {-1, 1}, { 0,  1},
				{-1, -1}, { 0, 1}, {-1,  0},
				{0,  -1}, {-1, 1}, { 0, -1},

				{-1, -1}, { 0, -1}, {-1,  0},
				{ 1, -1}, {-1,  0}, { 0, -1},
				{ 0, -1}, { 1,  1}, { 1,  0},
				{ 1,  0}, {-1,  1}, { 0,  1}
			};
			// clang-format on
			for(size_t i = 0; i < 24; i += 3)
			{
				if(at(position + directions[i]) & at(position + directions[i + 1]) & Tile::Unmovable &&
				   at(position + directions[i + 2]) & Tile::Crate)
					return true;
			}
		}

		// $X
		// XX
		{
			const sf::Vector2i directions[8] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
			for(size_t i = 0; i < 8; i += 2)
				if(at(position + directions[i]) & at(position + directions[i + 1]) &
				   at(position + directions[(i + 2) % 8]) & (Tile::Unmovable | Tile::Crate))
					return true;
		}

		return false;
	}

	/**
	 * @brief 检查箱子死否锁死, 若死锁标记死锁.
	 *
	 * @param position 箱子位置.
	 */
	void check_deadlock(const sf::Vector2i& position)
	{
		if(!is_crate_deadlocked(position))
			return;
		at(position) |= Tile::Deadlocked;
		const sf::Vector2i directions[4] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
		for(const auto direction : directions)
			if(at(position + direction) & Tile::Crate && !(at(position + direction) & Tile::Deadlocked))
				check_deadlock(position + direction);
	}

	/**
	 * @brief 重新检查所有箱子死否锁死, 若死锁标记死锁.
	 */
	void refresh_deadlocks()
	{
		clear(Tile::Deadlocked);
		for(const auto& crate_pos : crate_positions_)
			check_deadlock(crate_pos);
	}

	sf::Vector2i                                 size_;
	std::vector<uint8_t>                         map_;
	std::unordered_map<std::string, std::string> metadata_;

	sf::Vector2i                     player_position_;
	std::unordered_set<sf::Vector2i> crate_positions_;
	std::unordered_set<sf::Vector2i> target_positions_;
	std::string                      movements_;

	int  rotation_ = 0;
	bool flipped_  = false;
};

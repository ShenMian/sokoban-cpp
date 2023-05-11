// Copyright 2023 ShenMian
// License(Apache-2.0)

#pragma once

#include "crc32.hpp"
#include "material.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
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
};

class Level
{
public:
	/**
	* @brief 构造函数.
	* 
	* @param data XSB 格式数据.
	* @param size 地图大小.
	*/
	Level(const std::string& data, const sf::Vector2i& size) : size_(size)
	{
		map_.resize(size.x * size.y);

		int                y = 0;
		std::istringstream stream(data);
		for(std::string line; std::getline(stream, line);)
		{
			// 跳过注释
			if(line.front() == ';')
				continue;

			// 解析元数据
			const auto it = line.find(":");
			if(it != std::string::npos)
			{
				auto key   = line.substr(0, it);
				auto value = line.substr(it + 1);

				std::transform(key.cbegin(), key.cend(), key.begin(), [](auto c) { return std::tolower(c); });

				while(value.starts_with(" "))
					value.erase(0, 1);
				while(value.ends_with(" "))
					value.pop_back();

				if(key == "comment")
				{
					std::getline(stream, line);
					while(line.substr(0, 12) != "Comment-End:")
					{
						value += line + '\n';
						std::getline(stream, line);
					}
				}

				metadata_.emplace(key, value);
				continue;
			}

			// 解析地图
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
		fill(player_position_, Tile::Floor, Tile::Wall);
	}

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

				movements_ += std::toupper(movement);
			}
			else
			{
				at(player_position_) &= ~Tile::Player;
				at(player_next_pos) |= Tile::Player;
				player_position_ = player_next_pos;

				movements_ += std::tolower(movement);
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

		const auto last_direction = movement_to_direction(movements_.back());
		if(std::isupper(movements_.back()))
		{
			// 拉箱子
			const auto crate_pos = player_position_ + last_direction;
			at(crate_pos) &= ~(Tile::Crate | Tile::Deadlocked);
			at(player_position_) |= Tile::Crate;
			crate_positions_.erase(crate_pos);
			crate_positions_.insert(player_position_);
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
		while(!movements_.empty())
			undo();
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
	 * @param window   窗口.
	 * @param material 材质.
	 */
	void render(sf::RenderWindow& window, const Material& material)
	{
		const sf::Texture* player_texture;
		sf::Vector2i       player_dir;
		if(movements_.empty())
			player_dir = {0, 1};
		else
			player_dir = movement_to_direction(movements_.back());
		if(player_dir.y == -1)
			player_texture = &material.player_up_texture;
		else if(player_dir.y == 1)
			player_texture = &material.player_down_texture;
		else if(player_dir.x == -1)
			player_texture = &material.player_left_texture;
		else if(player_dir.x == 1)
			player_texture = &material.player_right_texture;

		// TODO: 需要重构, 可读性较差, 非必要的重复计算
		const auto window_size   = sf::Vector2f(window.getSize());
		const auto window_center = window_size / 2.f;

		const auto origin_tile_size = sf::Vector2f(material.wall_texture.getSize());
		const auto origin_map_size  = sf::Vector2f(origin_tile_size.x * size().x, origin_tile_size.y * size().y);

		const auto scale     = std::min({window_size.x / origin_map_size.x, window_size.y / origin_map_size.y, 1.f});
		const auto tile_size = origin_tile_size * scale;
		const auto map_size  = origin_map_size * scale;

		const auto offset = window_center - map_size / 2.f;
		for(int y = 0; y < size().y; y++)
		{
			for(int x = 0; x < size().x; x++)
			{
				sf::Sprite sprite;
				sprite.setScale(scale, scale);
				sprite.setOrigin(-offset);
				sprite.setPosition(x * tile_size.x, y * tile_size.y);

				auto tiles = at(x, y);

				if(tiles & Tile::Floor)
				{
					sprite.setTexture(material.floor_texture);
					window.draw(sprite);
					tiles &= ~Tile::Floor;
				}

				tiles &= ~Tile::Deadlocked;
				tiles &= ~Tile::PlayerMoveable;

				switch(tiles & ~Tile::CrateMoveable)
				{
				case Tile::Wall:
					sprite.setTexture(material.wall_texture);
					break;

				case Tile::Target:
					sprite.setTexture(material.target_texture);
					break;

				case Tile::Crate:
					sprite.setTexture(material.crate_texture);
					break;

				case Tile::Target | Tile::Crate:
					sprite.setColor(sf::Color(180, 180, 180));
					sprite.setTexture(material.crate_texture);
					break;

				case Tile::Target | Tile::Player:
					sprite.setTexture(material.target_texture);
					window.draw(sprite);
					sprite.setTexture(*player_texture);
					break;

				case Tile::Player:
					sprite.setTexture(*player_texture);
					break;
				}
				window.draw(sprite);

				/*
				if(tiles & Tile::CrateMoveable)
				{
					sf::CircleShape circle(10);
					circle.setFillColor(sf::Color(0, 255, 0, 100));
					circle.setScale(sprite.getScale());
					circle.setOrigin(sprite.getOrigin() - sf::Vector2f(sprite.getTexture()->getSize() / 2u) +
					                 sf::Vector2f(circle.getRadius(), circle.getRadius()));
					circle.setPosition(sprite.getPosition());
					window.draw(circle);
					tiles &= ~Tile::CrateMoveable;
				}
				*/
			}
		}
	}

	void calc_player_moveable()
	{
		clear(Tile::PlayerMoveable);
		fill(player_position_, Tile::PlayerMoveable, Tile::Crate | Tile::Wall);
	}

	void calc_crate_moveable(const sf::Vector2i& crate_pos)
	{
		calc_player_moveable();

		const sf::Vector2i directions[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
		for(const auto& direction : directions)
		{
			const auto neighbor_a_pos = crate_pos + direction;
			const auto neighbor_b_pos = crate_pos - direction;
			if(!(at(neighbor_b_pos) & Tile::PlayerMoveable))
				continue;
			auto pos = neighbor_a_pos;
			while(!(at(pos) & (Tile::Crate | Tile::Wall)))
			{
				at(pos) |= Tile::CrateMoveable;

				/*
				at(pos - direction) &= ~Tile::Crate;
				at(pos) |= Tile::Crate;

				show_crate_moveable(pos);

				at(pos) &= ~Tile::Crate;
				at(pos - direction) |= Tile::Crate;
				*/

				pos += direction;
			}
		}
	}

	void rotate()
	{
		// TODO
	}

	void flip()
	{
		// TODO
	}

	/**
	 * @brief 寻路.
	 * 
	 * @param start 起始点.
	 * @param end   终止点.
	 * 
	 * @return std::vector<sf::Vector2i> 最短路径.
	 */
	std::vector<sf::Vector2i> find_path(const sf::Vector2i& start, const sf::Vector2i& end)
	{
		struct Node
		{
			sf::Vector2i data;
			long         priority;

			bool operator==(const Node& rhs) const noexcept { return data == rhs.data; }
			bool operator>(const Node& rhs) const noexcept { return priority > rhs.priority; }
		};

		// 障碍物
		const auto border_tiles = Tile::Wall | Tile::Crate;

		auto manhattan_distance = [](auto a, auto b) {
			return std::abs(static_cast<long>(a.x) - static_cast<long>(b.x)) +
			       std::abs(static_cast<long>(a.y) - static_cast<long>(b.y));
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
				if(at(neighbor) & border_tiles)
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

		const auto origin_tile_size = sf::Vector2f(material.wall_texture.getSize());
		const auto origin_map_size  = sf::Vector2f(origin_tile_size.x * size().x, origin_tile_size.y * size().y);

		const auto scale     = std::min({window_size.x / origin_map_size.x, window_size.y / origin_map_size.y, 1.f});
		const auto tile_size = origin_tile_size * scale;
		const auto map_size  = origin_map_size * scale;

		const auto offset = window_center - map_size / 2.f;

		pos -= sf::Vector2i(static_cast<int>(std::round(offset.x)), static_cast<int>(std::round(offset.y)));
		return sf::Vector2i(static_cast<int>(pos.x / tile_size.x), static_cast<int>(pos.y / tile_size.y));
	}

	uint8_t& at(const sf::Vector2i& pos)
	{
		assert(0 <= pos.x && pos.x < size_.x && 0 <= pos.y && pos.y < size_.y);
		return map_[pos.y * size_.x + pos.x];
	}

	uint8_t at(const sf::Vector2i& pos) const
	{
		assert(0 <= pos.x && pos.x < size_.x && 0 <= pos.y && pos.y < size_.y);
		return map_[pos.y * size_.x + pos.x];
	}

	uint8_t& at(int x, int y) { return at({x, y}); }
	uint8_t  at(int x, int y) const { return at({x, y}); }

	const auto&         metadata() const noexcept { return metadata_; }
	const sf::Vector2i& size() const noexcept { return size_; };
	const auto&         movements() const noexcept { return movements_; }
	const auto&         player_position() const noexcept { return player_position_; }

	uint32_t crc32() const noexcept
	{
		// TODO: 先裁剪掉玩家无法到达的位置
		// TODO: 计算选择和镜像共 8 种地图的 CRC32, 并取最小值
		return ::crc32(0, map_.data(), map_.size());
	};

	/**
	 * @brief 获取 XSB 格式的地图数据.
	 * 
	 * @return std::string XSB 格式的地图数据.
	 */
	std::string map() const
	{
		std::string map;
		for(int y = 0; y < size().y; y++)
		{
			for(int x = 0; x < size().x; x++)
			{
				switch(at({x, y}) & ~(Tile::Deadlocked | Tile::PlayerMoveable | Tile::CrateMoveable))
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

		std::vector<Level> levels;

		// warning: 块注释必须被 "Comment:" 和 "Comment-End:" 包裹, 区分大小写

		std::string data;
		int         rows = 0, cols = 0;
		for(std::string line; std::getline(file, line);)
		{
			// 创建关卡, 关卡以空行分割
			if(line.empty())
			{
				// 仅创建有地图数据的关卡
				if(!data.empty() && rows + cols > 0)
					levels.emplace_back(data, sf::Vector2i(cols, rows));
				data.clear();
				rows = cols = 0;
				continue;
			}

			// 跳过注释
			if(line.front() == ';')
				continue;
			data += line + '\n';

			// 跳过元数据
			if(line.find(":") != std::string::npos)
			{
				if(line.substr(0, 8) == "Comment:")
				{
					do
					{
						std::getline(file, line);
						data += line + '\n';
					} while(line.substr(0, 12) != "Comment-End:");
				}
				continue;
			}
			rows++;
			cols = std::max(static_cast<int>(line.size()), cols);
		}
		if(!data.empty() && rows + cols > 0)
			levels.emplace_back(data, sf::Vector2i(cols, rows));

		return levels;
	}

private:
	void check_deadlock(const sf::Vector2i& position)
	{
		if(!is_crate_deadlocked(position))
			return;
		at(position) |= Tile::Deadlocked;
		if(!(at(position) & Tile::Target))
			puts("Crate was deadlocked!");
		const sf::Vector2i directions[4] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
		for(const auto direction : directions)
			if(at(position + direction) & Tile::Crate && !(at(position + direction) & Tile::Deadlocked))
				check_deadlock(position + direction);
	}

	/**
	 * @brief 检查箱子死否一定锁死.
	 *
	 * @param position 箱子位置.
	 * 
	 * @return true  箱子一定锁死.
	 * @return false 箱子不一定锁死.
	 */
	bool is_crate_deadlocked(const sf::Vector2i& position)
	{
		assert(at(position) & Tile::Crate);
		{
			const sf::Vector2i directions[4] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
			for(size_t i = 0; i < 4; i++)
				if(at(position + directions[i]) & (Tile::Wall | Tile::Deadlocked) &&
				   at(position + directions[(i + 1) % 4]) & (Tile::Wall | Tile::Deadlocked))
					return true;
		}
		{
			const sf::Vector2i directions[8] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
			for (size_t i = 0; i < 8; i += 2)
			{
				if((at(position + directions[i]) & Tile::Crate) &&
				   (at(position + directions[i + 1]) & (Tile::Wall | Tile::Deadlocked)) &&
				   (at(position + directions[(i + 2) % 8]) & (Tile::Wall | Tile::Deadlocked)))
					return true;
				if((at(position + directions[i]) & (Tile::Wall | Tile::Deadlocked)) &&
				   (at(position + directions[i + 1]) & (Tile::Wall | Tile::Deadlocked)) &&
				   (at(position + directions[(i + 2) % 8]) & Tile::Crate))
					return true;
			}
		}
		{
			const sf::Vector2i directions[8] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
			for(size_t i = 0; i < 8; i += 2)
				if(at(position + directions[i]) & at(position + directions[i + 1]) &
				   at(position + directions[(i + 2) % 8]) & (Tile::Wall | Tile::Deadlocked | Tile::Crate))
					return true;
		}
		return false;
	}

	void fill(const sf::Vector2i& position, uint8_t tiles, uint8_t border_tiles)
	{
		std::queue<sf::Vector2i> queue;
		queue.push(position);

		while(!queue.empty())
		{
			const auto pos = queue.front();
			queue.pop();
			at(pos) |= tiles;

			const sf::Vector2i directions[] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
			for(const auto offset : directions)
				if((at(pos + offset) & (border_tiles | tiles)) == 0)
					queue.push(pos + offset);
		}
	}

	void clear(uint8_t tiles)
	{
		std::transform(map_.cbegin(), map_.cend(), map_.begin(), [tiles](auto t) { return t & ~tiles; });
	}

	sf::Vector2i movement_to_direction(char c) const
	{
		switch(std::tolower(c))
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

	sf::Vector2i                                 size_;
	std::vector<uint8_t>                         map_;
	std::unordered_map<std::string, std::string> metadata_;

	sf::Vector2i                     player_position_;
	std::unordered_set<sf::Vector2i> crate_positions_;
	std::unordered_set<sf::Vector2i> target_positions_;
	std::string                      movements_;
};

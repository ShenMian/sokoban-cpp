// Copyright 2023 ShenMian
// License(Apache-2.0)

#include "database.hpp"
#include "level.hpp"
#include "material.hpp"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SQLiteCpp/Database.h>
#include <iostream>
#include <optional>
#include <thread>

class Sokoban
{
public:
	Sokoban() : level_(""), material_("img/default.png"), database_("database.db") {}

	void run(int argc, char* argv[])
	{
		load_sounds();
		background_music_.play();

		database_.import_levels_from_file(std::filesystem::path(argv[0]).parent_path() / "level" / "default.xsb");
		database_.import_levels_from_file(std::filesystem::path(argv[0]).parent_path() / "level" / "box_world.xsb");

		/*
		preview_levels(
		    database_.import_levels_from_file(std::filesystem::path(argv[0]).parent_path() / "level" / "default.xsb"));
		*/

		std::cout << R"(
   _____       __         __
  / ___/____  / /______  / /_  ____ _____
  \__ \/ __ \/ //_/ __ \/ __ \/ __ `/ __ \
 ___/ / /_/ / ,< / /_/ / /_/ / /_/ / / / /
/____/\____/_/|_|\____/_.___/\__,_/_/ /_/

)";
		std::cout << R"(
        1. Open the last session
        2. Open level by id
        3. Import from clipboard
        4. Import from file

)";

		char option;
		std::cin >> option;

		switch(option)
		{
		case '1':
			break;

		case '2': {
			int id;
			std::cout << "Level ID: ";
			std::cin >> id;
			database_.upsert_level_history(id);
			break;
		}

		case '3': {
			level_ = import_level_from_clipboard().value();
			database_.upsert_level_history(level_);
			break;
		}

		case '4': {
			std::filesystem::path path;
			std::cout << "File path: ";
			std::cin >> path;
			const auto levels = database_.import_levels_from_file(path);
			database_.upsert_level_history(database_.get_level_id(levels.front()).value());
			break;
		}

		default:
			throw std::runtime_error("invalid option");
		}

		create_window();

		load_latest_session();

		input_thread_ = std::jthread([&](std::stop_token token) {
			while(!token.stop_requested())
				handle_input();
		});

		while(window_.isOpen())
		{
			handle_window_event();

			render();

			if(!level_.movements().empty() && std::isupper(level_.movements().back()) && level_.passed())
			{
				render();

				passed_sound_.play();
				print_result();
				database_.update_level_solution(level_);
				level_.reset();
				database_.update_history_movements(level_);
				std::this_thread::sleep_for(std::chrono::seconds(2));

				load_next_unsolved_level();
			}
		}
		database_.update_history_movements(level_);
	}

private:
	void render()
	{
		level_.render(window_, material_);
		window_.display();
		window_.clear(sf::Color(115, 115, 115));
	}

	void load_sounds()
	{
		passed_buffer_.loadFromFile("audio/success.wav");
		passed_sound_.setVolume(80.f);
		passed_sound_.setBuffer(passed_buffer_);

		background_music_.openFromFile("audio/background.wav");
		background_music_.setVolume(60.f);
		background_music_.setLoop(true);
	}

	std::optional<Level> import_level_from_clipboard()
	{
		const std::string clipboard = sf::Clipboard::getString();
		if(!clipboard.empty())
		{
			try
			{
				Level level(clipboard);
				database_.import_level(level);
				return level;
			}
			catch(...)
			{
			}
		}
		return std::nullopt;
	}

	void preview_levels(const std::vector<Level>& levels)
	{
		const sf::Vector2i cell_size  = {5, static_cast<int>(std::ceil(levels.size() / 5.f))};
		const sf::Vector2i level_size = {500, 500};
		const sf::Vector2i spacing    = {20, 20};

		sf::RenderTexture preview;
		preview.create(cell_size.x * (level_size.x + spacing.x) - spacing.x,
		               cell_size.y * (level_size.y + spacing.y) - spacing.y);
		for(size_t i = 0; i < levels.size(); i++)
		{
			sf::RenderTexture target;
			target.create(level_size.x, level_size.y);
			target.clear(sf::Color::Transparent);
			levels[i].render(target, material_);
			target.display();

			sf::Sprite sprite;
			sprite.setTexture(target.getTexture());
			sprite.setPosition(static_cast<float>((i % cell_size.x) * (level_size.x + spacing.x)),
			                   static_cast<float>((i / cell_size.x) * (level_size.y + spacing.y)));
			preview.draw(sprite);
		}
		preview.display();
		preview.getTexture().copyToImage().saveToFile("D:/Users/sms/Desktop/level.png");
	}

	void load_next_level()
	{
		const auto id     = database_.get_level_id(level_).value();
		auto       result = database_.get_level_by_id(id + 1);
		if(!result.has_value())
			return;
		level_ = result.value();

		print_info();
		if(level_.metadata().contains("title"))
			window_.setTitle("Sokoban - " + level_.metadata().at("title"));

		database_.upsert_level_history(level_);
		level_.play(database_.get_level_history_movements(level_));
	}

	void load_prev_level()
	{
		const auto id     = database_.get_level_id(level_).value();
		auto       result = database_.get_level_by_id(id - 1);
		if(!result.has_value())
			return;
		level_ = result.value();

		print_info();
		if(level_.metadata().contains("title"))
			window_.setTitle("Sokoban - " + level_.metadata().at("title"));

		database_.upsert_level_history(level_);
		level_.play(database_.get_level_history_movements(level_));
	}

	void load_next_unsolved_level()
	{
		auto id = database_.get_level_id(level_).value();
		while(true)
		{
			auto level = database_.get_level_by_id(++id);
			if(level.has_value() && !level.value().metadata().contains("answer"))
			{
				level_ = level.value();
				break;
			}
		}

		print_info();
		if(level_.metadata().contains("title"))
			window_.setTitle("Sokoban - " + level_.metadata().at("title"));

		database_.upsert_level_history(level_);
		level_.play(database_.get_level_history_movements(level_));
	}

	void load_latest_session()
	{
		level_ = database_.get_level_by_id(database_.get_latest_level_id().value_or(1)).value();

		print_info();
		if(level_.metadata().contains("title"))
			window_.setTitle("Sokoban - " + level_.metadata().at("title"));

		database_.upsert_level_history(level_);
		level_.play(database_.get_level_history_movements(level_));
	}

	void create_window()
	{
		const auto mode = sf::VideoMode::getDesktopMode();
		// window_.create(sf::VideoMode{mode.width / 2, mode.height / 2}, "Sokoban", sf::Style::Close);
		window_.create(sf::VideoMode{mode.width / 2, mode.height / 2}, "Sokoban");
		sf::Image icon;
		icon.loadFromFile("img/crate.png");
		window_.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());
		window_.setFramerateLimit(60);
	}

	void handle_window_event()
	{
		for(auto event = sf::Event{}; window_.pollEvent(event);)
		{
			if(event.type == sf::Event::Closed)
			{
				input_thread_.request_stop();
				input_thread_.join();
				window_.close();
			}
			if(event.type == sf::Event::Resized)
				window_.setView(sf::View(sf::FloatRect(0.f, 0.f, static_cast<float>(event.size.width),
				                                       static_cast<float>(event.size.height))));
		}
	}

	void handle_input()
	{
		if(!window_.hasFocus())
			return;
		handle_mouse_input();
		handle_keyboard_input();
	}

	void handle_mouse_input()
	{
		if(!sf::Mouse::isButtonPressed(sf::Mouse::Left))
			return;

		const auto mouse_pos = level_.to_map_position(sf::Mouse::getPosition(window_), window_, material_);
		if(mouse_pos.x < 1 || mouse_pos.x > level_.size().x || mouse_pos.y < 1 || mouse_pos.y > level_.size().y)
			return;

		if(mouse_select_clock_.getElapsedTime() < sf::seconds(0.25f))
			return;
		mouse_select_clock_.restart();

		try
		{
			level_.at(mouse_pos);
		}
		catch(...)
		{
			return;
		}

		if(selected_crate_ != sf::Vector2i(-1, -1))
		{
			if(level_.at(mouse_pos) & Tile::CrateMovable && selected_crate_ != mouse_pos)
			{
				sf::Clock clock;

				// 推动选中箱子到鼠标位置
				level_.clear(Tile::CrateMovable);

				std::vector<sf::Vector2i> path;
				for(auto pos = mouse_pos; came_from_.contains(pos);)
				{
					path.push_back(pos);
					pos = came_from_[pos];
				}
				std::reverse(path.begin(), path.end());

				auto crate_pos = selected_crate_;
				for(const auto& pos : path)
				{
					sf::Vector2i push_dir;
					push_dir.x       = std::clamp(pos.x - crate_pos.x, -1, 1);
					push_dir.y       = std::clamp(pos.y - crate_pos.y, -1, 1);
					const auto start = crate_pos - push_dir;
					const auto end   = pos - push_dir;
					move_to(start, Tile::Wall | Tile::Crate);
					move_to(end, Tile::Wall);
					crate_pos = pos;
				}

				std::cout << "Move crate: " << clock.getElapsedTime().asMicroseconds()
				          << "us\n"; // TODO: performance test
			}
			else if(level_.at(mouse_pos) & Tile::Crate && selected_crate_ != mouse_pos)
			{
				// 切换选中的箱子
				level_.clear(Tile::CrateMovable);
				came_from_      = level_.calc_crate_movable(mouse_pos);
				selected_crate_ = mouse_pos;
			}
			else
			{
				// 取消选中箱子
				level_.clear(Tile::CrateMovable);
				selected_crate_ = {-1, -1};
			}
			return;
		}
		else if(level_.at(mouse_pos) & Tile::Crate)
		{
			// 选中鼠标处的箱子
			sf::Clock clock;
			came_from_ = level_.calc_crate_movable(mouse_pos);
			std::cout << "Calc crate movable: " << clock.getElapsedTime().asMicroseconds()
			          << "us\n"; // TODO: performance test
			selected_crate_ = mouse_pos;
			return;
		}

		if(level_.at(mouse_pos) & Tile::Floor && !(level_.at(mouse_pos) & Tile::Crate))
		{
			// 移动角色到点击位置
			move_to(mouse_pos, Tile::Wall | Tile::Crate);
		}
	}

	void move_to(const sf::Vector2i& pos, uint8_t border_tiles)
	{
		// 反着写是因为起始点可以为箱子, 但结束点不能
		auto        path        = level_.find_path(pos, level_.player_position(), border_tiles);
		auto        current_pos = level_.player_position();
		std::string movements;
		while(!path.empty())
		{
			const auto direction = path.back() - current_pos;
			path.pop_back();
			if(direction == sf::Vector2i(0, -1))
				movements += 'u';
			else if(direction == sf::Vector2i(0, 1))
				movements += 'd';
			else if(direction == sf::Vector2i(-1, 0))
				movements += 'l';
			else if(direction == sf::Vector2i(1, 0))
				movements += 'r';
			current_pos += direction;
		}
		level_.play(movements, move_interval_);
	}

	void handle_keyboard_input()
	{
		if(keyboard_input_clock_.getElapsedTime() < sf::seconds(0.25f))
			return;
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Up) ||
		   sf::Keyboard::isKeyPressed(sf::Keyboard::K))
		{
			level_.play("u");
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Down) ||
		        sf::Keyboard::isKeyPressed(sf::Keyboard::J))
		{
			level_.play("d");
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Left) ||
		        sf::Keyboard::isKeyPressed(sf::Keyboard::H))
		{
			level_.play("l");
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Right) ||
		        sf::Keyboard::isKeyPressed(sf::Keyboard::L))
		{
			level_.play("r");
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::BackSpace))
		{
			level_.undo();
			selected_crate_ = {-1, -1};
			level_.clear(Tile::PlayerMovable | Tile::CrateMovable);
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
		{
			level_.reset();
			selected_crate_ = {-1, -1};
			level_.clear(Tile::PlayerMovable | Tile::CrateMovable);
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::R))
		{
			level_.rotate();
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::Hyphen))
		{
			load_prev_level();
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::Equal))
		{
			load_next_level();
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::P))
		{
			if(!level_.metadata().contains("answer"))
				return;
			if(!level_.movements().empty())
			{
				level_.reset();
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			level_.play(level_.metadata().at("answer"), move_interval_);
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) && sf::Keyboard::isKeyPressed(sf::Keyboard::V))
		{
			level_ = import_level_from_clipboard().value();
			database_.upsert_level_history(level_);
			keyboard_input_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) && sf::Keyboard::isKeyPressed(sf::Keyboard::I))
		{
			if(move_interval_ != std::chrono::milliseconds(0))
				move_interval_ = std::chrono::milliseconds(0);
			else
				move_interval_ = std::chrono::milliseconds(200);
			keyboard_input_clock_.restart();
		}
	}

	void print_info()
	{
		if(level_.metadata().contains("title"))
			std::cout << "Title: " << level_.metadata().at("title") << '\n';
		if(level_.metadata().contains("author"))
			std::cout << "Author: " << level_.metadata().at("author") << '\n';
	}

	void print_result()
	{
		const auto movements = level_.movements();
		std::cout << "Moves: " << movements.size() << '\n';
		std::cout << "Pushs: "
		          << std::count_if(movements.begin(), movements.end(), [](auto c) { return std::isupper(c); }) << '\n';
		std::cout << "LURD : " << movements << '\n';
		std::cout << '\n';
	}

	Level level_;

	sf::RenderWindow window_;
	Material         material_;

	sf::SoundBuffer passed_buffer_;
	sf::Sound       passed_sound_;
	sf::Music       background_music_;

	sf::Clock    keyboard_input_clock_, mouse_select_clock_;
	std::jthread input_thread_;

	std::chrono::milliseconds move_interval_ = std::chrono::milliseconds(200);

	sf::Vector2i                                   selected_crate_ = {-1, -1};
	std::unordered_map<sf::Vector2i, sf::Vector2i> came_from_;

	Database database_;
};

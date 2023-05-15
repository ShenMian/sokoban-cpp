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

		const std::string clipboard = sf::Clipboard::getString();
		if(!clipboard.empty() && (clipboard.front() == ';' || clipboard.front() == '-' || clipboard.front() == '#'))
		{
			try
			{
				Level level(clipboard);
				database_.import_level(level);
				database_.upsert_level_history(level);
			}
			catch(...)
			{
			}
		}

		if(argc == 2)
			database_.upsert_level_history(std::atoi(argv[1]));

		level_ = database_.get_level_by_id(database_.get_latest_level_id().value_or(1)).value();

		if(level_.metadata().contains("title"))
			window_.setTitle("Sokoban - " + level_.metadata().at("title"));
		database_.upsert_level_history(level_);
		level_.play(database_.get_level_history_movements(level_));

		create_window();

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
				database_.update_level_answer(level_);
				level_.reset();
				database_.update_history_movements(level_);
				std::this_thread::sleep_for(std::chrono::seconds(2));

				// 加载下一个没有答案的关卡
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

				database_.upsert_level_history(level_);
				level_.play(database_.get_level_history_movements(level_));
				if(level_.metadata().contains("title"))
					window_.setTitle("Sokoban - " + level_.metadata().at("title"));
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

	void create_window()
	{
		const auto mode = sf::VideoMode::getDesktopMode();
		window_.create(sf::VideoMode{mode.width / 2, mode.height / 2}, "Sokoban", sf::Style::Close);
		sf::Image icon;
		icon.loadFromFile("img/crate.png");
		window_.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());
		window_.setFramerateLimit(60);
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

		if(mouse_select_clock.getElapsedTime() < sf::seconds(0.25f))
			return;
		mouse_select_clock.restart();
		if(selected_crate_ != sf::Vector2i(-1, -1))
		{
			if(level_.at(mouse_pos) & Tile::CrateMoveable)
			{
				level_.clear(Tile::CrateMoveable);

				// 推动选中箱子到鼠标位置
				sf::Vector2i push_dir;
				push_dir.x       = std::clamp(mouse_pos.x - selected_crate_.x, -1, 1);
				push_dir.y       = std::clamp(mouse_pos.y - selected_crate_.y, -1, 1);
				const auto start = selected_crate_ - push_dir;
				const auto end   = mouse_pos - push_dir;
				move_to(start, Tile::Wall | Tile::Crate);
				move_to(end, Tile::Wall);
			}
			else if(level_.at(mouse_pos) & Tile::Crate && selected_crate_ != mouse_pos)
			{
				// 切换选中的箱子
				level_.clear(Tile::CrateMoveable);
				level_.calc_one_step_crate_moveable(mouse_pos);
				selected_crate_ = mouse_pos;
			}
			else
			{
				// 取消选中箱子
				level_.clear(Tile::CrateMoveable);
				selected_crate_ = {-1, -1};
			}
			return;
		}
		else if(level_.at(mouse_pos) & Tile::Crate)
		{
			// 选中鼠标处的箱子
			level_.calc_one_step_crate_moveable(mouse_pos);
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
		level_.play(movements, std::chrono::milliseconds(100));
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
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::BackSpace))
		{
			level_.undo();
			keyboard_input_clock_.restart();
		}
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
		{
			level_.reset();
			keyboard_input_clock_.restart();
		}
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::P))
		{
			if(!level_.metadata().contains("answer"))
				return;
			if(!level_.movements().empty())
			{
				level_.reset();
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			level_.play(level_.metadata().at("answer"), std::chrono::milliseconds(200));
			keyboard_input_clock_.restart();
		}
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::R))
		{
			level_.rotate();
			keyboard_input_clock_.restart();
		}
	}

	void print_result()
	{
		const auto movements = level_.movements();
		if(level_.metadata().contains("title"))
			std::cout << "Title: " << level_.metadata().at("title") << '\n';
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

	sf::Clock    keyboard_input_clock_, mouse_select_clock;
	sf::Vector2i selected_crate_ = {-1, -1};
	std::jthread input_thread_;

	Database database_;

	// std::vector<sf::Keyboard::Key, sf::Clock>> key_pressed_;
};

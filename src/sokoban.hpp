// Copyright 2023 ShenMian
// License(Apache-2.0)

#include "level.hpp"
#include "material.hpp"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SQLiteCpp/Database.h>
#include <array>
#include <iostream>
#include <thread>

class Sokoban
{
public:
	Sokoban() : database_("database.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
	{
		// database_.exec("DROP TABLE IF EXISTS tb_levels");
		database_.exec("CREATE TABLE IF NOT EXISTS tb_levels ("
		               "	id INTEGER PRIMARY KEY AUTOINCREMENT,"
		               "	title TEXT,"
		               "	author TEXT,"
		               "	data TEXT NOT NULL,"
		               "	crc32 INTEGER NOT NULL,"
		               "	answer TEXT,"
		               "	date DATE NOT NULL"
		               ")");
	}

	void run(int argc, char* argv[])
	{
		create_window();

		load_sounds();
		background_music_.play();

		// const auto path = std::filesystem::current_path() / "level" / "box_world.xsb";
		const auto path = std::filesystem::current_path() / "level" / "default.xsb";
		load_levels_from_file(path);

		if(argc == 2)
			level_ = levels_.begin() + std::atoi(argv[1]) - 1;
		
		if(sf::Clipboard::getString().substring(0, 1) == "-" || sf::Clipboard::getString().substring(0, 1) == "#")
			load_level_from_clipboard();

		input_thread = std::jthread([&](std::stop_token token) {
			while(!token.stop_requested())
				handle_input();
		});

		while(window_.isOpen())
		{
			handle_window_event();

			render();

			if(!level_->movements().empty() && std::isupper(level_->movements().back()) && level_->passed())
			{
				render();

				success_sound_.play();
				print_result();
				std::this_thread::sleep_for(std::chrono::seconds(2));

				level_++;
				assert(level_ != levels_.end());

				if(level_->metadata().contains("title"))
					window_.setTitle("Sokoban - " + level_->metadata().at("title"));
			}
		}
	}

private:
	void render()
	{
		level_->render(window_, material_);
		window_.display();
		window_.clear(sf::Color(115, 115, 115));
	}

	void print_result()
	{
		const auto movements = level_->movements();
		if(level_->metadata().contains("title"))
			std::cout << "Title: " << level_->metadata().at("title") << '\n';
		std::cout << "Moves: " << movements.size() << '\n';
		std::cout << "Pushs: "
		          << std::count_if(movements.begin(), movements.end(), [](auto c) { return std::isupper(c); }) << '\n';
		std::cout << "LURD : " << movements << '\n';
		std::cout << '\n';
	}

	void create_window()
	{
		window_.create(sf::VideoMode{2560 / 2, 1440 / 2}, "Sokoban", sf::Style::Close);
		sf::Image icon;
		icon.loadFromFile("img/crate.png");
		window_.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());
		window_.setFramerateLimit(60);
	}

	void load_sounds()
	{
		success_buffer_.loadFromFile("audio/success.wav");
		success_sound_.setBuffer(success_buffer_);

		background_music_.openFromFile("audio/background.wav");
		background_music_.setVolume(80.f);
		background_music_.setLoop(true);
	}

	void load_levels_from_file(const std::filesystem::path& path)
	{
		levels_ = Level::load(path);

		// TODO: 通关后再录入数据
		SQLite::Statement query_level(database_, "SELECT * FROM tb_levels "
		                                         "WHERE crc32 = ?");
		SQLite::Statement add_level(database_, "INSERT INTO tb_levels (title, author, data, crc32, date) "
		                                       "VALUES (?, ?, ?, ?, DATE('now'))");
		for(const auto& level : levels_)
		{
			query_level.reset();
			query_level.bind(1, level.crc32());
			if(query_level.executeStep())
				continue;

			add_level.reset();
			if(level.metadata().contains("title"))
				add_level.bind(1, level.metadata().at("title"));
			if(level.metadata().contains("author"))
				add_level.bind(2, level.metadata().at("author"));
			add_level.bind(3, level.map());
			add_level.bind(4, level.crc32());
			add_level.exec();
		}

		level_ = levels_.begin();
		if(level_->metadata().contains("title"))
			window_.setTitle("Sokoban - " + level_->metadata().at("title"));
	}

	void load_level_from_clipboard()
	{
		const std::string  data = sf::Clipboard::getString();
		int                rows = 0, cols = 0;
		std::istringstream stream(data);
		for(std::string line; std::getline(stream, line);)
		{
			if(line.front() == ';' || line.find(":") != std::string::npos)
				continue;
			cols = std::max(static_cast<int>(line.size()), cols);
			rows++;
		}
		levels_.clear();
		levels_.emplace_back(data, sf::Vector2i(cols, rows));

		level_ = levels_.begin();
		if(level_->metadata().contains("title"))
			window_.setTitle("Sokoban - " + level_->metadata().at("title"));
	}

	void handle_window_event()
	{
		for(auto event = sf::Event{}; window_.pollEvent(event);)
		{
			if(event.type == sf::Event::Closed)
			{
				input_thread.request_stop();
				input_thread.join();
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

		const auto mouse_pos = level_->to_map_position(sf::Mouse::getPosition(window_), window_, material_);
		if(mouse_pos.x < 1 || mouse_pos.x > level_->size().x || mouse_pos.y < 1 || mouse_pos.y > level_->size().y)
			return;
		
		// if(level_->at(mouse_pos) & Tile::Floor)
		if(level_->at(mouse_pos) & (Tile::Floor | Tile::Crate))
		{
			// 移动角色
			
			// 反着写是因为起始点可以为箱子, 但结束点不能
			auto        path        = level_->find_path(mouse_pos, level_->player_position());
			auto        current_pos = level_->player_position();
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
			level_->play(movements, std::chrono::milliseconds(100));
		}
	}

	void handle_keyboard_input()
	{
		if(move_clock_.getElapsedTime() < sf::seconds(0.25f))
			return;
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
		{
			level_->play("u");
			move_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
		{
			level_->play("d");
			move_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
		{
			level_->play("l");
			move_clock_.restart();
		}
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
		{
			level_->play("r");
			move_clock_.restart();
		}
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::BackSpace))
		{
			level_->undo();
			move_clock_.restart();
		}
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
		{
			level_->reset();
			move_clock_.restart();
		}
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::P))
		{
			/*
			if(map_.get_metadata("lurd").empty())
			    return;
			if (!map_.get_movements().empty())
			{
			    load_levels();
			    std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			map_.play(map_.get_metadata("lurd"), std::chrono::milliseconds(200));
			move_clock_.restart();
			*/
		}
	}

	std::vector<Level>::iterator level_;
	std::vector<Level>           levels_;

	sf::RenderWindow window_;
	Material         material_;

	sf::SoundBuffer success_buffer_;
	sf::Sound       success_sound_;
	sf::Music       background_music_;

	sf::Clock    move_clock_;
	sf::Vector2i selected_crate_ = {-1, -1};
	std::jthread input_thread;

	SQLite::Database database_;

	// std::vector<sf::Keyboard::Key, std::chrono::duration<std::chrono::milliseconds>> key_pressed;
};

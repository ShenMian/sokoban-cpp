// Copyright 2023 ShenMian
// License(Apache-2.0)

#include "map.hpp"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <thread>

class Sokoban
{
public:
	void run(int argc, char* argv[])
	{
		create_window();

		load_sounds();
		background_sound_.play();

		if(argc == 2)
			level_ = std::atoi(argv[1]);
		load_level();

		input_thread = std::jthread([&](std::stop_token token) {
			while(!token.stop_requested())
				handle_input();
		});

		while(window_.isOpen())
		{
			handle_window_event();

			render();

			if(map_.is_passed())
			{
				render();

				success_sound_.play();
				print_result();
				std::this_thread::sleep_for(std::chrono::seconds(2));

				level_++;
				load_level();

				level_clock_.restart();
			}
		}
	}

private:
	void render()
	{
		map_.render(window_);
		window_.display();
		window_.clear(sf::Color(115, 115, 115));
	}

	void print_result()
	{
		std::cout << "Title: " << map_.get_metadata("title") << '\n';
		std::cout << "Time : " << std::fixed << std::setprecision(2) << level_clock_.getElapsedTime().asSeconds()
		          << " sec\n";
		std::cout << "Steps: " << map_.get_movements().size() << '\n';
		std::cout << "LURD : " << map_.get_movements() << '\n';
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

		background_buffer_.loadFromFile("audio/background.wav");
		background_sound_.setBuffer(background_buffer_);
		background_sound_.setVolume(80.f);
		background_sound_.setLoop(true);
	}

	void load_level()
	{
		const std::string group = "default";
		// const std::string group = "classic";

		const std::filesystem::path path =
		    std::filesystem::current_path() / "level" / group / ("level_" + std::to_string(level_) + ".txt");
		if(!std::filesystem::exists(path))
		{
			std::cout << "You have passed all levels. Thank you for playing :D\n";
			window_.close();
			return;
		}
		map_.load(path);
		if(!map_.get_metadata("title").empty())
			window_.setTitle("Sokoban - " + map_.get_metadata("title"));
	}

	void handle_window_event()
	{
		for (auto event = sf::Event{}; window_.pollEvent(event);)
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
		if(move_clock_.getElapsedTime() > sf::seconds(0.25f))
		{
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
			{
				map_.move({0, -1});
				move_clock_.restart();
			}
			else if(sf::Keyboard::isKeyPressed(sf::Keyboard::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
			{
				map_.move({0, 1});
				move_clock_.restart();
			}
			else if(sf::Keyboard::isKeyPressed(sf::Keyboard::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
			{
				map_.move({-1, 0});
				move_clock_.restart();
			}
			else if(sf::Keyboard::isKeyPressed(sf::Keyboard::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
			{
				map_.move({1, 0});
				move_clock_.restart();
			}
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
			{
				load_level();
				move_clock_.restart();
			}
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::P))
			{
				if(map_.get_metadata("lurd").empty())
					return;
				if (!map_.get_movements().empty())
				{
					load_level();
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
				map_.play(map_.get_metadata("lurd"));
				move_clock_.restart();
			}
		}
	}

	int level_ = 1;
	Map map_;

	sf::SoundBuffer success_buffer_, background_buffer_;
	sf::Sound       success_sound_, background_sound_;

	sf::RenderWindow window_;

	sf::Clock   level_clock_, move_clock_;
	std::jthread input_thread;
};

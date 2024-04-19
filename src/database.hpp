// Copyright 2023 ShenMian
// License(Apache-2.0)

#pragma once

#include <SQLiteCpp/Database.h>

#include <cassert>
#include <filesystem>
#include <optional>

#include "level.hpp"

class Database {
  public:
    Database(const std::filesystem::path& path) :
        database_(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
        setup();
    }

    /**
	 * @brief 配置数据库.
	 */
    void setup() {
        database_.exec(
            "CREATE TABLE IF NOT EXISTS tb_level ("
            "	id       INTEGER PRIMARY KEY AUTOINCREMENT,"
            "	title    TEXT,"
            "	author   TEXT,"
            "	map      TEXT NOT NULL,"
            "	crc32    INTEGER NOT NULL,"
            "	solution TEXT,"
            "	date     DATE NOT NULL"
            ")"
        );
        database_.exec(
            "CREATE TABLE IF NOT EXISTS tb_session ("
            "	level_id INTEGER UNIQUE,"
            "	movement TEXT,"
            "	datetime DATETIME NOT NULL,"
            "	FOREIGN KEY (level_id) REFERENCES tb_level(id)"
            ")"
        );
    }

    /**
	 * @brief 重置数据库.
	 */
    void reset() {
        database_.exec("DROP TABLE IF EXISTS tb_level, tb_session");
        setup();
    }

    /**
	 * @brief 导入关卡.
	 *
	 * @param level 关卡.
	 */
    void import_level(const Level& level) {
        SQLite::Statement query_level(
            database_,
            "SELECT * FROM tb_level "
            "WHERE crc32 = ?"
        );
        query_level.bind(1, level.crc32());
        if (query_level.executeStep())
            return;

        SQLite::Statement insert_level(
            database_,
            "INSERT INTO tb_level(title, author, map, crc32, date) "
            "VALUES (?, ?, ?, ?, DATE('now'))"
        );
        if (level.metadata().contains("title"))
            insert_level.bind(1, level.metadata().at("title"));
        if (level.metadata().contains("author"))
            insert_level.bind(2, level.metadata().at("author"));
        insert_level.bind(3, level.ascii_map());
        insert_level.bind(4, level.crc32());
        insert_level.exec();
    }

    /**
	 * @brief 从文件导入关卡.
	 *
	 * @param path XSB 格式文件路径.
	 */
    std::vector<Level> import_levels_from_file(const std::filesystem::path& path
    ) {
        const auto levels = Level::load(path);
        for (const auto& level : levels)
            import_level(level);
        return levels;
    }

    /**
	 * @brief 获取关卡 ID.
	 *
	 * @param level 关卡.
	 */
    std::optional<int> get_level_id(const Level& level) {
        SQLite::Statement query_id(
            database_,
            "SELECT id FROM tb_level "
            "WHERE crc32 = ?"
        );
        query_id.bind(1, level.crc32());
        if (!query_id.executeStep())
            return std::nullopt;
        return query_id.getColumn("id");
    }

    /**
	 * @brief 通过 ID 获取关卡.
	 *
	 * @param id 关卡 ID.
	 */
    std::optional<Level> get_level_by_id(int id) {
        SQLite::Statement query_level(
            database_,
            "SELECT title, author, map, solution FROM tb_level "
            "WHERE id = ?"
        );
        query_level.bind(1, id);
        if (!query_level.executeStep())
            return std::nullopt;
        std::string data;
        if (!query_level.getColumn("title").isNull())
            data +=
                "Title: " + query_level.getColumn("title").getString() + '\n';
        if (!query_level.getColumn("author").isNull())
            data +=
                "Author: " + query_level.getColumn("author").getString() + '\n';
        if (!query_level.getColumn("solution").isNull())
            data += "Solution: " + query_level.getColumn("solution").getString()
                + '\n';
        data += query_level.getColumn("map").getString();
        return Level(data);
    }

    /**
	 * @brief 更新关卡答案.
	 *
	 * @param level_id 关卡 ID.
	 * @param solution 关卡答案.
	 */
    bool update_level_solution(int level_id, const std::string& solution) {
        SQLite::Statement update_solution(
            database_,
            "UPDATE tb_level "
            "SET solution = ? "
            "WHERE id = ? AND EXISTS ("
            "	SELECT * FROM tb_level WHERE id = ? AND (solution IS NULL OR LENGTH(solution) > LENGTH(?))"
            ")"
        );
        update_solution.bind(1, solution);
        update_solution.bind(2, level_id);
        update_solution.bind(3, level_id);
        update_solution.bind(4, solution);
        return update_solution.exec();
    }

    /**
	 * @brief 更新关卡答案.
	 *
	 * @param level 已通关的关卡.
	 */
    bool update_level_solution(const Level& level) {
        assert(level.passed());
        return update_level_solution(
            get_level_id(level).value(),
            level.movement()
        );
    }

    /**
	 * @brief 更新关卡会话移动.
	 *
	 * @param level_id 关卡 ID.
	 */
    bool update_session_movement(int level_id, const std::string& movement) {
        SQLite::Statement update_movements(
            database_,
            "UPDATE tb_session "
            "SET movement = ? "
            "WHERE level_id = ?"
        );
        update_movements.bind(1, movement);
        update_movements.bind(2, level_id);
        return update_movements.exec();
    }

    /**
	 * @brief 更新关卡会话移动.
	 *
	 * @param level 关卡.
	 */
    bool update_session_movement(const Level& level) {
        return update_session_movement(
            get_level_id(level).value(),
            level.movement()
        );
    }

    /**
	 * @brief 添加关卡会话.
	 *
	 * @param level 关卡.
	 */
    bool upsert_level_session(const Level& level) {
        return upsert_level_session(get_level_id(level).value());
    }

    /**
	 * @brief 添加关卡会话.
	 *
	 * @param level_id 关卡 ID.
	 */
    bool upsert_level_session(int level_id) {
        SQLite::Statement upsert_history(
            database_,
            "INSERT OR IGNORE INTO tb_session(level_id, datetime) "
            "VALUES(?, DATETIME('now')) "
            "ON CONFLICT(level_id) DO UPDATE SET datetime = DATETIME('now')"
        );
        upsert_history.bind(1, level_id);
        return upsert_history.exec();
    }

    /**
	 * @brief 获取历史最新会话关卡 ID.
	 */
    std::optional<int> get_latest_level_id() {
        SQLite::Statement query_latest_history(
            database_,
            "SELECT level_id FROM tb_session "
            "ORDER BY datetime DESC "
            "LIMIT 1"
        );
        if (!query_latest_history.executeStep())
            return std::nullopt;
        return query_latest_history.getColumn("level_id");
    }

    /**
	 * @brief 获取关卡会话移动.
	 *
	 * @param level 关卡.
	 */
    std::string get_level_session_movements(const Level& level) {
        SQLite::Statement query_movements(
            database_,
            "SELECT movement FROM tb_session "
            "WHERE level_id = ?"
        );
        query_movements.bind(1, get_level_id(level).value());
        if (!query_movements.executeStep())
            return "";
        return query_movements.getColumn(0);
    }

  private:
    SQLite::Database database_;
};

#include <optional>
#define BOOST_BEAST_USE_STD_STRING_VIEW
#define BOOST_BEAST_USE_STD_STRING
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <iostream>
#include <pqxx/pqxx>
#include <string>

// #include "json_loader.h"
/*
constexpr auto ACTION_KEY = "action"sv;
constexpr auto PAYLOAD_KEY = "payload"sv;
constexpr auto TITLE_KEY = "title"sv;
constexpr auto AUTHOR_KEY = "author"sv;
constexpr auto YEAR_KEY = "year"sv;
constexpr auto ISBN_KEY = "ISBN"sv;
*/
using pqxx::operator"" _zv;
using namespace std::literals;

int main(int argc, const char* argv[]) {
    namespace json = boost::json;
    if (argc == 1) {
        std::cout << "Usage: connect_db <conn-string>\n"sv;
        return EXIT_SUCCESS;
    }
    if (argc != 2) {
        std::cerr << "Invalid command line\n"sv;
        return EXIT_FAILURE;
    }
    // Подключаемся к БД, указывая её параметры в качестве аргумента
    pqxx::connection conn{argv[1]};

    constexpr auto tag_add_book = "add_book"_zv;
    constexpr auto tag_all_books = "all_books"_zv;
        {
            pqxx::work w(conn);
            w.exec(
                "CREATE TABLE IF NOT EXISTS books ("
                "id SERIAL PRIMARY KEY, "
                "title varchar(100) NOT NULL, "
                "author varchar(100) NOT NULL, "
                "year integer NOT NULL, "
                "ISBN char(13) UNIQUE"
                ")"_zv);
            w.commit();
        }
    conn.prepare(tag_add_book,
        "INSERT INTO books (title, year, author, ISBN) "
        "VALUES ($1, $2, $3, $4)"_zv);
    conn.prepare(tag_all_books,
        "SELECT id, title, author, year, ISBN FROM books "
        "ORDER BY year DESC, title ASC, author ASC, ISBN ASC"_zv);
    std::string json_line;

    while (std::getline(std::cin, json_line)) {
        try {
            auto parsed = json::parse(json_line);
            auto action = parsed.at("action").as_string();
            if (action == "add_book"sv) {
                const json::object& payload = parsed.at("payload").as_object();
                const std::string title = value_to<std::string>(payload.at("title"));
                auto year = payload.at("year").as_int64();
                const auto author = json::value_to<std::string>(payload.at("author"));
                std::optional<std::string> isbn;
                if (!payload.at("ISBN").is_null()) {
                    isbn = json::value_to<std::string>(payload.at("ISBN"));
                }
                json::object result_obj;
                try {
                    pqxx::work w(conn);
                    w.exec_prepared(tag_add_book, title, year, author, isbn);
                    w.commit();
                    result_obj["result"] = true;
                } catch (const pqxx::sql_error& e) {
                    // Если ISBN не уникален, ловим ошибку здесь
                    result_obj["result"] = false;
                }
                std::cout << json::serialize(result_obj) << std::endl;

            } else if (action == "all_books"sv) {
                pqxx::read_transaction r(conn);
                auto rows = r.exec_prepared(tag_all_books);

                json::array books;
                for (const auto& row : rows) {
                    json::object book;
                    book["id"] = row["id"].as<int>();
                    book["title"] = row["title"].as<std::string>();
                    book["author"] = row["author"].as<std::string>();
                    book["year"] = row["year"].as<int>();

                    // Если в БД NULL, в JSON пишем null
                    if (row["ISBN"].is_null()) {
                        book["ISBN"] = nullptr;
                    } else {
                        // Удаляем лишние пробелы, т.к. char(13) дополняет строку пробелами
                        std::string s = row["ISBN"].as<std::string>();
                        while (!s.empty() && s.back() == ' ')
                            s.pop_back();
                        book["ISBN"] = s;
                    }
                    books.push_back(std::move(book));
                }
                std::cout << json::serialize(books) << std::endl;
            } else if (action == "exit"sv) {
                return EXIT_SUCCESS;
            }
        } catch (const std::exception& ex) {
            //std::cerr << ex.what() << std::endl;
            //return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

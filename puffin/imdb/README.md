# puffin::imdb

A small in-memory database: named tables of values, queried with a fluent `select().from().where().order_by()`
request, with optional materialized views and secondary indexes. Header only, C++20.

- Target: `puffin::imdb`
- Header: `<puffin/imdb.hpp>`
- Optional JSON helpers: `<puffin/imdb/json/nlohmann.hpp>` (needs [nlohmann/json](https://github.com/nlohmann/json),
  which you link yourself)

```cmake
target_link_libraries(my_app PRIVATE puffin::imdb nlohmann_json::nlohmann_json)
```

## Quick start

```c++
#include <puffin/imdb.hpp>
#include <puffin/imdb/json/nlohmann.hpp>

using namespace puffin::imdb;

in_memory_database<std::string, json> db;

db.insert("cards", R"({ "uuid": "1", "number": 1, "title": "My Title",   "type": ["Standard"] })"_json);
db.insert("cards", R"({ "uuid": "2", "number": 2, "title": "Your Title", "type": ["Promo"] })"_json);

auto cards = db.select()
               .from("cards")
               .where(json_search { .key = "title", .text = "My" })
               .order_by(json_sort { .key = "number" })
               .execute();

for (json& card : cards) {   // std::vector<std::reference_wrapper<json>>
  card["seen"] = true;       // rows are references, this changes the stored value
}
```

## Model

`basic_in_memory_database<Key, Value, Traits>` (alias `in_memory_database<Key, Value>`) owns every inserted value
once, in a single `std::list`, so references to rows stay valid for the lifetime of the database. A *table* is a
named vector of references into that storage: `Key` names tables, views and indexes, `Value` is the row type.
Any copyable type works as `Value`, `json` is just the most common choice.

The database is not copyable and not thread safe: protect it yourself if several threads use it.

## Inserting and reading

| Call | Effect |
| --- | --- |
| `insert(table, value)` | Copies `value` into the storage and appends it to `table`. Returns a `std::reference_wrapper<Value>` to the stored row. |
| `batch_insert(table, begin, end)` | Same for an iterator range. |
| `size(table)` | Number of rows in `table` (0 for an unknown table). |
| `row(table, i)` | Row `i` of `table`, throws `std::out_of_range` if `i` is out of bounds. |
| `data(table)` | The table itself, a `std::vector<std::reference_wrapper<Value>>`. |

Tables are created on first use, there is no schema. There is no removal yet.

## Requests

`db.select()` returns a `request` bound to the database. Every step is optional except `from`:

```c++
auto req = db.select()
             .from("cards")                                   // table or view to read
             .where([](const json& j) { return j["number"] > 1; })
             .order_by([](const json& a, const json& b) { return a["title"] < b["title"]; });

std::vector<std::reference_wrapper<json>> rows = req.execute();
```

- `where(predicate)` keeps the rows for which `bool(const Value&)` returns `true`.
- `order_by(less)` sorts the result with a `bool(const Value&, const Value&)` comparator.
- `execute()` runs the request now. Results are references to the stored rows, not copies. A request can be
  executed again later to see newly inserted rows.

**Projection.** `db.select(f)` transforms every row with `f` after filtering and sorting, and `execute()` then
returns copies of the projected type:

```c++
std::vector<std::string> titles = db.select([](const json& j) -> std::string { return j["title"]; })
                                    .from("cards")
                                    .execute();
```

**Detached requests.** A request can also be built on its own and run with `query`:

```c++
auto req = request<std::string, json>().from("cards");
auto rows = db.query(req);
```

## Views

A view stores the result of a request under a new name, which can then be queried like a table:

```c++
auto req = db.select().from("cards").order_by(json_sort { .key = "number" });
db.create_view("cards_by_number", std::move(req));

auto first = db.select().from("cards_by_number").execute().front();
```

Views are snapshots: inserting into `cards` does not change `cards_by_number`. Call
`db.update_views("cards_by_number")` to run its request again.

## Indexes

An index maps keys to rows. The function returns the list of keys under which a row is reachable:

```c++
db.create_index("cards", "cards_by_uuid", [](const json& j) -> std::list<std::string> {
  return { j["uuid"] };
});

json& card = db.row("cards_by_uuid", "2");   // throws std::out_of_range for an unknown key
```

Like views, indexes are built from the rows present when `create_index` is called and are not updated on insert.
A key is kept for its first row only (`std::map::insert`).

## JSON helpers

`<puffin/imdb/json/nlohmann.hpp>` defines `json` as `nlohmann::json` and a few function objects to use with
`where` and `order_by`:

| Helper | Use | Meaning |
| --- | --- | --- |
| `json_sort { .key }` | `order_by` | Ascending on `j[key]`. |
| `json_search { .key, .text }` | `where` | `j[key]` is a string containing `text`. |
| `json_contains { .key, .text }` | `where` | `j[key]` is an array holding the string `text`. |
| `json_exists { .key }` | `where` | `j` has `key`. |

## Custom storage

The third template parameter, `Traits`, selects the containers. `imdb_traits<Key, Value, Allocator>` uses a
`std::list<Value, Allocator>` for the storage, so a custom allocator can be passed without writing new traits:

```c++
using traits = imdb_traits<std::string, json, my_allocator<json>>;
basic_in_memory_database<std::string, json, traits> db;
```

## Limitations

- No update or removal of rows.
- Views and indexes are not refreshed automatically on insert.
- Not thread safe.

See the [roadmap](../../docs/roadmap.md) and the tests in [`tests/imdb_test.cpp`](tests/imdb_test.cpp).

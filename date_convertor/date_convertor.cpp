#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <clocale>

using namespace std;

namespace simple_json {
    class object {
        vector<pair<string, string>> data;
    public:
        void add(const string& k, const string& v) { data.emplace_back(k, v); }
        string dump() const {
            string r = "{";
            for (size_t i = 0; i < data.size(); ++i) {
                if (i) r += ",";
                r += "\"" + data[i].first + "\":\"" + data[i].second + "\"";
            }
            return r + "}";
        }
    };

    class array {
        vector<object> objects;
    public:
        void add(const object& o) { objects.push_back(o); }
        string dump() const {
            string r = "[";
            for (size_t i = 0; i < objects.size(); ++i) {
                if (i) r += ",";
                r += objects[i].dump();
            }
            return r + "]";
        }
    };
}

struct DateRecord {
    string name, iso, dmy, mdy;
    bool has_error;
    DateRecord() : has_error(false) {}
};

struct BenchmarkResult {
    int records_processed;
    long long load_time_ms, convert_time_ms, validation_time_ms, total_time_ms;
    double records_per_second;
    BenchmarkResult() : records_processed(0), load_time_ms(0), convert_time_ms(0),
                        validation_time_ms(0), total_time_ms(0), records_per_second(0.0) {}
};

vector<BenchmarkResult> benchmark_results;
int total_tests_run = 0, passed_tests = 0;

void printHeader(const string& t) {
    cout << "\n" << string(60, '=') << "\n  " << t
         << "\n" << string(60, '=') << "\n";
}

void help() {
    printHeader("КОНВЕРТЕР ДАТ");
    cout << "1) Генерация JSON\n"
         << "2) ISO -> DD.MM.YYYY\n"
         << "3) ISO -> MM/DD/YYYY\n"
         << "4) Анализ\n"
         << "5) Самотесты\n"
         << "6) Бенчмарк\n"
         << "7) Отладка\n"
         << "0) Выход\n";
}

bool validISO(const string& s) {
    if (s.length() != 10 || s[4] != '-' || s[7] != '-') return false;
    try {
        int y = stoi(s.substr(0, 4));
        int m = stoi(s.substr(5, 2));
        int d = stoi(s.substr(8, 2));

        if (y < 1900 || y > 2100) return false;
        if (m < 1 || m > 12) return false;
        if (d < 1 || d > 31) return false;

        if ((m == 4 || m == 6 || m == 9 || m == 11) && d > 30) return false;

        if (m == 2) {
            bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
            if (d > (leap ? 29 : 28)) return false;
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

string iso2dmy(const string& i) {
    return i.substr(8,2) + "." + i.substr(5,2) + "." + i.substr(0,4);
}

string iso2mdy(const string& i) {
    return i.substr(5,2) + "/" + i.substr(8,2) + "/" + i.substr(0,4);
}

vector<DateRecord> loadDates(const string& fname) {
    vector<DateRecord> res;
    ifstream f(fname.c_str());
    if (!f.is_open()) return res;

    string content;
    char ch;
    while (f.get(ch)) {
        content += ch;
    }
    f.close();

    size_t pos = 0;
    while ((pos = content.find("{", pos)) != string::npos) {
        size_t end = content.find("}", pos);
        if (end == string::npos) break;

        string obj = content.substr(pos, end - pos);
        DateRecord dr;

        size_t name_pos = obj.find("\"name\":\"");
        if (name_pos != string::npos) {
            name_pos += 8;
            size_t name_end = obj.find("\"", name_pos);
            if (name_end != string::npos)
                dr.name = obj.substr(name_pos, name_end - name_pos);
        }

        size_t date_pos = obj.find("\"date_iso\":\"");
        if (date_pos != string::npos) {
            date_pos += 12;
            size_t date_end = obj.find("\"", date_pos);
            if (date_end != string::npos)
                dr.iso = obj.substr(date_pos, date_end - date_pos);
        }

        dr.has_error = dr.name.empty() || dr.
            iso.empty();
        res.push_back(dr);

        pos = end + 1;
    }
    return res;
}

void generateMixedFiles(int n, int err_pct = 30) {
    srand((unsigned)time(NULL));

    for (int i = 0; i < n; ++i) {
        simple_json::array arr;
        bool has_errors = false;

        for (int j = 0; j < 10; ++j) {
            simple_json::object obj;
            bool make_err = (rand() % 100) < err_pct;

            obj.add("name", "record_" + to_string(i) + "_" + to_string(j));

            if (make_err) {
                has_errors = true;
                obj.add("date_iso", "2024-99-99");
            } else {
                int y = 2000 + rand() % 25;
                int m = 1 + rand() % 12;
                int d = 1 + rand() % 28;

                string ms = (m < 10 ? "0" : "") + to_string(m);
                string ds = (d < 10 ? "0" : "") + to_string(d);

                obj.add("date_iso", to_string(y) + "-" + ms + "-" + ds);
            }

            arr.add(obj);
        }

        string fname = (has_errors ? "mixed_data_" : "correct_data_") + to_string(i) + ".json";
        ofstream file(fname.c_str());
        if (file.is_open()) {
            file << arr.dump();
            file.close();
        }

        cout << "Создан файл: " << fname << "\n";
    }
}

void convert(int mode) {
    cout << "Имя файла: ";
    string fname;
    cin >> fname;

    auto start_load = chrono::high_resolution_clock::now();
    vector<DateRecord> data = loadDates(fname);
    auto end_load = chrono::high_resolution_clock::now();

    if (data.empty()) {
        cout << "Файл пуст или не найден!\n";
        return;
    }

    auto start_conv = chrono::high_resolution_clock::now();

    int errors = 0;
    int converted = 0;

    printHeader("РЕЗУЛЬТАТЫ КОНВЕРТАЦИИ");

    for (size_t i = 0; i < data.size(); ++i) {
        DateRecord& dr = data[i];

        if (dr.has_error) {
            cout << "[ОШИБКА] Некорректная запись\n";
            errors++;
            continue;
        }

        if (!validISO(dr.iso)) {
            cout << "[ОШИБКА] Неверная дата: " << dr.iso << "\n";
            errors++;
            continue;
        }

        if (mode == 2) {
            dr.dmy = iso2dmy(dr.iso);
            cout << dr.iso << " -> " << dr.dmy << "\n";
        } else {
            dr.mdy = iso2mdy(dr.iso);
            cout << dr.iso << " -> " << dr.mdy << "\n";
        }

        converted++;
    }

    auto end_conv = chrono::high_resolution_clock::now();

    auto load_t = chrono::duration_cast<chrono::milliseconds>(end_load - start_load);
    auto conv_t = chrono::duration_cast<chrono::milliseconds>(end_conv - start_conv);

    cout << "\n=== СВОДКА ===\n";
    cout << "Всего записей: " << data.size() << "\n";
    cout << "Конвертировано: " << converted << "\n";
    cout << "Ошибок: " << errors << "\n";
    cout << "Загрузка: " << load_t.count() << " мс\n";
    cout << "Конвертация: " << conv_t.count() << " мс\n";
}

int main() {
    setlocale(LC_ALL, "Russian");

    while (true) {
        help();
        cout << "Выбор: ";
        int c;
        if (!(cin >> c)) break;

        if (c == 1) {
            int n, p;
            cout << "Сколько файлов? ";
            cin >> n;
            cout << "Процент ошибок: ";
            cin >> p;
            generateMixedFiles(n, p);
        }
        else if (c == 2 || c == 3) {
            convert(c);
        }
        else if (c == 0) {
            break;
        }
    }
    return 0;
}

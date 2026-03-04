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
#include <random>

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
                r += "\\\"" + data[i].first + "\\\":\\\"" + data[i].second + "\\\"";
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
    bool conv = false, has_error = false, is_correct = true;
};

struct BenchmarkResult {
    int records_processed;
    long long load_time_ms, convert_time_ms, validation_time_ms, total_time_ms;
    double records_per_second;
};

vector<BenchmarkResult> benchmark_results;
int total_tests_run = 0, passed_tests = 0;

void printHeader(const string& t) {
    cout << "\n" << string(60, '=') << "\n  " << t << "\n" << string(60, '=') << "\n";
}

void printTableHeader() {
    cout << left << setw(20) << "Тест" << setw(15) << "Записей" << setw(15) << "Время (мс)" << setw(15) << "Статус\n" << string(65, '-') << "\n";
}

void help() {
    printHeader("КОНВЕРТЕР ДАТ");
    cout << "1) Генерация JSON (корректные + ошибки)\n2) Конвертация ISO->DD.MM.YYYY\n3) Конвертация ISO->MM/DD/YYYY\n4) Анализ файлов\n5) Самотесты\n6) Бенчмарк\n7) Отладка\n0) Выход\n";
}

bool validISO(const string& s) {
    if (s.length() != 10 || s[4] != '-' || s[7] != '-') return false;
    try {
        int y = stoi(s.substr(0, 4)), m = stoi(s.substr(5, 2)), d = stoi(s.substr(8, 2));
        if (y < 1900 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return false;
        if ((m == 4 || m == 6 || m == 9 || m == 11) && d > 30) return false;
        if (m == 2) {
            bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
            if (d > (leap ? 29 : 28)) return false;
        }
        return true;
    }
    catch (...) { return false; }
}

string iso2dmy(const string& i) { return validISO(i) ? i.substr(8, 2) + "." + i.substr(5, 2) + "." + i.substr(0, 4) : ""; }
string iso2mdy(const string& i) { return validISO(i) ? i.substr(5, 2) + "/" + i.substr(8, 2) + "/" + i.substr(0, 4) : ""; }

vector<DateRecord> loadDates(const string& fname) {
    vector<DateRecord> res;
    ifstream f(fname);
    if (!f.is_open()) return res;

    string line;
    while (getline(f, line)) {
        if (line.find('{') == string::npos) continue;
        DateRecord dr;
        size_t p;

        if ((p = line.find("\\\"name\\\":")) != string::npos) {
            if ((p = line.find('\\"', p + 7)) != string::npos) {
                size_t e = line.find('\\"', p + 1);
                if (e != string::npos) dr.name = line.substr(p + 1, e - p - 1);
            }
        }

        if ((p = line.find("\\\"date_iso\\\":")) != string::npos) {
            if ((p = line.find('\\"', p + 11)) != string::npos) {
                size_t e = line.find('\\"', p + 1);
                if (e != string::npos) dr.iso = line.substr(p + 1, e - p - 1);
            }
        }

        dr.has_error = dr.name.empty() || dr.iso.empty();
        res.push_back(dr);
    }
    return res;
}

void generateMixedFiles(int n, int err_pct = 30) {
    srand(static_cast<unsigned int>(time(nullptr)));
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> err_dist(0, 100), type_dist(0, 9);

    int correct_files = 0, error_files = 0;

    for (int i = 0; i < n; ++i) {
        simple_json::array arr;
        bool has_errors = false;
        int err_cnt = 0, corr_cnt = 0;

        for (int j = 0; j < 10; ++j) {
            simple_json::object obj;
            bool make_err = err_dist(gen) < err_pct;

            if (make_err) {
                has_errors = true;
                err_cnt++;
                obj.add("name", "error_record_" + to_string(i) + "_" + to_string(j));

                string bad_date;
                switch (type_dist(gen) % 8) {
                case 0: bad_date = "2024/12/31"; break;
                case 1: bad_date = "31-12-2024"; break;
                case 2: bad_date = "2024-13-45"; break;
                case 3: bad_date = "abcd-ef-gh"; break;
                case 4: bad_date = "2024-12"; break;
                case 5: bad_date = ""; break;
                case 6: bad_date = "2024-12-31-extra"; break;
                case 7: arr.add(obj); continue;
                }
                if (!bad_date.empty() && make_err) obj.add("date_iso", bad_date);
            }
            else {
                corr_cnt++;
                int y = 2000 + rand() % 25, m = 1 + rand() % 12, d;
                bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);

                if (m == 2) d = 1 + rand() % (leap ? 29 : 28);
                else if (m == 4 || m == 6 || m == 9 || m == 11) d = 1 + rand() % 30;
                else d = 1 + rand() % 31;

                string ms = (m < 10 ? "0" : "") + to_string(m);
                string ds = (d < 10 ? "0" : "") + to_string(d);
                obj.add("name", "record_" + to_string(i) + "_" + to_string(j));
                obj.add("date_iso", to_string(y) + "-" + ms + "-" + ds);
            }
            arr.add(obj);
        }

        string fname = (has_errors ? "mixed_data_" : "correct_data_") + to_string(i) + ".json";
        ofstream file(fname);
        if (file) {
            file << arr.dump();
            cout << "Создан " << (has_errors ? "СМЕШАННЫЙ (ошибок: " + to_string(err_cnt) + ")" : "КОРРЕКТНЫЙ")
                << " файл: " << fname << " (корректных: " << corr_cnt << ", ошибок: " << err_cnt << ")\n";
        }

        if (has_errors) error_files++; else correct_files++;
    }

    cout << "\n=== СВОДКА ГЕНЕРАЦИИ ===\nВсего файлов: " << n << "\nКорректных: " << correct_files << "\nС ошибками: " << error_files << "\nПроцент ошибок: " << err_pct << "%\n";
}

void convert(int mode) {
    cout << "Имя файла: "; string fname; cin >> fname;

    auto start_load = chrono::high_resolution_clock::now();
    auto data = loadDates(fname);
    auto end_load = chrono::high_resolution_clock::now();

    if (data.empty()) { cout << "Файл пуст или не найден!\n"; return; }

    auto start_conv = chrono::high_resolution_clock::now();
    int cnt = 0, errors = 0, correct = 0, valid_cnt = 0;
    vector<int> times;

    printHeader("РЕЗУЛЬТАТЫ КОНВЕРТАЦИИ");

    for (auto& dr : data) {
        auto rec_start = chrono::high_resolution_clock::now();

        if (dr.has_error) errors++;
        else if (mode == 2 && validISO(dr.iso)) { dr.dmy = iso2dmy(dr.iso); cnt++; valid_cnt++; correct++; }
        else if (mode == 3 && validISO(dr.iso)) { dr.mdy = iso2mdy(dr.iso); cnt++; valid_cnt++; correct++; }
        else if (!dr.iso.empty()) errors++;

        times.push_back(chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now() - rec_start).count());
    }

    auto end_conv = chrono::high_resolution_clock::now();

    if (!times.empty()) {
        sort(times.begin(), times.end());
        long long min_t = times.front(), max_t = times.back();
        double avg_t = accumulate(times.begin(), times.end(), 0.0) / times.size();
        double med_t = times.size() % 2 ? times[times.size() / 2] : (times[times.size() / 2 - 1] + times[times.size() / 2]) / 2.0;

        double sum_sq = 0;
        for (auto t : times) sum_sq += (t - avg_t) * (t - avg_t);
        double thresh = avg_t + 3 * sqrt(sum_sq / times.size());
        int anom = count_if(times.begin(), times.end(), [thresh](int t) { return t > thresh; });

        cout << "\n=== СТАТИСТИКА ВРЕМЕНИ ===\n"
            << left << setw(25) << "Минимальное:" << min_t << " мкс\n"
            << setw(25) << "Максимальное:" << max_t << " мкс\n"
            << setw(25) << "Среднее:" << fixed << setprecision(2) << avg_t << " мкс\n"
            << setw(25) << "Медиана:" << med_t << " мкс\n"
            << setw(25) << "Аномалий:" << anom << "\n"
            << setw(25) << "Порог:" << thresh << " мкс\n";
    }

    auto load_t = chrono::duration_cast<chrono::milliseconds>(end_load - start_load);
    auto conv_t = chrono::duration_cast<chrono::milliseconds>(end_conv - start_conv);
    auto total_t = load_t + conv_t;

    cout << "\n=== СВОДКА ===\n"
        << left << setw(30) << "Всего записей:" << data.size() << "\n"
        << setw(30) << "Корректных дат:" << correct << "\n"
        << setw(30) << "Конвертировано:" << cnt << "\n"
        << setw(30) << "Ошибок:" << errors << "\n"
        << setw(30) << "Загрузка:" << load_t.count() << " мс\n"
        << setw(30) << "Конвертация:" << conv_t.count() << " мс\n"
        << setw(30) << "Всего:" << total_t.count() << " мс\n";

    if (total_t.count() > 0)
        cout << setw(30) << "Записей/сек:" << fixed << setprecision(2) << (data.size() * 1000.0) / total_t.count() << "\n";

    cout << "\n=== ПРИМЕРЫ ===\n";
    for (int ex = 0, i = 0; i < data.size() && ex < 3; ++i)
        if (!data[i].iso.empty() && validISO(data[i].iso)) {
            cout << data[i].iso << " -> " << (mode == 2 ? iso2dmy(data[i].iso) : iso2mdy(data[i].iso)) << "\n";
            ex++;
        }
}

void analyze() {
    cout << "Сколько файлов? "; int n; cin >> n;
    if (n <= 0) return;

    int total = 0, valid = 0, errors = 0;
    int mixed = 0, correct_files = 0, error_files = 0;
    vector<long long> times;

    for (int i = 0; i < n; ++i) {
        auto start = chrono::high_resolution_clock::now();
        string fname;
        bool found = false;

        vector<string> names = { "mixed_data_", "correct_data_", "data_", "error_data_" };
        for (const auto& base : names) {
            fname = base + to_string(i) + ".json";
            if (ifstream(fname).good()) {
                found = true;
                if (base.find("mixed") != string::npos) mixed++;
                else if (base.find("correct") != string::npos || base == "data_") correct_files++;
                else error_files++;
                break;
            }
        }

        if (!found) { cout << "Файл " << i << " не найден\n"; continue; }

        auto data = loadDates(fname);
        times.push_back(chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count());

        total += data.size();
        for (auto& dr : data) validISO(dr.iso) ? valid++ : errors++;
    }

    printHeader("РЕЗУЛЬТАТЫ АНАЛИЗА");

    if (!times.empty()) {
        auto min_it = min_element(times.begin(), times.end());
        auto max_it = max_element(times.begin(), times.end());
        long long min_t = *min_it;
        long long max_t = *max_it;
        double avg_t = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "\n=== ПРОИЗВОДИТЕЛЬНОСТЬ ===\n"
            << left << setw(25) << "Минимальное:" << min_t << " мс\n"
            << setw(25) << "Максимальное:" << max_t << " мс\n"
            << setw(25) << "Среднее:" << fixed << setprecision(2) << avg_t << " мс\n";
    }

    cout << "\n=== ДАННЫЕ ===\n"
        << left << setw(25) << "Проверено файлов:" << n << "\n"
        << setw(25) << "Смешанных:" << mixed << "\n"
        << setw(25) << "Корректных:" << correct_files << "\n"
        << setw(25) << "С ошибками:" << error_files << "\n"
        << setw(25) << "Всего записей:" << total << "\n"
        << setw(25) << "Корректных дат:" << valid << "\n"
        << setw(25) << "Ошибок:" << errors << "\n";

    if (total > 0) {
        cout << setw(25) << "Корректных %:" << fixed << setprecision(1) << (valid * 100.0 / total) << "%\n"
            << setw(25) << "Ошибок %:" << (errors * 100.0 / total) << "%\n";
    }
}

void runSelfTests() {
    printHeader("САМОТЕСТЫ");
    total_tests_run = passed_tests = 0;
    printTableHeader();

    auto run_test = [&](string name, int cnt, auto test_func) {
        total_tests_run++;
        auto start = chrono::high_resolution_clock::now();
        bool passed = test_func();
        auto time = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now() - start).count();
        if (passed) passed_tests++;
        cout << left << setw(20) << name << setw(15) << cnt << setw(15) << time << setw(15) << (passed ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН") << "\n";
        };

    run_test("Валидация корр.", 3, [] { return validISO("2024-12-31") && validISO("2000-01-01") && validISO("2024-02-29"); });
    run_test("Валидация ошибок", 3, [] { return !validISO("2024-13-31") && !validISO("2024-12-32") && !validISO("abcd-ef-gh"); });
    run_test("Конвертация", 2, [] { return iso2dmy("2024-12-31") == "31.12.2024" && iso2mdy("2024-12-31") == "12/31/2024"; });

    total_tests_run++;
    auto start = chrono::high_resolution_clock::now();
    generateMixedFiles(1, 30);
    bool passed = ifstream("mixed_data_0.json").good() || ifstream("correct_data_0.json").good();
    auto time = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count();
    if (passed) passed_tests++;
    cout << left << setw(20) << "Генерация" << setw(15) << "1" << setw(15) << time << setw(15) << (passed ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН") << "\n";

    cout << string(65, '-') << "\n\nИТОГО: " << passed_tests << "/" << total_tests_run << " тестов\n"
        << "УСПЕШНОСТЬ: " << fixed << setprecision(1) << (passed_tests * 100.0 / total_tests_run) << "%\n";
}

void runBenchmark() {
    printHeader("БЕНЧМАРК");
    cout << "Сколько файлов? "; int n; cin >> n;
    if (n <= 0) return;

    generateMixedFiles(n, 30);
    BenchmarkResult r = { 0 };

    auto total_start = chrono::high_resolution_clock::now();
    auto load_start = chrono::high_resolution_clock::now();

    vector<vector<DateRecord>> all_data;
    for (int i = 0; i < n; ++i) {
        string fname = "mixed_data_" + to_string(i) + ".json";
        if (!ifstream(fname).good()) fname = "correct_data_" + to_string(i) + ".json";
        auto data = loadDates(fname);
        if (!data.empty()) {
            all_data.push_back(data);
            r.records_processed += data.size();
        }
    }
    r.load_time_ms = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - load_start).count();

    auto conv_start = chrono::high_resolution_clock::now();
    for (auto& data : all_data)
        for (auto& dr : data)
            if (validISO(dr.iso)) dr.dmy = iso2dmy(dr.iso);
    r.convert_time_ms = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - conv_start).count();

    auto valid_start = chrono::high_resolution_clock::now();
    int valid_cnt = 0, err_cnt = 0;
    for (auto& data : all_data)
        for (auto& dr : data)
            validISO(dr.iso) ? valid_cnt++ : (!dr.iso.empty() ? err_cnt++ : 0);
    r.validation_time_ms = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - valid_start).count();

    r.total_time_ms = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - total_start).count();
    r.records_per_second = r.total_time_ms ? (r.records_processed * 1000.0) / r.total_time_ms : 0;
    benchmark_results.push_back(r);

    cout << "\n=== РЕЗУЛЬТАТЫ ===\n"
        << left << setw(30) << "Файлов:" << n << "\n"
        << setw(30) << "Записей:" << r.records_processed << "\n"
        << setw(30) << "Корректных:" << valid_cnt << "\n"
        << setw(30) << "С ошибками:" << err_cnt << "\n"
        << setw(30) << "Загрузка:" << r.load_time_ms << " мс\n"
        << setw(30) << "Конвертация:" << r.convert_time_ms << " мс\n"
        << setw(30) << "Валидация:" << r.validation_time_ms << " мс\n"
        << setw(30) << "Всего:" << r.total_time_ms << " мс\n"
        << setw(30) << "Записей/сек:" << fixed << setprecision(2) << r.records_per_second << "\n";

    long long max_t = max({ r.load_time_ms, r.convert_time_ms, r.validation_time_ms });
    cout << "\n=== УЗКОЕ МЕСТО ===\n"
        << (max_t == r.load_time_ms ? "ЗАГРУЗКА" : max_t == r.convert_time_ms ? "КОНВЕРТАЦИЯ" : "ВАЛИДАЦИЯ")
        << " (" << max_t << " мс)\nРекомендация: "
        << (max_t == r.load_time_ms ? "Кэширование" : max_t == r.convert_time_ms ? "Оптимизация" : "Фильтрация") << "\n";
}

void debugMode() {
    printHeader("ОТЛАДКА");
    cout << "=== СИСТЕМА ===\n"
        << "int: " << sizeof(int) << " байт\n"
        << "string: " << sizeof(string) << " байт\n"
        << "vector: " << sizeof(vector<DateRecord>) << " байт\n"
        << "\n=== СОСТОЯНИЕ ===\n"
        << "Тестов: " << total_tests_run << "\nПройдено: " << passed_tests << "\nБенчмарков: " << benchmark_results.size() << "\n";

    if (!benchmark_results.empty()) {
        auto& last = benchmark_results.back();
        cout << "\n=== ПОСЛЕДНИЙ ===\n"
            << "Записей: " << last.records_processed << "\n"
            << "Время: " << last.total_time_ms << " мс\n"
            << "Произв.: " << fixed << setprecision(2) << last.records_per_second << " зап/сек\n";
    }

    cout << "\n=== ФАЙЛЫ ===\n";
    system("dir *.json 2>nul || ls *.json 2>/dev/null || echo 'Нет файлов'");
}

int main() {
    setlocale(LC_ALL, "Russian");
    srand(static_cast<unsigned int>(time(nullptr)));

    while (true) {
        help();
        cout << "Выбор: ";
        int c; if (!(cin >> c)) { cin.clear(); cin.ignore(1000, '\n'); cout << "Неверный ввод!\n"; continue; }

        if (c == 1) {
            int n, p; cout << "Сколько файлов? "; cin >> n;
            if (n > 0) {
                cout << "Процент ошибок (0-100): "; cin >> p;
                generateMixedFiles(n, max(0, min(100, p)));
            }
        }
        else if (c == 2 || c == 3) convert(c);
        else if (c == 4) analyze();
        else if (c == 5) runSelfTests();
        else if (c == 6) runBenchmark();
        else if (c == 7) debugMode();
        else if (c == 0) { cout << "Выход\n"; break; }
        else cout << "Неверный выбор!\n";

        cin.ignore(1000, '\n');
    }
    return 0;
}

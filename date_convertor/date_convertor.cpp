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

// Простая JSON-реализация
namespace simple_json {
    class object {
        vector<pair<string, string>> data;
    public:
        void add(const string& key, const string& val) {
            data.emplace_back(key, val);
        }
        string dump() const {
            string result = "{";
            for (size_t i = 0; i < data.size(); ++i) {
                if (i > 0) result += ",";
                result += "\"" + data[i].first + "\":\"" + data[i].second + "\"";
            }
            result += "}";
            return result;
        }
    };

    class array {
        vector<object> objects;
    public:
        void add(const object& obj) {
            objects.push_back(obj);
        }
        string dump() const {
            string result = "[";
            for (size_t i = 0; i < objects.size(); ++i) {
                if (i > 0) result += ",";
                result += objects[i].dump();
            }
            result += "]";
            return result;
        }
    };
}

struct DateRecord {
    string name;
    string iso;
    string dmy;
    string mdy;
    bool conv = false;
    bool has_error = false;
    bool is_correct = true;  // Новое поле для отслеживания корректности
};

struct BenchmarkResult {
    int records_processed;
    long long load_time_ms;
    long long convert_time_ms;
    long long validation_time_ms;
    long long total_time_ms;
    double records_per_second;
};

// Глобальные переменные для тестирования
vector<BenchmarkResult> benchmark_results;
int total_tests_run = 0;
int passed_tests = 0;

void printHeader(const string& title) {
    cout << "\n" << string(60, '=') << endl;
    cout << "  " << title << endl;
    cout << string(60, '=') << endl;
}

void printTableHeader() {
    cout << left << setw(20) << "Тест"
        << setw(15) << "Записей"
        << setw(15) << "Время (мс)"
        << setw(15) << "Статус" << endl;
    cout << string(65, '-') << endl;
}

void help() {
    printHeader("КОНВЕРТЕР ДАТ");
    cout << "1) Генерация смешанных JSON файлов (корректные + ошибки)\n"
        << "2) Конвертация ISO->DD.MM.YYYY\n"
        << "3) Конвертация ISO->MM/DD/YYYY\n"
        << "4) Анализ файлов\n"
        << "5) Запуск самотестов\n"
        << "6) Бенчмарк производительности\n"
        << "7) Режим отладки\n"
        << "0) Выход из программы\n";
}

bool validISO(const string& s) {
    if (s.length() != 10) return false;
    if (s[4] != '-' || s[7] != '-') return false;

    try {
        int year = stoi(s.substr(0, 4));
        int month = stoi(s.substr(5, 2));
        int day = stoi(s.substr(8, 2));

        if (year < 1900 || year > 2100) return false;
        if (month < 1 || month > 12) return false;
        if (day < 1 || day > 31) return false;

        if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) return false;
        if (month == 2) {
            bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            if (day > (isLeap ? 29 : 28)) return false;
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

string iso2dmy(const string& iso) {
    if (!validISO(iso)) return "";
    return iso.substr(8, 2) + "." + iso.substr(5, 2) + "." + iso.substr(0, 4);
}

string iso2mdy(const string& iso) {
    if (!validISO(iso)) return "";
    return iso.substr(5, 2) + "/" + iso.substr(8, 2) + "/" + iso.substr(0, 4);
}

vector<DateRecord> loadDates(const string& fname) {
    vector<DateRecord> res;
    ifstream f(fname);
    if (!f.is_open()) {
        return res;
    }

    string line;
    while (getline(f, line)) {
        if (line.find('{') != string::npos) {
            DateRecord dr;
            dr.has_error = false;

            size_t name_pos = line.find("\"name\":");
            if (name_pos != string::npos) {
                name_pos = line.find('\"', name_pos + 7);
                if (name_pos != string::npos) {
                    size_t name_end = line.find('\"', name_pos + 1);
                    if (name_end != string::npos) {
                        dr.name = line.substr(name_pos + 1, name_end - name_pos - 1);
                    }
                }
            }

            size_t date_pos = line.find("\"date_iso\":");
            if (date_pos != string::npos) {
                date_pos = line.find('\"', date_pos + 11);
                if (date_pos != string::npos) {
                    size_t date_end = line.find('\"', date_pos + 1);
                    if (date_end != string::npos) {
                        dr.iso = line.substr(date_pos + 1, date_end - date_pos - 1);
                    }
                }
            }

            if (dr.name.empty() || dr.iso.empty()) {
                dr.has_error = true;
            }

            res.push_back(dr);
        }
    }

    return res;
}

void generateMixedFiles(int n, int error_percentage = 30) {
    srand(static_cast<unsigned int>(time(nullptr)));
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> error_dist(0, 100);
    uniform_int_distribution<> error_type_dist(0, 9);

    int correct_files = 0;
    int error_files = 0;

    for (int i = 0; i < n; i++) {
        simple_json::array arr;
        bool file_has_errors = false;
        int error_count = 0;
        int correct_count = 0;

        for (int j = 0; j < 10; j++) {
            simple_json::object obj;
            
            // Решаем, будет ли эта запись с ошибкой
            bool make_error = (error_dist(gen) < error_percentage);
            
            if (make_error) {
                file_has_errors = true;
                error_count++;
                obj.add("name", "error_record_" + to_string(i) + "_" + to_string(j));
                
                int error_type = error_type_dist(gen);
                string bad_date;
                
                switch (error_type % 8) {
                    case 0: bad_date = "2024/12/31"; break;  // неправильный разделитель
                    case 1: bad_date = "31-12-2024"; break;  // европейский формат
                    case 2: bad_date = "2024-13-45"; break;  // несуществующий месяц/день
                    case 3: bad_date = "abcd-ef-gh"; break;  // некорректные символы
                    case 4: bad_date = "2024-12"; break;     // неполная дата
                    case 5: bad_date = ""; break;           // пустая строка
                    case 6: bad_date = "2024-12-31-extra"; break;  // лишние символы
                    case 7: 
                        // пропущенное поле date_iso
                        obj.add("name", "error_record_" + to_string(i) + "_" + to_string(j));
                        // не добавляем date_iso
                        arr.add(obj);
                        continue;
                }
                
                if (!bad_date.empty() && error_type != 7) {
                    obj.add("date_iso", bad_date);
                }
            } else {
                correct_count++;
                int y = 2000 + rand() % 25;
                int m = 1 + rand() % 12;
                int d;

                if (m == 2) {
                    bool isLeap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
                    d = 1 + rand() % (isLeap ? 29 : 28);
                }
                else if (m == 4 || m == 6 || m == 9 || m == 11) {
                    d = 1 + rand() % 30;
                }
                else {
                    d = 1 + rand() % 31;
                }

                string month_str = (m < 10) ? "0" + to_string(m) : to_string(m);
                string day_str = (d < 10) ? "0" + to_string(d) : to_string(d);

                string iso = to_string(y) + "-" + month_str + "-" + day_str;

                obj.add("name", "record_" + to_string(i) + "_" + to_string(j));
                obj.add("date_iso", iso);
            }
            
            arr.add(obj);
        }

        string filename;
        string file_type;
        
        if (file_has_errors) {
            filename = "mixed_data_" + to_string(i) + ".json";
            file_type = "СМЕШАННЫЙ (ошибок: " + to_string(error_count) + ")";
            error_files++;
        } else {
            filename = "correct_data_" + to_string(i) + ".json";
            file_type = "КОРРЕКТНЫЙ";
            correct_files++;
        }
        
        ofstream file(filename);
        if (file) {
            file << arr.dump();
            cout << "Создан " << file_type << " файл: " << filename 
                 << " (корректных: " << correct_count 
                 << ", ошибок: " << error_count << ")" << endl;
        }
    }
    
    cout << "\n=== СВОДКА ГЕНЕРАЦИИ ===\n";
    cout << "Всего создано файлов: " << n << endl;
    cout << "Корректных файлов: " << correct_files << endl;
    cout << "Файлов с ошибками: " << error_files << endl;
    cout << "Процент ошибок в смешанных файлах: " << error_percentage << "%\n";
}

void convert(int mode) {
    cout << "Имя файла: ";
    string fname;
    cin >> fname;

    auto start_load = chrono::high_resolution_clock::now();
    auto data = loadDates(fname);
    auto end_load = chrono::high_resolution_clock::now();

    if (data.empty()) {
        cout << "Файл пуст или не найден!\n";
        return;
    }

    auto start_convert = chrono::high_resolution_clock::now();
    int cnt = 0;
    int errors = 0;
    int correct_dates = 0;

    printHeader("РЕЗУЛЬТАТЫ КОНВЕРТАЦИИ");

    // Расчет статистики
    vector<int> processing_times;
    int valid_count = 0;

    for (auto& dr : data) {
        auto start_record = chrono::high_resolution_clock::now();

        if (dr.has_error) {
            errors++;
        }
        else if (mode == 2 && validISO(dr.iso)) {  // ISO->DD.MM.YYYY
            dr.dmy = iso2dmy(dr.iso);
            cnt++;
            valid_count++;
            correct_dates++;
        }
        else if (mode == 3 && validISO(dr.iso)) {  // ISO->MM/DD/YYYY
            dr.mdy = iso2mdy(dr.iso);
            cnt++;
            valid_count++;
            correct_dates++;
        }
        else if (!dr.iso.empty()) {
            errors++;
        }

        auto end_record = chrono::high_resolution_clock::now();
        processing_times.push_back(chrono::duration_cast<chrono::microseconds>(end_record - start_record).count());
    }

    auto end_convert = chrono::high_resolution_clock::now();

    // Расчет статистики
    if (!processing_times.empty()) {
        sort(processing_times.begin(), processing_times.end());
        long long min_time = processing_times.front();
        long long max_time = processing_times.back();
        double avg_time = accumulate(processing_times.begin(), processing_times.end(), 0.0) / processing_times.size();

        double median_time;
        size_t size = processing_times.size();
        if (size % 2 == 0) {
            median_time = (processing_times[size / 2 - 1] + processing_times[size / 2]) / 2.0;
        }
        else {
            median_time = processing_times[size / 2];
        }

        // Обнаружение аномалий (больше чем 3 стандартных отклонения от среднего)
        double sum_sq = 0;
        for (auto t : processing_times) {
            sum_sq += (t - avg_time) * (t - avg_time);
        }
        double std_dev = sqrt(sum_sq / processing_times.size());
        double threshold = avg_time + 3 * std_dev;

        int anomalies = 0;
        for (auto t : processing_times) {
            if (t > threshold) anomalies++;
        }

        // Вывод статистики
        cout << "\n=== СТАТИСТИКА ВРЕМЕНИ ОБРАБОТКИ ===\n";
        cout << left << setw(25) << "Минимальное время:" << min_time << " мкс\n";
        cout << left << setw(25) << "Максимальное время:" << max_time << " мкс\n";
        cout << left << setw(25) << "Среднее время:" << fixed << setprecision(2) << avg_time << " мкс\n";
        cout << left << setw(25) << "Медиана:" << fixed << setprecision(2) << median_time << " мкс\n";
        cout << left << setw(25) << "Найдено аномалий:" << anomalies << " записей\n";
        cout << left << setw(25) << "Порог аномалий:" << fixed << setprecision(2) << threshold << " мкс\n";
    }

    auto load_time = chrono::duration_cast<chrono::milliseconds>(end_load - start_load);
    auto convert_time = chrono::duration_cast<chrono::milliseconds>(end_convert - start_convert);
    auto total_time = load_time + convert_time;

    cout << "\n=== СВОДКА ===\n";
    cout << left << setw(30) << "Всего записей:" << data.size() << endl;
    cout << left << setw(30) << "Корректных дат:" << correct_dates << endl;
    cout << left << setw(30) << "Конвертировано:" << cnt << endl;
    cout << left << setw(30) << "Найдено ошибок:" << errors << endl;
    cout << left << setw(30) << "Время загрузки:" << load_time.count() << " мс\n";
    cout << left << setw(30) << "Время конвертации:" << convert_time.count() << " мс\n";
    cout << left << setw(30) << "Общее время:" << total_time.count() << " мс\n";

    if (total_time.count() > 0) {
        double records_per_second = (data.size() * 1000.0) / total_time.count();
        cout << left << setw(30) << "Записей в секунду:" << fixed << setprecision(2) << records_per_second << endl;
    }
    
    // Вывод примера конвертации
    cout << "\n=== ПРИМЕР КОНВЕРТАЦИИ ===\n";
    int examples_shown = 0;
    for (const auto& dr : data) {
        if (examples_shown >= 3) break;
        if (!dr.iso.empty() && validISO(dr.iso)) {
            if (mode == 2) {
                cout << dr.iso << " -> " << iso2dmy(dr.iso) << endl;
            } else if (mode == 3) {
                cout << dr.iso << " -> " << iso2mdy(dr.iso) << endl;
            }
            examples_shown++;
        }
    }
}

void analyze() {
    cout << "Сколько файлов проанализировать? ";
    int n;
    cin >> n;

    if (n <= 0) return;

    int total = 0, valid = 0, errors = 0;
    int mixed_files = 0, correct_files = 0, error_files = 0;
    vector<long long> processing_times;

    for (int i = 0; i < n; i++) {
        auto start = chrono::high_resolution_clock::now();

        // Пробуем разные имена файлов
        string filename;
        bool file_found = false;
        
        // Пробуем mixed_data
        filename = "mixed_data_" + to_string(i) + ".json";
        ifstream test1(filename);
        if (test1.good()) {
            file_found = true;
            mixed_files++;
        }
        
        // Пробуем correct_data
        if (!file_found) {
            filename = "correct_data_" + to_string(i) + ".json";
            ifstream test2(filename);
            if (test2.good()) {
                file_found = true;
                correct_files++;
            }
        }
        
        // Пробуем data_ (старый формат)
        if (!file_found) {
            filename = "data_" + to_string(i) + ".json";
            ifstream test3(filename);
            if (test3.good()) {
                file_found = true;
                correct_files++;
            }
        }
        
        // Пробуем error_data_ (старый формат)
        if (!file_found) {
            filename = "error_data_" + to_string(i) + ".json";
            ifstream test4(filename);
            if (test4.good()) {
                file_found = true;
                error_files++;
            }
        }

        if (!file_found) {
            cout << "Файл с индексом " << i << " не найден, пропускаем...\n";
            continue;
        }

        auto data = loadDates(filename);
        auto end = chrono::high_resolution_clock::now();
        processing_times.push_back(chrono::duration_cast<chrono::milliseconds>(end - start).count());

        if (!data.empty()) {
            total += data.size();
            for (auto& dr : data) {
                if (validISO(dr.iso)) {
                    valid++;
                }
                else {
                    errors++;
                }
            }
        }
    }

    printHeader("РЕЗУЛЬТАТЫ АНАЛИЗА");

    if (!processing_times.empty()) {
        cout << "\n=== ПРОИЗВОДИТЕЛЬНОСТЬ ===\n";
        long long min_time = *min_element(processing_times.begin(), processing_times.end());
        long long max_time = *max_element(processing_times.begin(), processing_times.end());
        double avg_time = accumulate(processing_times.begin(), processing_times.end(), 0.0) / processing_times.size();

        cout << left << setw(25) << "Минимальное время:" << min_time << " мс\n";
        cout << left << setw(25) << "Максимальное время:" << max_time << " мс\n";
        cout << left << setw(25) << "Среднее время:" << fixed << setprecision(2) << avg_time << " мс\n";
    }

    cout << "\n=== ДАННЫЕ ===\n";
    cout << left << setw(25) << "Проверено файлов:" << n << endl;
    cout << left << setw(25) << "Смешанных файлов:" << mixed_files << endl;
    cout << left << setw(25) << "Корректных файлов:" << correct_files << endl;
    cout << left << setw(25) << "Файлов с ошибками:" << error_files << endl;
    cout << left << setw(25) << "Всего записей:" << total << endl;
    cout << left << setw(25) << "Корректных дат:" << valid << endl;
    cout << left << setw(25) << "Ошибок:" << errors << endl;

    if (total > 0) {
        cout << left << setw(25) << "Процент корректных:"
            << fixed << setprecision(1) << (valid * 100.0 / total) << "%" << endl;
        cout << left << setw(25) << "Процент ошибок:"
            << fixed << setprecision(1) << (errors * 100.0 / total) << "%" << endl;
    }
}

void runSelfTests() {
    printHeader("ЗАПУСК САМОТЕСТОВ");
    total_tests_run = 0;
    passed_tests = 0;

    printTableHeader();

    // Тест 1: Валидация корректных дат
    total_tests_run++;
    auto start = chrono::high_resolution_clock::now();
    bool test1 = validISO("2024-12-31") && validISO("2000-01-01") && validISO("2024-02-29");
    auto end = chrono::high_resolution_clock::now();
    auto time1 = chrono::duration_cast<chrono::microseconds>(end - start).count();

    if (test1) passed_tests++;
    cout << left << setw(20) << "Валидация корректных"
        << setw(15) << "3"
        << setw(15) << time1
        << setw(15) << (test1 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН") << endl;

    // Тест 2: Валидация некорректных дат
    total_tests_run++;
    start = chrono::high_resolution_clock::now();
    bool test2 = !validISO("2024-13-31") && !validISO("2024-12-32") && !validISO("abcd-ef-gh");
    end = chrono::high_resolution_clock::now();
    auto time2 = chrono::duration_cast<chrono::microseconds>(end - start).count();

    if (test2) passed_tests++;
    cout << left << setw(20) << "Валидация ошибок"
        << setw(15) << "3"
        << setw(15) << time2
        << setw(15) << (test2 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН") << endl;

    // Тест 3: Конвертация форматов
    total_tests_run++;
    start = chrono::high_resolution_clock::now();
    bool test3 = (iso2dmy("2024-12-31") == "31.12.2024") &&
        (iso2mdy("2024-12-31") == "12/31/2024");
    end = chrono::high_resolution_clock::now();
    auto time3 = chrono::duration_cast<chrono::microseconds>(end - start).count();

    if (test3) passed_tests++;
    cout << left << setw(20) << "Конвертация"
        << setw(15) << "2"
        << setw(15) << time3
        << setw(15) << (test3 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН") << endl;

    // Тест 4: Генерация смешанных файлов
    total_tests_run++;
    start = chrono::high_resolution_clock::now();
    generateMixedFiles(1, 30);  // Создаем 1 файл для теста
    ifstream test_file1("mixed_data_0.json");
    ifstream test_file2("correct_data_0.json");
    bool test4 = test_file1.good() || test_file2.good();
    end = chrono::high_resolution_clock::now();
    auto time4 = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    if (test4) passed_tests++;
    cout << left << setw(20) << "Генерация файлов"
        << setw(15) << "1"
        << setw(15) << time4
        << setw(15) << (test4 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН") << endl;

    cout << string(65, '-') << endl;
    cout << "\nИТОГО: " << passed_tests << "/" << total_tests_run << " тестов пройдено\n";
    cout << "УСПЕШНОСТЬ: " << fixed << setprecision(1)
        << (passed_tests * 100.0 / total_tests_run) << "%\n";
}

void runBenchmark() {
    printHeader("БЕНЧМАРК ПРОИЗВОДИТЕЛЬНОСТИ");

    cout << "Сколько файлов для теста? ";
    int n;
    cin >> n;

    if (n <= 0) return;

    // Создаем тестовые файлы (смешанные)
    generateMixedFiles(n, 30);  // 30% ошибок

    BenchmarkResult result;
    result.records_processed = 0;

    auto total_start = chrono::high_resolution_clock::now();

    // Тест загрузки
    auto load_start = chrono::high_resolution_clock::now();
    vector<vector<DateRecord>> all_data;
    for (int i = 0; i < n; i++) {
        // Пробуем разные имена файлов
        string filename;
        
        filename = "mixed_data_" + to_string(i) + ".json";
        ifstream test1(filename);
        if (!test1.good()) {
            filename = "correct_data_" + to_string(i) + ".json";
        }
        
        auto data = loadDates(filename);
        if (!data.empty()) {
            all_data.push_back(data);
            result.records_processed += data.size();
        }
    }
    auto load_end = chrono::high_resolution_clock::now();
    result.load_time_ms = chrono::duration_cast<chrono::milliseconds>(load_end - load_start).count();

    // Тест конвертации
    auto convert_start = chrono::high_resolution_clock::now();
    int converted = 0;
    for (auto& data : all_data) {
        for (auto& dr : data) {
            if (validISO(dr.iso)) {
                dr.dmy = iso2dmy(dr.iso);
                converted++;
            }
        }
    }
    auto convert_end = chrono::high_resolution_clock::now();
    result.convert_time_ms = chrono::duration_cast<chrono::milliseconds>(convert_end - convert_start).count();

    // Тест валидации
    auto valid_start = chrono::high_resolution_clock::now();
    int valid_count = 0;
    int error_count = 0;
    for (auto& data : all_data) {
        for (auto& dr : data) {
            if (validISO(dr.iso)) {
                valid_count++;
            } else if (!dr.iso.empty()) {
                error_count++;
            }
        }
    }
    auto valid_end = chrono::high_resolution_clock::now();
    result.validation_time_ms = chrono::duration_cast<chrono::milliseconds>(valid_end - valid_start).count();

    auto total_end = chrono::high_resolution_clock::now();
    result.total_time_ms = chrono::duration_cast<chrono::milliseconds>(total_end - total_start).count();

    if (result.total_time_ms > 0) {
        result.records_per_second = (result.records_processed * 1000.0) / result.total_time_ms;
    }
    else {
        result.records_per_second = 0;
    }

    benchmark_results.push_back(result);

    // Вывод результатов
    cout << "\n=== РЕЗУЛЬТАТЫ БЕНЧМАРКА ===\n";
    cout << left << setw(30) << "Файлов обработано:" << n << endl;
    cout << left << setw(30) << "Записей обработано:" << result.records_processed << endl;
    cout << left << setw(30) << "Корректных записей:" << valid_count << endl;
    cout << left << setw(30) << "Записей с ошибками:" << error_count << endl;
    cout << left << setw(30) << "Время загрузки:" << result.load_time_ms << " мс\n";
    cout << left << setw(30) << "Время конвертации:" << result.convert_time_ms << " мс\n";
    cout << left << setw(30) << "Время валидации:" << result.validation_time_ms << " мс\n";
    cout << left << setw(30) << "Общее время:" << result.total_time_ms << " мс\n";
    cout << left << setw(30) << "Записей в секунду:" << fixed << setprecision(2) << result.records_per_second << endl;

    // Анализ узкого места
    cout << "\n=== АНАЛИЗ УЗКОГО МЕСТА ===\n";
    long long max_time = max({ result.load_time_ms, result.convert_time_ms, result.validation_time_ms });

    if (max_time == result.load_time_ms) {
        cout << "Узкое место: ЗАГРУЗКА ФАЙЛОВ (" << result.load_time_ms << " мс)\n";
        cout << "Рекомендация: Кэширование, потоковая загрузка\n";
    }
    else if (max_time == result.convert_time_ms) {
        cout << "Узкое место: КОНВЕРТАЦИЯ (" << result.convert_time_ms << " мс)\n";
        cout << "Рекомендация: Векторизация, оптимизация алгоритмов\n";
    }
    else {
        cout << "Узкое место: ВАЛИДАЦИЯ (" << result.validation_time_ms << " мс)\n";
        cout << "Рекомендация: Предварительная фильтрация, кэширование\n";
    }
}

void debugMode() {
    printHeader("РЕЖИМ ОТЛАДКИ");

    cout << "=== ИНФОРМАЦИЯ О СИСТЕМЕ ===\n";
    cout << "Размер int: " << sizeof(int) << " байт\n";
    cout << "Размер string: " << sizeof(string) << " байт\n";
    cout << "Размер vector: " << sizeof(vector<DateRecord>) << " байт\n";

    cout << "\n=== СОСТОЯНИЕ ПРОГРАММЫ ===\n";
    cout << "Всего запущено тестов: " << total_tests_run << endl;
    cout << "Пройдено тестов: " << passed_tests << endl;
    cout << "Результатов бенчмарка: " << benchmark_results.size() << endl;

    if (!benchmark_results.empty()) {
        cout << "\n=== ПОСЛЕДНИЙ БЕНЧМАРК ===\n";
        auto& last = benchmark_results.back();
        cout << "Записей: " << last.records_processed << endl;
        cout << "Общее время: " << last.total_time_ms << " мс\n";
        cout << "Производительность: " << fixed << setprecision(2) << last.records_per_second << " зап/сек\n";
    }

    cout << "\n=== ФАЙЛЫ В ПАПКЕ ===\n";
    system("dir *.json 2>nul || ls *.json 2>/dev/null || echo 'Не удалось получить список файлов'");
}

int main() {
    setlocale(LC_ALL, "Russian");
    srand(static_cast<unsigned int>(time(nullptr)));

    while (true) {
        help();
        cout << "Выбор: ";

        int choice;
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "Неверный ввод!\n";
            continue;
        }

        if (choice == 1) {
            cout << "Сколько файлов создать? ";
            int n;
            cin >> n;
            if (n > 0) {
                cout << "Процент ошибок в файлах (0-100): ";
                int error_percent;
                cin >> error_percent;
                if (error_percent < 0) error_percent = 0;
                if (error_percent > 100) error_percent = 100;
                generateMixedFiles(n, error_percent);
            }
        }
        else if (choice == 2 || choice == 3) {
            convert(choice);
        }
        else if (choice == 4) {
            analyze();
        }
        else if (choice == 5) {
            runSelfTests();
        }
        else if (choice == 6) {
            runBenchmark();
        }
        else if (choice == 7) {
            debugMode();
        }
        else if (choice == 0) {
            cout << "Выход из программы.\n";
            break;
        }
        else {
            cout << "Неверный выбор!\n";
        }

        cin.ignore(1000, '\n');
    }

    return 0;
}

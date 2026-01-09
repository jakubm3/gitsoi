#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <string>
#include <atomic> // Potrzebne do liczników statystyk
#include <iomanip> // Do ładnego formatowania tabeli

// --- Implementacja Semafora ---
class Semaphore {
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;

public:
    Semaphore(int init_count = 0) : count(init_count) {}

    void wait() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return count > 0; });
        count--;
    }

    void signal() {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        cv.notify_one();
    }
};

// --- Struktura Bufora ---
struct Buffer {
    std::string name;
    std::vector<int> data;
    int size;
    int head = 0; 
    int tail = 0; 
    
    Semaphore sem_empty;
    Semaphore sem_full;
    std::mutex mtx_access;

    Buffer(std::string n, int N) : name(n), size(N), sem_empty(N), sem_full(0) {
        data.resize(N);
    }

    void push(int item) {
        sem_empty.wait();
        {
            std::lock_guard<std::mutex> lock(mtx_access);
            data[head] = item;
            head = (head + 1) % size;
        }
        sem_full.signal();
    }

    int pop() {
        int item;
        {
            std::lock_guard<std::mutex> lock(mtx_access);
            item = data[tail];
            tail = (tail + 1) % size;
        }
        return item;
    }
    
    void signal_space_freed() {
        sem_empty.signal();
    }
};

// --- Funkcja Producenta ---
// Dodano argument: counter (referencja do licznika statystyk)
void producer_thread(Buffer& buffer, int id, std::atomic<int>& counter) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sleep_dist(50, 200); // Szybsza produkcja dla testu

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(gen)));
        int product = rand() % 100;
        
        buffer.push(product);
        counter++; // Zwiększ statystykę wyprodukowanych surowców

        // Opcjonalnie wyłączamy logi producenta, żeby nie śmiecić, lub zostawiamy
         static std::mutex console_mtx;
         {
             std::lock_guard<std::mutex> lock(console_mtx);
             std::cout << " -> [P " << id << "] " << buffer.name << std::endl;
         }
    }
}

// --- Funkcja Konsumenta ---
// Dodano argument: counter (referencja do licznika zrobionych pierogów)
void consumer_thread(Buffer& doughBuf, Buffer& fillingBuf, std::string dumplingType, int id, std::atomic<int>& counter) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sleep_dist(200, 500);

    while (true) {
        fillingBuf.sem_full.wait();
        doughBuf.sem_full.wait();

        int fillingItem = fillingBuf.pop();
        int doughItem = doughBuf.pop();

        fillingBuf.signal_space_freed();
        doughBuf.signal_space_freed();
        
        counter++; // Zwiększ statystykę zrobionych pierogów

        static std::mutex console_mtx;
        {
            std::lock_guard<std::mutex> lock(console_mtx);
            std::cout << " <- [K " << id << "] Pierog: " << dumplingType << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(gen)));
    }
}

int main() {
    int N = 5; // Domyślny rozmiar bufora
    int producers_count = 1;
    int consumers_count = 2;
    int simulation_time = 10; // Czas w sekundach

    std::cout << "Podaj czas symulacji w sekundach: ";
    std::cin >> simulation_time;
    if (simulation_time <= 0) simulation_time = 5;

    std::cout << "\n--- Start Symulacji na " << simulation_time << " sekund ---\n" << std::endl;

    // --- Liczniki Statystyk (Atomic) ---
    std::atomic<int> stats_dough(0);
    std::atomic<int> stats_meat(0);
    std::atomic<int> stats_cheese(0);
    std::atomic<int> stats_cabbage(0);

    std::atomic<int> stats_pierogi_meat(0);
    std::atomic<int> stats_pierogi_cheese(0);
    std::atomic<int> stats_pierogi_cabbage(0);

    // Inicjalizacja buforów
    Buffer bufDough("CIASTO", N);
    Buffer bufMeat("MIESO", N);
    Buffer bufCheese("SER", N);
    Buffer bufCabbage("KAPUSTA", N);

    std::vector<std::thread> threads;

    // --- Uruchomienie Producentów (przekazujemy odpowiedni licznik) ---
    for (int i = 0; i < producers_count; ++i) 
        threads.emplace_back(producer_thread, std::ref(bufDough), i, std::ref(stats_dough));
    
    for (int i = 0; i < producers_count; ++i) 
        threads.emplace_back(producer_thread, std::ref(bufMeat), i, std::ref(stats_meat));

    for (int i = 0; i < producers_count; ++i) 
        threads.emplace_back(producer_thread, std::ref(bufCheese), i, std::ref(stats_cheese));

    for (int i = 0; i < producers_count; ++i) 
        threads.emplace_back(producer_thread, std::ref(bufCabbage), i, std::ref(stats_cabbage));

    // --- Uruchomienie Konsumentów ---
    for (int i = 0; i < consumers_count; ++i)
        threads.emplace_back(consumer_thread, std::ref(bufDough), std::ref(bufMeat), "MIESEM", i, std::ref(stats_pierogi_meat));

    for (int i = 0; i < consumers_count; ++i)
        threads.emplace_back(consumer_thread, std::ref(bufDough), std::ref(bufCheese), "SEREM", i, std::ref(stats_pierogi_cheese));

    for (int i = 0; i < consumers_count; ++i)
        threads.emplace_back(consumer_thread, std::ref(bufDough), std::ref(bufCabbage), "KAPUSTA", i, std::ref(stats_pierogi_cabbage));


    // --- Główny wątek czeka (Timer) ---
    std::this_thread::sleep_for(std::chrono::seconds(simulation_time));

    // --- PODSUMOWANIE ---
    // Blokujemy konsolę, żeby nic się nie wcięło
    std::cout << "\n\n==========================================" << std::endl;
    std::cout << "         KONIEC CZASU - PODSUMOWANIE       " << std::endl;
    std::cout << "==========================================" << std::endl;
    
    std::cout << std::left << std::setw(20) << "SUROWIEC" << " | " << "WYPRODUKOWANO" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << std::left << std::setw(20) << "Ciasto" << " | " << stats_dough << std::endl;
    std::cout << std::left << std::setw(20) << "Mieso" << " | " << stats_meat << std::endl;
    std::cout << std::left << std::setw(20) << "Ser" << " | " << stats_cheese << std::endl;
    std::cout << std::left << std::setw(20) << "Kapusta" << " | " << stats_cabbage << std::endl;
    
    std::cout << "\n------------------------------------------" << std::endl;
    
    std::cout << std::left << std::setw(20) << "TYP PIEROGA" << " | " << "UGOTOWANO (ZJEDZONO)" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << std::left << std::setw(20) << "Z Miesem" << " | " << stats_pierogi_meat << std::endl;
    std::cout << std::left << std::setw(20) << "Z Serem" << " | " << stats_pierogi_cheese << std::endl;
    std::cout << std::left << std::setw(20) << "Z Kapusta" << " | " << stats_pierogi_cabbage << std::endl;
    
    int total_pierogi = stats_pierogi_meat + stats_pierogi_cheese + stats_pierogi_cabbage;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << std::left << std::setw(20) << "RAZEM PIEROGOW" << " | " << total_pierogi << std::endl;
    std::cout << "==========================================" << std::endl;

    // Brutalne zakończenie programu (najszybszy sposób na zatrzymanie wątków w pętli while(true))
    exit(0);
}
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <random>
#include <memory>   // for std::unique_ptr

class Account {
public:
    Account(int id, double initial_balance)
        : accountId(id), balance(initial_balance) {}

    void deposit(double amount) {
        std::lock_guard<std::mutex> lock(mtx);
        balance += amount;
    }

    bool withdraw(double amount) {
        std::lock_guard<std::mutex> lock(mtx);
        if (balance >= amount) {
            balance -= amount;
            return true;
        }
        return false;
    }

    double getBalance() const {
        std::lock_guard<std::mutex> lock(mtx);
        return balance;
    }

    int getId() const { return accountId; }

    // Deadlock‑free transfer using std::lock
    static bool transfer(Account& from, Account& to, double amount) {
        if (&from == &to) return false;

        std::unique_lock<std::mutex> lock1(from.mtx, std::defer_lock);
        std::unique_lock<std::mutex> lock2(to.mtx, std::defer_lock);
        std::lock(lock1, lock2);

        if (from.balance >= amount) {
            from.balance -= amount;
            to.balance += amount;
            return true;
        }
        return false;
    }

private:
    int accountId;
    double balance;
    mutable std::mutex mtx;
};

int main() {
    const int NUM_ACCOUNTS = 5;
    const int NUM_THREADS = 5;
    const int OPERATIONS_PER_THREAD = 1000;

    // Store accounts as unique_ptr to avoid moving the Account objects.
    std::vector<std::unique_ptr<Account>> accounts;
    accounts.reserve(NUM_ACCOUNTS);
    for (int i = 0; i < NUM_ACCOUNTS; ++i) {
        accounts.push_back(std::unique_ptr<Account>(new Account(i, 1000.0)));
    }

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    // Launch worker threads
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&accounts]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> accountDist(0, accounts.size() - 1);
            std::uniform_int_distribution<int> opDist(0, 2);
            std::uniform_real_distribution<double> amountDist(1.0, 100.0);

            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                int op = opDist(gen);
                if (op == 0) {   // deposit
                    int idx = accountDist(gen);
                    double amount = amountDist(gen);
                    accounts[idx]->deposit(amount);
                } else if (op == 1) {   // withdraw
                    int idx = accountDist(gen);
                    double amount = amountDist(gen);
                    accounts[idx]->withdraw(amount);   // ignore failure
                } else {   // transfer
                    int fromIdx = accountDist(gen);
                    int toIdx = accountDist(gen);
                    if (fromIdx != toIdx) {
                        double amount = amountDist(gen);
                        Account::transfer(*accounts[fromIdx], *accounts[toIdx], amount);
                    }
                }
            }
        });
    }

    // Wait for all threads to finish
    for (auto& th : threads) {
        th.join();
    }

    // Print final balances and total (should remain constant)
    double total = 0.0;
    std::cout << "Final balances:\n";
    for (const auto& accPtr : accounts) {
        double bal = accPtr->getBalance();
        std::cout << "  Account " << accPtr->getId() << ": " << bal << "\n";
        total += bal;
    }
    std::cout << "Total balance: " << total << "\n";

    return 0;
}
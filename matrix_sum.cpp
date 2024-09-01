#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <future>
#include <random>

#include <algorithm>
#include <parlay/primitives.h>

using namespace std;

int calculate_sum(const vector<vector<int>>& matrix) {
    int sum = 0;
    for (int i = 0; i < matrix.size(); i++) {
        for (int j = 0; j < matrix[i].size(); j++) {
            sum += matrix[i][j];
        }
    }
    return sum;
}

int calculate_row_sum(const vector<vector<int>>& matrix, int start_row, int end_row){
    int sum = 0;
    for (int i = start_row; i < end_row; i++) {
            for (int j = 0; j < matrix[i].size(); j++) {
                    sum += matrix[i][j];
            }
    }

    return sum;
}

int parallel_matrix_sum(const vector<vector<int>>& matrix, int num_threads) {
    int rows = matrix.size();

    int rows_per_thread = rows / num_threads;
    int extra_rows = rows % num_threads;

    vector<future<int>> futures;
    int total_sum = 0;

    for (int i = 0; i < num_threads; ++i) {
        int start_row = i * rows_per_thread;
        int end_row = start_row + rows_per_thread;
        if (i == num_threads - 1) {
            end_row += extra_rows;
        }

        futures.push_back(async(launch::async, calculate_row_sum,
                    ref(matrix), start_row, end_row));
    }

    for (auto& future : futures) {
        int partial_sum = future.get();
        total_sum += partial_sum;
    }

    return total_sum;
}

int parlay_scan_matrix_sum(const vector<vector<int>>& matrix) {
    parlay::sequence row_sums(matrix.size(), 0);

    parlay::parallel_for(0, matrix.size(), [&] (int i) {
        row_sums[i] = parlay::scan(matrix[i], parlay::plus<int>()).second;
    });

    int sum = parlay::scan(row_sums, parlay::plus<int>()).second;
    return sum;
}

int main() {
    // Example matrix

    int n = 1000000;
    int x = 1000;

    vector<vector<int>> matrix(n, vector<int>(1));
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(1, 2);

    parlay::parallel_for (0, n, [&](int ii) {
            for (int j = 0; j < x; ++j) {
                    if (j == 0 && ii < 3*n/4)
                        matrix[ii][j] = 20;
                    if (j == 0 && ii >= 3*n/4)
                        matrix[ii][j] = x;

                    if (j < matrix[ii][0] && j != 0)
                        matrix[ii].push_back(distrib(gen));
            }
    });
    std::cout << "finished initializing" << std::endl;

    int num_threads = 10; // Adjust the number of threads as needed

    parlay::internal::timer t("Time");
    t.next("initialize timer");
    int seq_sum = calculate_sum(matrix);
    t.next("sequential sum");
    int total_sum = parallel_matrix_sum(matrix, num_threads);
    t.next("thread sum");
    int total_parlay_sum = parlay_scan_matrix_sum(matrix);
    t.next("parlay scan");

    cout << "sequential sum: " << seq_sum << endl;
    cout << "Total thread sum: " << total_sum << endl;
    cout << "Total parlay scan sum: " << total_parlay_sum << endl;

    return 0;
}

#include <iostream>
#include <vector>
#include <cmath>

using namespace std;

vector<vector<int>> generateBayerMatrix(int n) {
    if ((n & (n - 1)) != 0 || n <= 0) {
        throw invalid_argument("Size must be a power of 2.");
    }

    vector<vector<int>> matrix(n, vector<int>(n));

    matrix[0][0] = 0;
    matrix[0][1] = 2;
    matrix[1][0] = 3;
    matrix[1][1] = 1;

    for (int size = 2; size < n; size *= 2) {
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                matrix[i][j + size] = matrix[i][j] + 2 * size * size;
                matrix[i + size][j] = matrix[i][j] + 3 * size * size;
                matrix[i + size][j + size] = matrix[i][j] + size * size;
            }
        }
    }

    return matrix;
}

void printMatrix(const vector<vector<int>>& matrix) {
    for (const auto& row : matrix) {
        for (const auto& value : row) {
            cout << value << " ";
        }
        cout << endl;
    }
}

int main() {
    try {
        int size;
        cout << "Enter the size of the Bayer matrix (power of 2): ";
        cin >> size;

        vector<vector<int>> bayerMatrix = generateBayerMatrix(size);

        cout << "Bayer Matrix of size " << size << "x" << size << ":" << endl;
        printMatrix(bayerMatrix);
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}

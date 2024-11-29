#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include <sstream>
#include <algorithm>
#include <cctype>

using namespace std;

typedef struct {
    std::pair<string, int> file;
    pthread_barrier_t* barrier;
    vector<map<string, int>> *allWords;
    int id;
} thread_data_t;

// From Geeks for geeks
vector<string> split_sentence(string sen) {
    stringstream ss(sen);
    string word;
    vector<string> words;
    while (ss >> word) {
        words.push_back(word);
    }
    return words;
}

void *Mapper(void *arg) {
    thread_data_t* data = (thread_data_t*)arg;
    map<string, int> result;
    const auto& [filename, id] = data->file;
    // printf("Thread processing file: %s with ID: %d, threadID:%d\n", filename.c_str(), id, data->id);

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        pthread_exit(NULL);
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string file_content = buffer.str();
    transform(file_content.begin(), file_content.end(), file_content.begin(), [](unsigned char c) { return tolower(c); });
    file_content.erase(remove(file_content.begin(), file_content.end(), '\''), file_content.end());
    file_content.erase(remove(file_content.begin(), file_content.end(), ','), file_content.end());
    file_content.erase(remove(file_content.begin(), file_content.end(), '.'), file_content.end());

    vector<string> words = split_sentence(file_content);
    map<string, int> mapping;
    for (const auto& word : words) {
        mapping.insert(make_pair(word, id));
        cout << word;
    }

    // Protecting shared data (allWords) with mutex or managing each thread's output separately
    data->allWords->push_back(mapping);
    file.close();

    int r = pthread_barrier_wait(data->barrier);
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    const int M = stoi(argv[1]);
    const int R = stoi(argv[2]);
    const string entry_file = argv[3];

    map<string, int> inputFiles;

    ifstream entry_data(entry_file);
    int numberOfFiles;
    if (entry_data.is_open()) {
        string line;
        if (getline(entry_data, line)) {
            numberOfFiles = stoi(line);
        }

        for (size_t id = 1; id <= numberOfFiles; ++id) {
            if (getline(entry_data, line)) {
                inputFiles.insert(make_pair(line, id));
            }
        }
        entry_data.close();
    }


    pthread_t threads[M];
    int r;

    // Initialize allWords as an empty vector of maps
    vector<map<string, int>> allWords;

    // Initialize barrier
    pthread_barrier_t barrier;
    r = pthread_barrier_init(&barrier, NULL, M);

    vector<thread_data_t> thread_data_array(M);
    auto it = inputFiles.begin();
    for (size_t i = 0; i < M; ++i) {
        if (it == inputFiles.end()) {
            break; // Avoid dereferencing past the end of the map

        }
        thread_data_array[i].file = *it++;
        thread_data_array[i].barrier = &barrier;
        thread_data_array[i].id = i;
        thread_data_array[i].allWords = &allWords;

        r = pthread_create(&threads[i], NULL, Mapper, &thread_data_array[i]);

        if (r) {
            printf("Error creating thread %zu\n", i);
            exit(-1);
        }
    }

    void* status;
    for (size_t i = 0; i < M; i++) {
        r = pthread_join(threads[i], &status);
        if (r) {
            printf("Error joining thread %zu\n", i);
            exit(-1);
        }
    }

    r = pthread_barrier_destroy(&barrier);
    return 0;
}

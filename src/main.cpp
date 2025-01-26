#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <bits/stdc++.h>

using namespace std;

// Thread structure
typedef struct {
    vector<pair<string, int>> inputFiles;
    vector<map<string, int>> *allWords;
    map<string, set<int>> *result;
    pthread_barrier_t* barrier;
    pthread_mutex_t* mutex;
    int id;
    int M, R;
} thread_data_t;

/*
    Splits the sentence into words
*/
void split_sentence(string sen, map<string, int> *list, int id) {
    stringstream ss(sen);
    string word;
    while (ss >> word) {
        (*list)[word] = id;
    }
}

/*
    Compare function for words sorting
*/
bool cmp(pair<string, set<int>>& a, pair<string, set<int>>& b) { 
    if (a.second.size() != b.second.size()) {
        return a.second.size() > b.second.size();
    }
    return a.first < b.first;
}

/*
    Mapper function
*/
void mapper(thread_data_t* data) {
    if (data->id < data->M) {
        for (size_t i = data->id; i < data->inputFiles.size(); i += data->M) {
            const auto& [filename, id] = data->inputFiles[i];
            ifstream file(filename);
            if (!file.is_open()) {
                cerr << "Error opening file: " << filename << endl;
                pthread_exit(NULL);
            }

            // Reading entire file
            stringstream buffer;
            buffer << file.rdbuf();
            string file_content = buffer.str();

            // Erasing unuseful letters
            transform(file_content.begin(), file_content.end(), file_content.begin(), [](unsigned char c) { return tolower(c); });
            file_content.erase(
                std::remove_if(file_content.begin(), file_content.end(), 
                            [](unsigned char c) { return !std::isalpha(c) && c != ' ' && c != '\n'; }),
                file_content.end()
            );
            
            // Spliting sentence in words
            map<string, int> list;
            split_sentence(file_content, &list, id);

            pthread_mutex_lock(data->mutex);
            data->allWords->push_back(list);
            pthread_mutex_unlock(data->mutex);
            file.close();
        }
    }
}

/*
    Reducer function
*/
void reducer(thread_data_t* data) {
    // Aggregation
    if (data->id >= data->M) {
        int start = (data->id - data->M);
        pthread_mutex_lock(data->mutex);
        for (size_t i = start; i < data->allWords->size(); i += data->R) {
            const auto& list = (*data->allWords)[i];
            for ( auto [word, id] : list) {
                (*data->result)[word].insert(id);
            }
        }
        pthread_mutex_unlock(data->mutex);
    }

    // Waiting for all reducer threads to finish aggregation
    int r = pthread_barrier_wait(data->barrier);

    // Processing words
    if (data->id >= data->M) {
        char start = 'a' + (data->id - data->M);
        for (int ch = start; ch <= 'z'; ch += data->R) {
            char current_char = static_cast<char>(ch);
            string out_name(1, current_char);
            ofstream out(out_name + ".txt");

            // Taking all words that start with letter 'ch'
            vector<pair<string, set<int>>> words;
            for (auto& [word, ids] : *data->result) {
                if (word[0] == ch) {
                    words.emplace_back(word, ids);
                }
            }

            // Sorting words
            sort(words.begin(), words.end(), cmp);

            // Output
            for (auto& [word, ids] : words) {
                out << word << ":[";

                auto it = ids.begin();
                for (; it != prev(ids.end()); ++it) {
                    out << *it << " ";
                }

                out << *it << "]" << endl;
            }

            out.close();
        }
    }
}
void *thread_function(void *arg) {
    thread_data_t* data = (thread_data_t*)arg;

    // Calling mapper
    mapper(data);

    // Barrier waiting for mapper threads
    int r = pthread_barrier_wait(data->barrier);

    // Calling reducer
    reducer(data);

    pthread_exit(NULL);
}


int main(int argc, char **argv)
{
    int M = stoi(argv[1]);
    int R = stoi(argv[2]);
    const string entry_file = argv[3];

    vector<pair<string, int>> inputFiles;

    // Reading input files name
    ifstream entry_data(entry_file);
    int numberOfFiles;
    if (entry_data.is_open()) {
        string line;
        if (getline(entry_data, line)) {
            numberOfFiles = stoi(line);
        }

        for (size_t id = 1; id <= numberOfFiles; ++id) {
            if (getline(entry_data, line)) {
                inputFiles.emplace_back(make_pair(line, id));
            }
        }
        entry_data.close();
    }

    // Threads
    pthread_t threads[M + R];
    int r;

    vector<map<string, int>> allWords;
    map<string, set<int>> result;

    // Initializing barrier
    pthread_barrier_t barrier;
    r = pthread_barrier_init(&barrier, NULL, M + R);

    // Initializing mutex
    pthread_mutex_t mutex;
    r = pthread_mutex_init(&mutex, NULL);

    vector<thread_data_t> thread_data_array(M + R);

    for (size_t i = 0; i < M + R; ++i) {
        thread_data_array[i].inputFiles = inputFiles;
        thread_data_array[i].barrier = &barrier;
        thread_data_array[i].mutex = &mutex;
        thread_data_array[i].id = i;
        thread_data_array[i].allWords = &allWords;
        thread_data_array[i].M = M;
        thread_data_array[i].R = R;
        thread_data_array[i].result = &result;

        r = pthread_create(&threads[i], NULL, thread_function, &thread_data_array[i]);

        if (r) {
            printf("Error creating thread %zu\n", i);
            exit(-1);
        }
    }

    void* status;
    for (size_t i = 0; i < M + R ; i++) {
        r = pthread_join(threads[i], &status);
        if (r) {
            printf("Error joining thread %zu\n", i);
            exit(-1);
        }
    }

    r = pthread_mutex_destroy(&mutex);
    r = pthread_barrier_destroy(&barrier);
    return 0;
}

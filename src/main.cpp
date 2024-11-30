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
    pthread_mutex_t* mutex;
    vector<map<string, int>> *allWords;
    map<string, vector<int>> *result;
    int id;
    int M, R;
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

bool cmp(pair<string, vector<int>>& a, pair<string, vector<int>>& b) { 
    if (a.second.size() != b.second.size()) {
        return a.second.size() > b.second.size();
    } 
    return a.first < b.first;
}
void *Mapper(void *arg) {
    thread_data_t* data = (thread_data_t*)arg;
    const auto& [filename, id] = data->file;
    // printf("Thread processing file: %s with ID: %d, threadID:%d\n", filename.c_str(), id, data->id);
    if (data->id < data->M) {
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
        }

        // Protecting shared data (allWords) with mutex or managing each thread's output separately
        pthread_mutex_lock(data->mutex);
        data->allWords->push_back(mapping);
        pthread_mutex_unlock(data->mutex);
        file.close();
    }

    int r = pthread_barrier_wait(data->barrier);

    if (data->id >= data->M) {
        int wordsCount = data->allWords->size() / data->R;
        int start = (data->id - data->M) * wordsCount;
        int end = start + wordsCount;
        if (end > data->allWords->size()) {
            end = data->allWords->size();
        }
        pthread_mutex_lock(data->mutex);
        for (size_t i = start; i < end; i++) {
            auto& words = (*data->allWords)[i];

            for (auto& [word, id] : words) {
                (*data->result)[word].push_back(id);
                stable_sort((*data->result)[word].begin(), (*data->result)[word].end());
            }
        }
        pthread_mutex_unlock(data->mutex);

    }
    
    r = pthread_barrier_wait(data->barrier);

    if (data->id >= data->M) {
        char start = 'a' + (data->id - data->M);
        for (char ch = start; ch <= 'z'; ch += data->M) {
            string out_name;
            out_name[0] = ch;
            ofstream out (out_name);

            vector<pair<string, vector<int>>> words;
            for (auto& [word, ids] : *data->result) {
                if (word[0] == ch) {
                    words.emplace_back(word, ids);
                }
            }
            sort(words.begin(), words.end(), cmp);
            for (auto& [word, ids] : words) {
                out << word << ":[";
                size_t i = 0;
                for (; i < ids.size() - 1; ++i) {
                    out << ids[i] << " ";
                }
                out << ids[i] << "]" << endl;
            }

            out.close();
        }

    }
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    int M = stoi(argv[1]);
    int R = stoi(argv[2]);
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
    
    map<string, vector<int>> result;

    // Initialize barrier
    pthread_barrier_t barrier;
    r = pthread_barrier_init(&barrier, NULL, M + R);

    pthread_mutex_t mutex;
    r = pthread_mutex_init(&mutex, NULL);

    vector<thread_data_t> thread_data_array(M + R);
    auto it = inputFiles.begin();

    for (size_t i = 0; i < M + R; ++i) {
        if (i < M) {
            if (it == inputFiles.end()) {
                M = i;
            } else {
                thread_data_array[i].file = *it++;
            }
        }
        thread_data_array[i].barrier = &barrier;
        thread_data_array[i].mutex = &mutex;
        thread_data_array[i].id = i;
        thread_data_array[i].allWords = &allWords;
        thread_data_array[i].M = M;
        thread_data_array[i].R = R;
        thread_data_array[i].result = &result;

        r = pthread_create(&threads[i], NULL, Mapper, &thread_data_array[i]);

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
    // for (auto& [word, id] : result) {
    //     cout << word << " ";
    //     for(auto i : id) {
    //         cout << i << " ";
    //     }
    //     cout << endl;
    // }
    r = pthread_barrier_destroy(&barrier);
    return 0;
}

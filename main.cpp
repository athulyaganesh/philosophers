#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>
typedef std::chrono::high_resolution_clock Clock;

struct Fork {
    std::mutex lock;
    std::condition_variable condition_req_fork;
    std::condition_variable condition_fork;
    volatile bool hold;
    volatile bool reqf;
    volatile bool dirty = true;
};


struct Bottle {
    std::mutex lock;
    std::condition_variable condition{};
    volatile bool hold;
    volatile bool reqb;
};

struct Resource {
    Fork fork;
    Bottle bottle;
};

enum class Dine {
    THINKING = 1, HUNGRY, EATING
};

enum class Drink {
    TRANQUIL = 1, THIRSTY, DRINKING
};


void *philosopher(void *pid);

void tranquil(long id);

void drink(long id);

void send_fork_request(long from, long to);

void send_fork(long from, long to);

void send_bottle_request(long from, long to);

void send_bottle(long from, long to);



int parser(int argc, char **argv);

std::vector<std::vector<std::pair<int, Resource*>>> graph_initialize(int mode);


constexpr long TRANQUIL_MIN = 1, TRANQUIL_MAX = 1000;  
constexpr long DRINKING_MIN = 1, DRINKING_MAX = 1000; 
constexpr long TRANQUIL_RANGE = TRANQUIL_MAX - TRANQUIL_MIN;
constexpr long DRINKING_RANGE = DRINKING_MAX - DRINKING_MIN;

bool debug = false;
int p_cnt;
int count_session = 20;
std::string path;
std::atomic_bool start;
std::vector<Dine> dineState;
std::vector<Drink> drinkState;
std::vector<std::vector<std::pair<int, Resource*>>> graph;
std::vector<unsigned int> rand_seeds;
std::mutex print_lock;


int parser(int argc, char **argv) {
    int opt;
    static struct option opts[] = {
            {"session",  required_argument, nullptr, 's'},
            {"filename", required_argument, nullptr, 'f'},
            {nullptr,    no_argument,       nullptr, 0},
    };
    while ((opt = getopt_long(argc, argv, ":s:f:-d", opts, nullptr)) != EOF) {
        switch (opt) {
            case 's':
                count_session = static_cast<int>(std::strtol(optarg, nullptr, 10));
                break;
            case 'f':
                path = optarg;
                break;
            case 'd':
                debug = true;
                break;
            case ':':
                std::cerr << "INVALID : needs value" << opt << std::endl;
                break;
            case '?':
                std::cout << "USAGE: philosophers -s <session_count> -f <filename> [-]" << std::endl;
                exit(-1);
            default:
                break;
        }
    }
    if (debug) {
        std::cout << "SESSIONS COUNT:   " << (count_session = count_session < 1 ? 20 : count_session) << std::endl;
    }
    for (; optind < argc; optind++) {
        if (!strcmp(argv[optind], "-")) {
            return 2;
        }
    }
    if (path.length()) {
        if (debug) {
            std::cout << " PATH: " << path << std::endl;
        }
        return 1;
    }
    return 0;
}

std::vector<std::vector<std::pair<int, Resource*>>> graph_initialize(int mode) {
    if (mode == 1) {
        std::ifstream file(path);
        if (file.good()) {
            int p1, p2, n = 0;
            file >> p_cnt;
            std::vector<std::vector<std::pair<int, Resource*>>> graph(static_cast<unsigned long>(p_cnt));
            for (int i = 0; i < p_cnt; i++) {
                file >> p1 >> p2;
                if (p1 < 1 || p2 < 1 || p1 > p_cnt || p2 > p_cnt) {
                    std::cerr << "ERROR: invalid graph" << std::endl;
                    exit(-1);
                }
                Resource *pos = new Resource, *neg = new Resource;
                pos->fork.hold = pos->bottle.hold = true;
                neg->fork.reqf = neg->bottle.reqb = true;
                graph[p1 - 1].push_back(std::make_pair(p2 - 1, p1 < p2 ? pos : neg));
                graph[p2 - 1].push_back(std::make_pair(p1 - 1, p1 < p2 ? neg : pos));
                n++;
            }
            if (n < p_cnt - 1 || n > (p_cnt * (p_cnt - 1) / 2)) {
                std::cerr << "ERROR: invalid graph" << std::endl;
                exit(-1);
            }
            return graph;
        } else {
            std::cerr << "ERROR: file '" << path << "' not found" << std::endl;
            exit(-1);
        }

    } else if (mode == 2) {
        int p1, p2, n = 0;
        std::cout << "NUM PHILOSOPHERS: ";
        std::cin >> p_cnt;
        std::cout << "EDGE PAIRS (0 to exit):" << std::endl;
        std::vector<std::vector<std::pair<int, Resource*>>> graph(static_cast<unsigned long>(p_cnt));
        while (true) {
            std::cin >> p1 >> p2;
            if (p1 < 1 || p2 < 1 || p1 > p_cnt || p2 > p_cnt) break;
            Resource *pos = new Resource(), *neg = new Resource();
            pos->fork.hold = pos->bottle.hold = true;
            neg->fork.reqf = neg->bottle.reqb = true;
            graph[p1 - 1].push_back(std::make_pair(p2 - 1, p1 < p2 ? pos : neg));
            graph[p2 - 1].push_back(std::make_pair(p1 - 1, p1 < p2 ? neg : pos));
            n++;
        }
        if (n < p_cnt - 1 || n > (p_cnt * (p_cnt - 1) / 2)) {
            std::cerr << "ERROR: invalid graph" << std::endl;
            exit(-1);
        }
        return graph;
    }

    p_cnt = 5; //num philosophers 
    Resource *r1a = new Resource, *r2a = new Resource, *r3a = new Resource, *r4a = new Resource, *r5a = new Resource;
    Resource *r1b = new Resource, *r2b = new Resource, *r3b = new Resource, *r4b = new Resource, *r5b = new Resource;
    r1a->fork.hold = r2a->fork.hold = r3a->fork.hold = r4a->fork.hold = r5a->fork.hold = true;
    r1b->fork.reqf = r2b->fork.reqf = r3b->fork.reqf = r4b->fork.reqf = r5b->fork.reqf = true;
    r1a->bottle.hold = r2a->bottle.hold = r3a->bottle.hold = r4a->bottle.hold = r5a->bottle.hold = true;
    r1b->bottle.reqb = r2b->bottle.reqb = r3b->bottle.reqb = r4b->bottle.reqb = r5b->bottle.reqb = true;
    return {{std::make_pair(1, r1a), std::make_pair(4, r5a)},
            {std::make_pair(2, r2a), std::make_pair(0, r1b)},
            {std::make_pair(3, r3a), std::make_pair(1, r2b)},
            {std::make_pair(4, r4a), std::make_pair(2, r3b)},
            {std::make_pair(0, r5b), std::make_pair(3, r4b)}};
}

void *philosopher(void *pid) {
    while (!start.load());
    long id = (long) pid;
    for (int session = 0; session < count_session;) {
        std::vector<std::pair<int, Resource*>> refs = graph[id];

        switch (drinkState[id]) {
            case Drink::TRANQUIL:
                for (std::pair<int, Resource*> ref_pair : refs) {
                    Resource *resource = ref_pair.second;
                    resource->bottle.lock.lock();
                    if (resource->bottle.hold && resource->bottle.reqb && !resource->fork.hold) {
                        send_bottle(id, ref_pair.first);
                        resource->bottle.hold = false;
                    }
                    resource->bottle.lock.unlock();
                }
                tranquil(id);
                drinkState[id] = Drink::THIRSTY;
                break;

            case Drink::THIRSTY:
                
                for (std::pair<int, Resource*> ref_pair : refs) {
                    Resource *resource = ref_pair.second;
                    resource->bottle.lock.lock();
                    if (resource->bottle.hold && resource->bottle.reqb && !resource->fork.hold) {
                        send_bottle(id, ref_pair.first);
                        resource->bottle.hold = false;
                    }
                    if (!resource->bottle.hold) {
                        while (!resource->bottle.reqb) {
                            std::unique_lock<std::mutex> lk(resource->bottle.lock);
                            resource->bottle.condition.wait(lk);
                        }
                        send_bottle_request(id, ref_pair.first);
                        resource->bottle.reqb = false;
                    }
                    resource->bottle.lock.unlock();
                }
                drinkState[id] = Drink::DRINKING;
                break;

            case Drink::DRINKING:
                drink(id);
                print_lock.lock();
                std::cout << "philosopher " << id + 1 << " drinking" << std::endl;
                print_lock.unlock();
                drinkState[id] = Drink::TRANQUIL;
                session++;
                if (session == count_session) {
                    dineState[id] = Dine::THINKING;
                }
                break;
        }

        switch (dineState[id]) {
            case Dine::THINKING: 
                 print_lock.lock(); 
                std::cout << "Thinking time for philosopher " << id + 1 << std::endl;
                print_lock.unlock();

                for (std::pair<int, Resource*> ref_pair : refs) {
                    auto to = std::find_if(graph[id].begin(), graph[id].end(), [ref_pair](std::pair<int, Resource*> pair) -> bool {return ref_pair.first == pair.first;});
                    to->second->fork.lock.lock();
                    if (session == count_session || (to->second->fork.hold && to->second->fork.dirty && to->second->fork.reqf)) {
                        send_fork(id, ref_pair.first);
                        to->second->fork.hold = false;
                        to->second->fork.dirty = false;
                    }
                    to->second->fork.lock.unlock();
                }
                if (session == count_session) {
                    break;
                }

                // (D1) A thinking, thirsty philosopher becomes hungry
                if (drinkState[id] == Drink::THIRSTY) {
                    dineState[id] = Dine::HUNGRY;
                }
                break;

            case Dine::HUNGRY:
                for (std::pair<int, Resource*> ref_pair : refs) {
                    auto to = std::find_if(graph[id].begin(), graph[id].end(), [ref_pair](std::pair<int, Resource*> pair) -> bool {return ref_pair.first == pair.first;});
                    if (to->second->fork.hold && to->second->fork.dirty && to->second->fork.reqf) {
                        send_fork(id, ref_pair.first);
                        to->second->fork.hold  = false;
                        to->second->fork.dirty = false;
                    }
                    if (!to->second->fork.hold) {
                        while (!to->second->fork.reqf) {
                            std::unique_lock<std::mutex> lk(to->second->fork.lock);
                            to->second->fork.condition_req_fork.wait(lk);
                        }
                        send_fork_request(id, ref_pair.first);
                        to->second->fork.reqf = false;
                    }
                }

                for (std::pair<int, Resource*> ref_pair : refs) {
                    auto to = std::find_if(graph[id].begin(), graph[id].end(), [ref_pair](std::pair<int, Resource*> pair) -> bool {return ref_pair.first == pair.first;});
                    if (!to->second->fork.hold && !to->second->fork.reqf) {
                        std::unique_lock<std::mutex> lk(to->second->fork.lock);
                        to->second->fork.condition_fork.wait(lk);
                    }
                }

                dineState[id] = Dine::EATING;
                break;

            case Dine::EATING:
                for (std::pair<int, Resource*> ref_pair : refs) {
                    auto to = std::find_if(graph[id].begin(), graph[id].end(), [ref_pair](std::pair<int, Resource*> pair) -> bool {return ref_pair.first == pair.first;});
                    to->second->fork.dirty = true; 
                }
             
                if (drinkState[id] != Drink::THIRSTY) {
                    dineState[id] = Dine::THINKING;
                }
                break;
        }
    }
    return nullptr;
}

void send_fork_request(long from, long to) {
    auto it = std::find_if(graph[to].begin(), graph[to].end(), [from](std::pair<int, Resource*> ref_pair) -> bool {
        return from == ref_pair.first;
    });
    if (it == graph[to].end()) {
        std::cerr << "WARN: reverse edge for <" << from << "> not found" << std::endl;
        return;
    }
    it->second->fork.reqf = true;
    it->second->fork.condition_req_fork.notify_one();
}

void send_fork(long from, long to) {
    auto it = std::find_if(graph[to].begin(), graph[to].end(), [from](std::pair<int, Resource*> ref_pair) -> bool {
        return from == ref_pair.first;
    });
    if (it == graph[to].end()) {
        std::cerr << "WARN: reverse edge for <" << from << "> not found" << std::endl;
        return;
    }
    it->second->fork.dirty = false;
    it->second->fork.hold = true;
    it->second->fork.condition_fork.notify_one();
}

void send_bottle_request(long from, long to) {
    auto it = std::find_if(graph[to].begin(), graph[to].end(), [from](std::pair<int, Resource*> ref_pair) -> bool {
        return from == ref_pair.first;
    });
    if (it == graph[to].end()) {
        std::cerr << "WARN: reverse edge for <" << from << "> not found" << std::endl;
        return;
    }
    it->second->bottle.lock.lock();
    it->second->bottle.reqb = true;
    it->second->bottle.condition.notify_one();
    it->second->bottle.lock.unlock();
}

void send_bottle(long from, long to) {
    auto it = std::find_if(graph[to].begin(), graph[to].end(), [from](std::pair<int, Resource*> ref_pair) -> bool {
        return from == ref_pair.first;
    });
    if (it == graph[to].end()) {
        std::cerr << "WARN: reverse edge for <" << from << "> not found" << std::endl;
        return;
    }
    it->second->bottle.lock.lock();
    it->second->bottle.hold = true;
    it->second->bottle.lock.unlock();
}

void tranquil(long id) {
    usleep(static_cast<useconds_t>(TRANQUIL_MIN + rand_r(&rand_seeds[id]) % TRANQUIL_RANGE));
}

void drink(long id) {
    usleep(static_cast<useconds_t>(DRINKING_MIN + rand_r(&rand_seeds[id]) % DRINKING_RANGE));
}


int main(int argc, char **argv) {
    graph = graph_initialize(parser(argc, argv));
    if (debug) {
        std::cout << "press any key to continue." << std::endl;
        getchar();

        std::cout << "graph initialization:" << std::endl;
        for (int i = 0; i < graph.size(); i++) {
            std::cout << i << ": ";
            for (const auto &adjacent : graph[i]) {
                std::cout << adjacent.first << " (" << adjacent.second << ") ";
            }
            std::cout << std::endl;
        }
        printf("CONFIG: %d philosophers DRINK  %d times.\n\n", p_cnt, count_session);
    }

    dineState.resize(static_cast<unsigned long>(p_cnt), Dine::THINKING);
    drinkState.resize(static_cast<unsigned long>(p_cnt), Drink::TRANQUIL);

    pthread_t threads[p_cnt];
    rand_seeds.resize(static_cast<unsigned long>(p_cnt));
    srand(static_cast<unsigned int>(time(nullptr)));
    for (long i = 0; i < p_cnt; i++) {
        pthread_create(&threads[i], nullptr, philosopher, (void *) i);
        rand_seeds[i] = static_cast<unsigned int>(rand());
    }
    start = true;
    for (int i = 0; i < p_cnt; i++) {
        pthread_join(threads[i], nullptr);
    }
    return 0;
}


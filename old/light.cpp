#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>

#include <libsumo/libtraci.h>

#include "Connection.h"

using namespace libtraci;

// Fonctions utilitaires
int countVehiclesOnEdge(const std::string& edgeID) {
    return Edge::getLastStepVehicleIDs(edgeID).size();
}

// Données partagées
std::vector<std::string> EdgesIDs = {"-E3", "-E4", "E2"};
std::unordered_map<std::string, int> vehicleCountMap;
std::mutex mutex;

// Thread de surveillance du trafic
void* analyzeTraffic(void* arg) {
    Connection::connect("localhost", 8888, 10, "conn", nullptr);
    Connection::switchCon("conn");
    libtraci::Connection& conn = libtraci::Connection::getActive();
    conn.setOrder(2);

    using clock = std::chrono::steady_clock;
    auto nextTick = clock::now();

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& edge : EdgesIDs) {
                int count = countVehiclesOnEdge(edge);
                vehicleCountMap[edge] = count;
            }
        }

        nextTick += std::chrono::seconds(1);
        while (clock::now() < nextTick) {
            Simulation::step();
        }
    }

    return nullptr;
}

//thread pour dire a la simulation qu'on peut continuer
void* continueSimulation(void* arg) {
    while (true) {
        if (mutex.try_lock()) {
        
        }
        Simulation::step();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}


int main(int argc, char* argv[]) {

    const std::string trafficLightID = "J4";

    pthread_t trafficThread;
    pthread_create(&trafficThread, nullptr, analyzeTraffic, nullptr);

    pthread_join(trafficThread, nullptr);

    return 0;
}
